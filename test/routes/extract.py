#!/usr/bin/env python3
"""Extracts the conversion ("route") behavior of the current implementation.

For every combination of domain, leaf kind, scope, stored shape, and request the
extractor records what the library does: whether the registration compiles,
whether the resolution compiles, throws, or succeeds, and for a successful
resolution the constructions it performed and the identity of the leaf it
handed out. The table is ground truth for a reference implementation of the
route rules; it describes the code as it is, not as it is documented.

Usage:

    extract.py --work-dir build/routes [--jobs N] [--cxx-command CMD]
               [--domains runtime,static,mixed] [--output FILE]

    CXX=g++ extract.py --work-dir build/routes
    extract.py --work-dir build/routes --cxx-command "clang++ -std=c++20 \\
        -I include" --domains runtime

The default output is test/routes/observed/<toolchain>.jsonl, where the toolchain
key is the one test/contracts/probe.py uses, such as gcc16-x86_64-c++20. The
file is JSON Lines sorted by case id; every line is one case.

Universe (the tables below):

- domains: "runtime" is dingo::container<> with register_type, "static" is
  dingo::static_container over a bindings list, "mixed" is dingo::container
  over a bindings list (static binding, resolved through the container).
- leaf kinds: "leaf" (copyable, movable) and "move_only_leaf"; both count
  default, copy, and move constructions and destructions and register every
  live instance address with a creation sequence number.
- scopes: shared, unique, external, shared_cyclical. External registrations
  pass an instance; the static and mixed containers cannot take one, which
  shows as a registration compile error.
- stored shapes: T, T*, T&, unique_ptr/shared_ptr of T and T[], optional<T>,
  T[2], variant<T, other_leaf>, and the nine {unique_ptr, shared_ptr, optional}
  of {unique_ptr<T>, shared_ptr<T>, optional<T>} combinations.
- requests: the carriers value, &, const &, &&, *, const * applied to T,
  unique_ptr<T>, shared_ptr<T>, optional<T>, the stored type, and every
  wrapper layer of the stored type, deduplicated. The spelling in the case id
  is the one written in the generated C++ with single spaces: "leaf&",
  "const leaf&", "leaf const*" only after a pointer ("leaf* const&"),
  "leaf(&)[2]" for arrays.

A case id is "<domain>/<leaf>/<scope>/<stored>/<request>". A record has the
keys

  counts         {"copy", "default", "destroy", "move"} deltas during the first
                 resolve of an "ok" case, else null
  domain, leaf, scope, stored, request, id
  identity       "existing" (the leaf was live before the call), "new" (created
                 during the call), "none" (empty handle), "unknown" (the
                 address is not live when the call returns: a dangling
                 result), or null for a plain leaf value and for a case that
                 is not "ok"
  registration   "ok", "compile_error", or "throws:<exception>"
  resolve        "ok", "compile_error", "throws:<exception>", "crash:<signal>",
                 or "not_run" when the registration is not "ok"
  stable         whether a second resolve reaches the same leaf address (false
                 for a plain leaf value, null when not "ok")
  warm_counts, warm_identity
                 the same observations for that second resolve, made while the
                 result of the first is still alive

Every case uses a fresh container: the first resolve is a cold resolve.
<exception> is the most specific dingo exception type caught (they are caught
in the order of include/dingo/base/exceptions.h; "exception" is the common
base), else "std" or "unknown".

Method: per (domain, leaf, scope, stored) one generated program contains one
function per request. A registration-only compile decides the registration
outcome; the requests that compile (all at once, else one -fsyntax-only compile
per request, which is the only way to observe a static compile error) are then
built into one executable per group that runs each request on a fresh container
and prints one JSON line. A crash is attributed to the request being run and
the remaining requests continue.
"""

import argparse
import concurrent.futures
import itertools
import json
import os
import platform
import re
import shlex
import signal
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
OBSERVED_DIR = Path(__file__).resolve().parent / "observed"

# ---------------------------------------------------------------------------
# The universe.
# ---------------------------------------------------------------------------

DOMAINS = ("runtime", "static", "mixed")

LEAVES = ("leaf", "move_only_leaf")

SCOPES = ("shared", "unique", "external", "shared_cyclical")

# Carriers applied to a base type. The spelling is built by carrier_spellings.
CARRIERS = ("value", "&", "const &", "&&", "*", "const *")

# The dingo exception types, most derived first. All are siblings derived from
# dingo::exception; arena_allocation_exception exists in debug builds only.
EXCEPTIONS = (
    "type_not_found_exception",
    "type_not_convertible_exception",
    "type_ambiguous_exception",
    "type_recursion_exception",
    "lookup_already_registered_exception",
    "type_index_out_of_range_exception",
    "virtual_pointer_exception",
    "arena_allocation_exception",
)


@dataclass(frozen=True)
class Shape:
    """A stored shape. {T} is the leaf; the external expression builds the
    instance an external registration is given; declarations are objects the
    probe owns for it."""

    name: str
    spelling: str
    external: str
    layers: tuple = ()
    declarations: tuple = ()
    # Registered without an instance in a scope that constructs.
    external_note: str = ""


def _wrapper_shapes():
    outer = {
        "std::unique_ptr": "std::make_unique<{inner}>({arg})",
        "std::shared_ptr": "std::make_shared<{inner}>({arg})",
        "std::optional": "std::optional<{inner}>(std::in_place, {arg})",
    }
    inner = {
        "std::unique_ptr<{T}>": "std::make_unique<{T}>()",
        "std::shared_ptr<{T}>": "std::make_shared<{T}>()",
        "std::optional<{T}>": "std::optional<{T}>(std::in_place)",
    }
    for (o, template), (i, arg) in itertools.product(outer.items(), inner.items()):
        yield Shape(
            name=f"{o.split('::')[1]}_of_{i.split('::')[1].split('<')[0]}",
            spelling=f"{o}<{i}>",
            external=template.format(inner=f"{i}", arg=arg),
            layers=(i,),
        )


SHAPES = (
    Shape("value", "{T}", "{T}{{}}"),
    Shape("pointer", "{T}*", "&owned", declarations=("{T} owned;",)),
    Shape("reference", "{T}&", "owned", declarations=("{T} owned;",)),
    Shape("unique_ptr", "std::unique_ptr<{T}>", "std::make_unique<{T}>()"),
    Shape("shared_ptr", "std::shared_ptr<{T}>", "std::make_shared<{T}>()"),
    Shape("optional", "std::optional<{T}>", "std::optional<{T}>(std::in_place)"),
    Shape("array", "{T}[2]", "owned", declarations=("{T} owned[2];",)),
    Shape("unique_ptr_array", "std::unique_ptr<{T}[]>", "std::make_unique<{T}[]>(2)"),
    Shape(
        "shared_ptr_array",
        "std::shared_ptr<{T}[]>",
        "std::shared_ptr<{T}[]>(new {T}[2])",
    ),
    Shape(
        "variant",
        "std::variant<{T}, other_leaf>",
        "std::variant<{T}, other_leaf>(std::in_place_index<0>)",
    ),
    *_wrapper_shapes(),
)


def spell(template, leaf):
    return template.replace("{T}", leaf).replace("{{}}", "{}")


def stored_spelling(shape, leaf):
    return spell(shape.spelling, leaf)


ARRAY_RE = re.compile(r"^(.*)\[(\d+)\]$")


def carrier_spellings(base):
    """Valid spellings of the carriers applied to a base type, as
    (carrier, spelling) pairs. Invalid compositions are skipped."""
    array = ARRAY_RE.match(base)
    if array:
        element, size = array.groups()
        return [
            ("&", f"{element}(&)[{size}]"),
            ("const &", f"const {element}(&)[{size}]"),
            ("&&", f"{element}(&&)[{size}]"),
            ("*", f"{element}(*)[{size}]"),
            ("const *", f"const {element}(*)[{size}]"),
        ]
    if base.endswith("&"):
        return [("value", base)]
    const = (lambda b: f"{b} const") if base.endswith("*") else (lambda b: f"const {b}")
    return [
        ("value", base),
        ("&", f"{base}&"),
        ("const &", f"{const(base)}&"),
        ("&&", f"{base}&&"),
        ("*", f"{base}*"),
        ("const *", f"{const(base)}*"),
    ]


def requests_for(shape, leaf):
    """The deduplicated request spellings for a (leaf, stored shape)."""
    bases = [
        leaf,
        f"std::unique_ptr<{leaf}>",
        f"std::shared_ptr<{leaf}>",
        f"std::optional<{leaf}>",
        stored_spelling(shape, leaf),
        *(spell(layer, leaf) for layer in shape.layers),
    ]
    requests = []
    for base in bases:
        for _, request in carrier_spellings(base):
            if request not in requests:
                requests.append(request)
    return requests


def case_id(domain, leaf, scope, stored, request):
    return f"{domain}/{leaf}/{scope}/{stored}/{request}"


# ---------------------------------------------------------------------------
# Generated C++.
# ---------------------------------------------------------------------------

PRELUDE = r"""#pragma once
#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace probe {
struct state {
  unsigned long def = 0, copy = 0, move = 0, dtor = 0, seq = 0;
  std::map<const void *, unsigned long> live;
};
inline state &st() {
  static state &s = *new state;
  return s;
}
inline void created(const void *p) { st().live[p] = ++st().seq; }
inline void destroyed(const void *p) {
  st().live.erase(p);
  ++st().dtor;
}
} // namespace probe

struct other_leaf {
  other_leaf() {}
};

struct leaf {
  leaf() { probe::created(this); ++probe::st().def; }
  leaf(const leaf &) { probe::created(this); ++probe::st().copy; }
  leaf(leaf &&) noexcept { probe::created(this); ++probe::st().move; }
  leaf &operator=(const leaf &) = default;
  leaf &operator=(leaf &&) = default;
  ~leaf() { probe::destroyed(this); }
};

struct move_only_leaf {
  move_only_leaf() { probe::created(this); ++probe::st().def; }
  move_only_leaf(const move_only_leaf &) = delete;
  move_only_leaf(move_only_leaf &&) noexcept { probe::created(this); ++probe::st().move; }
  move_only_leaf &operator=(const move_only_leaf &) = delete;
  move_only_leaf &operator=(move_only_leaf &&) = default;
  ~move_only_leaf() { probe::destroyed(this); }
};

namespace probe {
template <typename T>
inline constexpr bool is_leaf_v =
    std::is_same_v<std::remove_cv_t<T>, leaf> ||
    std::is_same_v<std::remove_cv_t<T>, move_only_leaf>;

// The address of the leaf a result reaches, or null for an empty handle.
template <typename X> const void *reach(const X &x) {
  using U = std::remove_cv_t<X>;
  if constexpr (is_leaf_v<U>) {
    return &x;
  } else if constexpr (std::is_array_v<U>) {
    return reach(x[0]);
  } else if constexpr (std::is_pointer_v<U>) {
    return x ? reach(*x) : nullptr;
  } else if constexpr (std::is_same_v<U, std::optional<leaf>> ||
                       std::is_same_v<U, std::optional<move_only_leaf>>) {
    return x ? reach(*x) : nullptr;
  } else if constexpr (requires { x.get(); }) {
    return reach(x.get());
  } else if constexpr (requires { x.has_value(); }) {
    return x ? reach(*x) : nullptr;
  } else {
    return std::visit(
        [](const auto &v) -> const void * {
          if constexpr (is_leaf_v<std::remove_cvref_t<decltype(v)>>)
            return &v;
          else
            return nullptr;
        },
        x);
  }
}

inline std::string classify() {
  try {
    throw;
  } catch (const dingo::type_not_found_exception &) {
    return "type_not_found_exception";
  } catch (const dingo::type_not_convertible_exception &) {
    return "type_not_convertible_exception";
  } catch (const dingo::type_ambiguous_exception &) {
    return "type_ambiguous_exception";
  } catch (const dingo::type_recursion_exception &) {
    return "type_recursion_exception";
  } catch (const dingo::lookup_already_registered_exception &) {
    return "lookup_already_registered_exception";
  } catch (const dingo::type_index_out_of_range_exception &) {
    return "type_index_out_of_range_exception";
  } catch (const dingo::virtual_pointer_exception &) {
    return "virtual_pointer_exception";
#ifdef _DEBUG
  } catch (const dingo::arena_allocation_exception &) {
    return "arena_allocation_exception";
#endif
  } catch (const dingo::exception &) {
    return "exception";
  } catch (const std::exception &) {
    return "std";
  } catch (...) {
    return "unknown";
  }
}

struct snapshot {
  unsigned long def, copy, move, dtor, seq;
};
inline snapshot take() {
  auto &s = st();
  return {s.def, s.copy, s.move, s.dtor, s.seq};
}
inline void print_counts(const char *key, const snapshot &a, const snapshot &b) {
  std::printf(",\"%s\":{\"default\":%ld,\"copy\":%ld,\"move\":%ld,\"destroy\":%ld}",
              key, (long)(b.def - a.def), (long)(b.copy - a.copy),
              (long)(b.move - a.move), (long)(b.dtor - a.dtor));
}
inline const char *identity(const void *address, unsigned long before) {
  if (!address)
    return "none";
  auto it = st().live.find(address);
  if (it == st().live.end())
    return "unknown";
  return it->second > before ? "new" : "existing";
}
inline void print_identity(const char *key, const void *address, unsigned long before,
                           bool applicable) {
  if (applicable)
    std::printf(",\"%s\":\"%s\"", key, identity(address, before));
  else
    std::printf(",\"%s\":null", key);
}

// Resolves R twice from a fresh container and prints one JSON line.
template <typename R, typename Container>
void run(int index, Container &container, const std::string &registration) {
  std::printf("{\"i\":%d,\"registration\":\"%s\"", index, registration.c_str());
  if (registration != "ok") {
    std::printf(",\"resolve\":\"not_run\"}\n");
    std::fflush(stdout);
    return;
  }
  constexpr bool plain_leaf =
      !std::is_reference_v<R> && is_leaf_v<std::remove_cvref_t<R>>;
  snapshot before = take();
  try {
    decltype(auto) first = container.template resolve<R>();
    snapshot after = take();
    const void *address = plain_leaf ? nullptr : reach(first);
    std::printf(",\"resolve\":\"ok\"");
    print_counts("counts", before, after);
    print_identity("identity", address, before.seq, !plain_leaf);
    snapshot before2 = take();
    try {
      decltype(auto) second = container.template resolve<R>();
      snapshot after2 = take();
      const void *address2 = plain_leaf ? nullptr : reach(second);
      std::printf(",\"stable\":%s", (!plain_leaf && address == address2) ? "true" : "false");
      print_counts("warm_counts", before2, after2);
      print_identity("warm_identity", address2, before2.seq, !plain_leaf);
    } catch (...) {
      std::printf(",\"stable\":false,\"warm_counts\":null,\"warm_identity\":null,"
                  "\"warm_resolve\":\"throws:%s\"", classify().c_str());
    }
  } catch (...) {
    std::printf(",\"resolve\":\"throws:%s\"", classify().c_str());
  }
  std::printf("}\n");
  std::fflush(stdout);
}
} // namespace probe
"""


@dataclass
class Group:
    domain: str
    leaf: str
    scope: str
    shape: Shape
    requests: list = field(default_factory=list)

    @property
    def stored(self):
        return stored_spelling(self.shape, self.leaf)

    @property
    def name(self):
        return f"{self.leaf}-{self.scope}-{self.shape.name}"

    @property
    def directory(self):
        return self.domain

    def case(self, index):
        return case_id(
            self.domain, self.leaf, self.scope, self.stored, self.requests[index]
        )


def container_code(group):
    """Lines declaring the container of a case, the registration, and the
    registration exception handling. The container is named c."""
    stored = group.stored
    scope = f"dingo::scope<dingo::{group.scope}>"
    if group.domain == "runtime":
        arguments = ""
        if group.scope == "external":
            arguments = spell(group.shape.external, group.leaf)
        return [
            "dingo::container<> c;",
            "try {",
            f"  c.register_type<{scope}, dingo::storage<{stored}>>({arguments});",
            "} catch (...) { registration = probe::classify(); }",
        ]
    template = "dingo::static_container" if group.domain == "static" else "dingo::container"
    return [f"{template}<source> c;"]


def render(group, indices, guarded):
    """The program of a group. With guarded the program contains every request
    but a request is compiled only when ONLY is undefined or its index; the
    registration is always compiled. Otherwise it contains exactly the given
    requests and runs them, starting at argv[1]."""
    leaf = group.leaf
    lines = ['#include "prelude.h"', ""]
    if group.domain != "runtime":
        lines += [
            "using source = dingo::bindings<dingo::bind<"
            f"dingo::scope<dingo::{group.scope}>, dingo::storage<{group.stored}>>>;",
            "",
        ]
    declarations = [spell(d, leaf) for d in group.shape.declarations]
    selected = range(len(group.requests)) if guarded else indices
    for index in selected:
        request = group.requests[index]
        lines.append(f"void run_case_{index}() {{")
        lines += [f"  {line}" for line in declarations]
        lines.append('  std::string registration = "ok";')
        lines += [f"  {line}" for line in container_code(group)]
        if guarded:
            lines.append(f"#if !defined(ONLY) || ONLY == {index}")
        lines.append(f"  probe::run<{request}>({index}, c, registration);")
        if guarded:
            lines.append("#endif")
        lines.append("}")
        lines.append("")
    lines.append("int main(int argc, char **argv) {")
    lines.append("  int from = argc > 1 ? std::atoi(argv[1]) : 0;")
    for index in selected:
        lines.append(f"  if (from <= {index}) run_case_{index}();")
    lines.append("  return 0;")
    lines.append("}")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Toolchain.
# ---------------------------------------------------------------------------


class Toolchain:
    def __init__(self, command):
        self.command = shlex.split(command)
        version = self._run([*self.command, "--version"]).lower()
        if "clang" in version:
            self.family = "clang"
        elif "gcc" in version or "g++" in version:
            self.family = "gcc"
        else:
            self.family = "unknown"
        self.major = self._run([*self.command, "-dumpversion"]).split(".", 1)[0]
        machine = platform.machine().lower()
        self.arch = {"amd64": "x86_64", "aarch64": "arm64"}.get(machine, machine)
        std = next(
            (arg for arg in self.command if arg.startswith("-std=")), "-std=default"
        )
        self.std = std.split("=", 1)[1]

    @property
    def key(self):
        return f"{self.family}{self.major}-{self.arch}-{self.std}"

    @staticmethod
    def _run(args):
        return subprocess.run(
            args, check=True, capture_output=True, text=True
        ).stdout.strip()

    def compile(self, args):
        """Returns (succeeded, first error line)."""
        result = subprocess.run(
            [*self.command, *args], capture_output=True, text=True
        )
        if result.returncode == 0:
            return True, ""
        error = next(
            (l for l in result.stderr.splitlines() if "error" in l),
            result.stderr.strip()[:300],
        )
        return False, error.strip()[:400]


# ---------------------------------------------------------------------------
# Extraction.
# ---------------------------------------------------------------------------


def build_groups(domains, pattern=None):
    groups = []
    for domain, leaf, scope, shape in itertools.product(
        domains, LEAVES, SCOPES, SHAPES
    ):
        group = Group(domain, leaf, scope, shape)
        group.requests = requests_for(shape, leaf)
        if pattern is None or re.search(
            pattern, f"{domain}/{leaf}/{scope}/{group.stored}"
        ):
            groups.append(group)
    return groups


class Extraction:
    def __init__(self, toolchain, work_dir, jobs):
        self.toolchain = toolchain
        self.work_dir = work_dir / toolchain.key
        self.jobs = jobs
        self.work_dir.mkdir(parents=True, exist_ok=True)
        (self.work_dir / "prelude.h").write_text(PRELUDE)
        self.errors = {}

    def path(self, group, suffix):
        directory = self.work_dir / group.domain
        directory.mkdir(exist_ok=True)
        return directory / f"{group.name}{suffix}"

    def syntax(self, group, source, only):
        args = ["-fsyntax-only", f"-I{self.work_dir}", str(source)]
        if only is not None:
            args.insert(0, f"-DONLY={only}")
        return self.toolchain.compile(args)

    def map(self, function, items):
        items = list(items)
        with concurrent.futures.ThreadPoolExecutor(max_workers=self.jobs) as pool:
            return list(pool.map(function, items))

    def run(self, domains, pattern=None):
        groups = build_groups(domains, pattern)
        for group in groups:
            self.path(group, ".cpp").write_text(
                render(group, range(len(group.requests)), guarded=True)
            )

        # Registration outcome: the program without any resolve.
        log("registration compiles", len(groups))
        outcomes = self.map(lambda g: self.syntax(g, self.path(g, ".cpp"), -1), groups)
        compiled = {}
        registration_errors = {}
        for group, (ok, error) in zip(groups, outcomes):
            if ok:
                compiled[id(group)] = True
            else:
                registration_errors[id(group)] = error

        # Resolve compile outcome: all requests at once, else one by one.
        live = [g for g in groups if id(g) in compiled]
        log("whole-group compiles", len(live))
        whole = self.map(lambda g: self.syntax(g, self.path(g, ".cpp"), None), live)
        compile_ok = {}
        singles = []
        for group, (ok, _) in zip(live, whole):
            if ok:
                compile_ok[id(group)] = set(range(len(group.requests)))
            else:
                compile_ok[id(group)] = set()
                singles += [(group, i) for i in range(len(group.requests))]
        log("per-request compiles", len(singles))
        results = self.map(
            lambda task: self.syntax(task[0], self.path(task[0], ".cpp"), task[1]),
            singles,
        )
        for (group, index), (ok, _) in zip(singles, results):
            if ok:
                compile_ok[id(group)].add(index)

        # Executables, one per group with at least one compiling request.
        log("executables", len(live))
        runs = self.map(lambda g: self.build_and_run(g, compile_ok[id(g)]), live)
        observed = {id(g): r for g, r in zip(live, runs)}

        records = []
        for group in groups:
            records += self.records(
                group,
                registration_errors.get(id(group)),
                observed.get(id(group)),
            )
        for group in groups:
            if id(group) in registration_errors:
                self.errors[
                    f"{group.domain}/{group.leaf}/{group.scope}/{group.stored}"
                ] = registration_errors[id(group)]
        records.sort(key=lambda r: r["id"])
        return records

    def build_and_run(self, group, indices):
        """Returns {index: parsed line or crash marker}, plus indices that fail
        to build as 'compile_error'."""
        indices = sorted(indices)
        result = {}
        executable = self.path(group, ".exe")
        while indices:
            source = self.path(group, ".run.cpp")
            source.write_text(render(group, indices, guarded=False))
            ok, error = self.toolchain.compile(
                ["-O0", f"-I{self.work_dir}", str(source), "-o", str(executable)]
            )
            if ok:
                break
            # Syntax-only compiles missed an error that code generation finds:
            # find the failing requests by building each alone.
            failing = []
            for index in indices:
                source_one = self.path(group, ".one.cpp")
                source_one.write_text(render(group, [index], guarded=False))
                if not self.toolchain.compile(
                    ["-c", f"-I{self.work_dir}", str(source_one), "-o", os.devnull]
                )[0]:
                    failing.append(index)
            if not failing:
                raise RuntimeError(f"{source} fails to build: {error}")
            for index in failing:
                result[index] = {"resolve": "compile_error", "registration": "ok"}
            indices = [i for i in indices if i not in failing]
        position = 0
        while position < len(indices):
            start = indices[position]
            process = subprocess.run(
                [str(executable), str(start)], capture_output=True, text=True
            )
            printed = []
            for line in process.stdout.splitlines():
                parsed = json.loads(line)
                result[parsed.pop("i")] = parsed
                printed.append(parsed)
            if process.returncode == 0:
                break
            crashed = next(i for i in indices[position:] if i not in result)
            name = (
                signal.Signals(-process.returncode).name
                if process.returncode < 0
                else f"exit{process.returncode}"
            )
            result[crashed] = {"resolve": f"crash:{name}", "registration": "ok"}
            position = indices.index(crashed) + 1
        return result

    def records(self, group, registration_error, observed):
        for index, request in enumerate(group.requests):
            record = {
                "counts": None,
                "domain": group.domain,
                "id": group.case(index),
                "identity": None,
                "leaf": group.leaf,
                "registration": "ok",
                "request": request,
                "resolve": "compile_error",
                "scope": group.scope,
                "stable": None,
                "stored": group.stored,
                "warm_counts": None,
                "warm_identity": None,
            }
            if registration_error is not None:
                record["registration"] = "compile_error"
                record["resolve"] = "not_run"
            elif observed is not None and index in observed:
                record.update(observed[index])
                record.pop("warm_resolve", None)
                if record["registration"] not in ("ok", "compile_error"):
                    record["registration"] = "throws:" + record["registration"]
            yield record


def log(message, count):
    print(f"[{time.strftime('%H:%M:%S')}] {message}: {count}", file=sys.stderr, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "--cxx-command",
        default=os.environ.get("CXX", "c++") + f" -std=c++20 -I{PROJECT_ROOT / 'include'}",
        help="compiler command including the standard and include flags",
    )
    parser.add_argument(
        "--work-dir",
        type=Path,
        required=True,
        help="directory for generated programs and executables",
    )
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 8))
    parser.add_argument("--domains", default=",".join(DOMAINS))
    parser.add_argument("--output", type=Path, default=None)
    parser.add_argument(
        "--filter",
        help="regular expression selecting groups by domain/leaf/scope/stored "
        "(development aid; the output then covers the selection only)",
    )
    args = parser.parse_args()

    domains = args.domains.split(",")
    unknown = [d for d in domains if d not in DOMAINS]
    if unknown:
        parser.error(f"unknown domains: {unknown}")

    toolchain = Toolchain(args.cxx_command)
    started = time.time()
    extraction = Extraction(toolchain, args.work_dir.resolve(), args.jobs)
    records = extraction.run(domains, args.filter)
    output = args.output or OBSERVED_DIR / f"{toolchain.key}.jsonl"
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w") as stream:
        for record in records:
            stream.write(json.dumps(record, sort_keys=True) + "\n")
    errors = extraction.work_dir / "registration-errors.txt"
    errors.write_text(
        "".join(f"{key}\t{error}\n" for key, error in sorted(extraction.errors.items()))
    )
    per_domain = {d: sum(1 for r in records if r["domain"] == d) for d in domains}
    print(
        f"{toolchain.key}: {len(records)} cases {per_domain} in "
        f"{time.time() - started:.0f}s -> {output}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
