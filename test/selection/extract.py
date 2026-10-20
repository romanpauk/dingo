#!/usr/bin/env python3
"""Extracts how the current implementation selects bindings by key.

For every combination of container domain, bindings, lookup declarations, and
request the extractor records what the library does: whether each binding can
be registered, and which implementation a singular request selects, the
ordered implementations a collection request selects, or how the request
fails. The table is ground truth for the reference implementation of
selection in spec/reference/selection.py; it describes the code as it is.

Usage:

    extract.py --work-dir build/selection [--jobs N] [--cxx-command CMD]
               [--include-dir DIR] [--domains runtime,static,mixed,parent]
               [--filter REGEX] [--output FILE]

The default output is <work-dir>/observed/<toolchain>.jsonl, where the
toolchain key is the one test/routes/extract.py uses, such as
gcc16-x86_64-c++20. The file is JSON Lines sorted by case id.

Universe:

- one interface `iface` (abstract, `virtual int id() const`) and the
  implementations impl_0..impl_3 returning their index, all registered with
  scope<shared> and interfaces<iface>. The first binding of a case is impl_0,
  the second impl_1 or impl_0 again.
- keys: none, the tags tag_a and tag_b, the fixed values key_type<int, 7> and
  key_type<int, 8>, and the runtime values key_value{7} and key_value{8}.
- domains: "runtime" is dingo::container<traits>; "static" is
  dingo::static_container<dingo::bindings<...>, traits>; "mixed" is
  dingo::container<dingo::bindings<...>> with runtime registrations added
  (its traits are fixed); "parent" is a runtime child container whose parent
  is a runtime container, the request made of the child.
- bindings: up to two. A binding of a mixed container is static or runtime, of
  a parent case in the parent or in the child. The order of a static and a
  runtime binding, and of a parent and a child binding, is not observable, so
  static bindings precede runtime ones and parent bindings child ones.
- settings: "default" declares associative<int, iface> (cardinality one, the
  current library needs a declaration for value keys) and nothing else;
  "many" declares every key domain of the universe many: collection<iface>,
  typed<tag_a, iface, many>, typed<tag_b, iface, many>, and
  associative<int, iface, many>. The mixed container has no traits, so it has
  the default setting only, without any declaration.
- requests: "one" is a singular request, resolve<iface &>() with no key,
  resolve<iface &>(key_type<tag>{}) with a tag, the constructor parameter
  dependency<iface &, key_type<int, V>> of consumer<V> constructed with
  construct<consumer<V>>() with a fixed value, resolve<iface &>(V) with a
  runtime value; "all" is construct_collection<std::vector<iface *>>() with
  the same argument (key_type<tag>{}, key_type<int, V>{}, or V). The
  construct_collection of dingo::container takes no value key (a key_type<int,
  V> or an int argument is taken for the append function), so outside the
  static container the collection of a value key is resolved instead:
  resolve<std::vector<iface *>>(key_type<int, V>{}) or (V).

A case id is "<domain>/<setting>/<bindings>/<request>". The bindings are
"empty" or joined with "+", each "<impl>:<key>" followed by ":<place>" in the
mixed (static, runtime) and parent (parent, child) domains. A key is none,
tag_a, tag_b, fixed7, fixed8, runtime7, runtime8. The request is
"<mode>:<key>" with the mode one or all. For example
"mixed/default/impl_0:fixed7:static+impl_1:runtime7:runtime/one:runtime7".

A record has the keys

  id, domain, setting, bindings, request
  registration    "ok", "compile_error", "not_expressible", or
                  "throws:<exception>" of the first binding that throws
  registrations   the outcome of every binding in binding order ("ok" or
                  "throws:<exception>"), null unless the registration compiles
  selection       "ok", "compile_error", "throws:<exception>", "crash:<signal>",
                  or "not_run" when the registration does not compile
  selected        the index of the selected implementation (one), the ordered
                  indices (all), or null unless the selection is "ok"

A combination the current library cannot spell is "not_expressible": a runtime
value key in a static binding, and the many setting of the mixed container,
which --mixed-traits spells for an implementation that takes traits there.
Every case uses a fresh container. <exception> is the most specific dingo
exception type caught, else "std" or "unknown".

Method: per container configuration (domain, setting, bindings) one generated
program contains one function per request. A registration-only compile decides
the registration; the requests that compile (all at once, else one
-fsyntax-only compile per request) are built into one executable that runs
each request on a fresh container and prints one JSON line. Run with -1 the
executable makes the registrations alone, which gives the registrations of a
case whose request does not compile.
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

sys.dont_write_bytecode = True

import jinja2  # noqa: E402

PROJECT_ROOT = Path(__file__).resolve().parents[2]
TEMPLATES = Path(__file__).resolve().parent / "templates"

# ---------------------------------------------------------------------------
# The universe.
# ---------------------------------------------------------------------------

DOMAINS = ("runtime", "static", "mixed", "parent")
SETTINGS = ("default", "many")

# Key names of case ids and their C++ data: kind, type, value.
KEYS = {
    "none": {"kind": "none", "type": None, "value": None},
    "tag_a": {"kind": "tag", "type": "tag_a", "value": None},
    "tag_b": {"kind": "tag", "type": "tag_b", "value": None},
    "fixed7": {"kind": "fixed", "type": "int", "value": 7},
    "fixed8": {"kind": "fixed", "type": "int", "value": 8},
    "runtime7": {"kind": "runtime", "type": "int", "value": 7},
    "runtime8": {"kind": "runtime", "type": "int", "value": 8},
}

MODES = ("one", "all")
REQUESTS = tuple(f"{mode}:{key}" for mode in MODES for key in KEYS)

# The places a binding of each domain can have, in canonical order.
PLACES = {
    "runtime": ("runtime",),
    "static": ("static",),
    "mixed": ("static", "runtime"),
    "parent": ("parent", "child"),
}

# The implementation of the first binding, and the choices for the second.
FIRST_IMPL = "impl_0"
SECOND_IMPLS = ("impl_1", "impl_0")

# The lookup declarations of each setting, for the domains with traits.
LOOKUPS = {
    "default": ({"kind": "associative", "key": "int", "cardinality": "one"},),
    "many": (
        {"kind": "collection"},
        {"kind": "typed", "key": "tag_a", "cardinality": "many"},
        {"kind": "typed", "key": "tag_b", "cardinality": "many"},
        {"kind": "associative", "key": "int", "cardinality": "many"},
    ),
}
TRAITS_BASE = {
    "runtime": "dynamic_container_traits",
    "parent": "dynamic_container_traits",
    "static": "static_container_traits",
}

# Whether the mixed container takes traits, container<bindings<...>,
# Traits>, as the next implementation does; --mixed-traits sets it.
MIXED_TRAITS = False

# The dingo exception types, as test/routes/extract.py lists them.
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
class Binding:
    impl: str
    key: str
    place: str

    def label(self, domain):
        if len(PLACES[domain]) == 1:
            return f"{self.impl}:{self.key}"
        return f"{self.impl}:{self.key}:{self.place}"

    @property
    def expressible(self):
        return not (self.place == "static" and KEYS[self.key]["kind"] == "runtime")


def bindings_label(domain, bindings):
    return "+".join(b.label(domain) for b in bindings) or "empty"


def case_id(domain, setting, bindings, request):
    return f"{domain}/{setting}/{bindings_label(domain, bindings)}/{request}"


def parse_case_id(case):
    """Splits a case id into (domain, setting, bindings, mode, key); the
    bindings are Binding values with their place filled in."""
    domain, setting, label, request = case.split("/")
    bindings = []
    if label != "empty":
        for text in label.split("+"):
            parts = text.split(":")
            impl, key = parts[0], parts[1]
            place = parts[2] if len(parts) > 2 else PLACES[domain][0]
            if domain not in PLACES or place not in PLACES[domain] or key not in KEYS:
                raise ValueError(f"malformed binding {text!r} in {case!r}")
            bindings.append(Binding(impl, key, place))
    mode, key = request.split(":")
    if mode not in MODES or key not in KEYS:
        raise ValueError(f"malformed request {request!r} in {case!r}")
    return domain, setting, tuple(bindings), mode, key


def binding_sequences(domain):
    """The canonical binding lists of a domain: none, one, or two bindings,
    the places in canonical order."""
    places = PLACES[domain]
    yield ()
    for key, place in itertools.product(KEYS, places):
        yield (Binding(FIRST_IMPL, key, place),)
    for (k1, p1), (k2, p2) in itertools.product(
        itertools.product(KEYS, places), repeat=2
    ):
        if places.index(p1) > places.index(p2):
            continue
        for second in SECOND_IMPLS:
            yield (Binding(FIRST_IMPL, k1, p1), Binding(second, k2, p2))


@dataclass
class Group:
    domain: str
    setting: str
    bindings: tuple
    requests: list = field(default_factory=lambda: list(REQUESTS))

    @property
    def key(self):
        return f"{self.domain}/{self.setting}/{bindings_label(self.domain, self.bindings)}"

    @property
    def expressible(self):
        if self.domain == "mixed" and self.setting != "default" and not MIXED_TRAITS:
            return False
        return all(b.expressible for b in self.bindings)

    @property
    def name(self):
        return f"{self.setting}-{bindings_label(self.domain, self.bindings).replace(':', '_')}"

    def case(self, index):
        return f"{self.key}/{self.requests[index]}"


def build_groups(domains, pattern=None):
    groups = []
    for domain in domains:
        for setting in SETTINGS:
            for bindings in binding_sequences(domain):
                group = Group(domain, setting, bindings)
                if pattern is None or re.search(pattern, group.key):
                    groups.append(group)
    return groups


# ---------------------------------------------------------------------------
# Generated C++.
# ---------------------------------------------------------------------------

ENVIRONMENT = jinja2.Environment(
    loader=jinja2.FileSystemLoader(str(TEMPLATES)),
    undefined=jinja2.StrictUndefined,
    trim_blocks=True,
    lstrip_blocks=True,
    keep_trailing_newline=True,
)
PRELUDE_HEADERS = ("probe.h",)


def group_data(group):
    """The data the template renders a group from."""
    traits = None
    if group.domain == "mixed" and MIXED_TRAITS:
        traits = {
            "base": "dynamic_container_traits",
            "lookups": list(LOOKUPS[group.setting]),
        }
    elif group.domain in TRAITS_BASE:
        traits = {
            "base": TRAITS_BASE[group.domain],
            "lookups": list(LOOKUPS[group.setting]),
        }
    return {
        "domain": group.domain,
        "traits": traits,
        "bindings": [
            {"impl": b.impl, "key": KEYS[b.key], "place": b.place}
            for b in group.bindings
        ],
        "requests": [
            {"mode": mode, "key": KEYS[key]}
            for mode, key in (r.split(":") for r in group.requests)
        ],
    }


def render(group, indices, guarded):
    selected = range(len(group.requests)) if guarded else indices
    return ENVIRONMENT.get_template("group.cpp.j2").render(
        group=group_data(group), selected=list(selected), guarded=guarded
    )


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
        result = subprocess.run([*self.command, *args], capture_output=True, text=True)
        if result.returncode == 0:
            return True, ""
        error = next(
            (line for line in result.stderr.splitlines() if "error" in line),
            result.stderr.strip()[:300],
        )
        return False, error.strip()[:400]


# ---------------------------------------------------------------------------
# Extraction.
# ---------------------------------------------------------------------------


class Extraction:
    def __init__(self, toolchain, work_dir, jobs):
        self.toolchain = toolchain
        self.work_dir = work_dir / toolchain.key
        self.jobs = jobs
        self.work_dir.mkdir(parents=True, exist_ok=True)
        for header in PRELUDE_HEADERS:
            (self.work_dir / header).write_text((TEMPLATES / header).read_text())
        self.errors = {}

    def path(self, group, suffix):
        directory = self.work_dir / group.domain
        directory.mkdir(exist_ok=True)
        return directory / f"{group.name}{suffix}"

    def syntax(self, group, only):
        args = ["-fsyntax-only", f"-I{self.work_dir}", str(self.path(group, ".cpp"))]
        if only is not None:
            args.insert(0, f"-DONLY={only}")
        return self.toolchain.compile(args)

    def map(self, function, items):
        items = list(items)
        with concurrent.futures.ThreadPoolExecutor(max_workers=self.jobs) as pool:
            return list(pool.map(function, items))

    def run(self, domains, pattern=None):
        groups = build_groups(domains, pattern)
        live = [g for g in groups if g.expressible]
        for group in live:
            self.path(group, ".cpp").write_text(
                render(group, range(len(group.requests)), True)
            )

        log("registration compiles", len(live))
        outcomes = self.map(lambda g: self.syntax(g, -1), live)
        registration_errors = {}
        for group, (ok, error) in zip(live, outcomes):
            if not ok:
                registration_errors[id(group)] = error
                self.errors[group.key] = error
        live = [g for g in live if id(g) not in registration_errors]

        log("whole-group compiles", len(live))
        whole = self.map(lambda g: self.syntax(g, None), live)
        compile_ok = {}
        singles = []
        for group, (ok, _) in zip(live, whole):
            if ok:
                compile_ok[id(group)] = set(range(len(group.requests)))
            else:
                compile_ok[id(group)] = set()
                singles += [(group, i) for i in range(len(group.requests))]
        log("per-request compiles", len(singles))
        results = self.map(lambda task: self.syntax(task[0], task[1]), singles)
        for (group, index), (ok, _) in zip(singles, results):
            if ok:
                compile_ok[id(group)].add(index)

        log("executables", len(live))
        runs = self.map(lambda g: self.build_and_run(g, compile_ok[id(g)]), live)
        observed = {id(g): r for g, r in zip(live, runs)}

        records = []
        for group in groups:
            records += self.records(
                group, id(group) in registration_errors, observed.get(id(group))
            )
        records.sort(key=lambda r: r["id"])
        return records

    def build_and_run(self, group, indices):
        """Returns ({index: parsed line}, registrations): a request that fails
        to build is a compile_error, one whose run dies a crash. The
        registrations are those of a run without a request."""
        indices = sorted(indices)
        result = {}
        executable = self.path(group, ".exe")
        while True:
            source = self.path(group, ".run.cpp")
            source.write_text(render(group, indices, False))
            ok, error = self.toolchain.compile(
                ["-O0", f"-I{self.work_dir}", str(source), "-o", str(executable)]
            )
            if ok:
                break
            if not indices:
                raise RuntimeError(f"{source} fails to build: {error}")
            failing = []
            for index in indices:
                one = self.path(group, ".one.cpp")
                one.write_text(render(group, [index], False))
                if not self.toolchain.compile(
                    ["-c", f"-I{self.work_dir}", str(one), "-o", os.devnull]
                )[0]:
                    failing.append(index)
            if not failing:
                raise RuntimeError(f"{source} fails to build: {error}")
            for index in failing:
                result[index] = {"selection": "compile_error"}
            indices = [i for i in indices if i not in failing]
        position = 0
        while indices and position < len(indices):
            process = subprocess.run(
                [str(executable), str(indices[position])],
                capture_output=True,
                text=True,
            )
            for line in process.stdout.splitlines():
                parsed = json.loads(line)
                result[parsed.pop("i")] = parsed
            if process.returncode == 0:
                break
            crashed = next(i for i in indices[position:] if i not in result)
            name = (
                signal.Signals(-process.returncode).name
                if process.returncode < 0
                else f"exit{process.returncode}"
            )
            result[crashed] = {"selection": f"crash:{name}"}
            position = indices.index(crashed) + 1
        process = subprocess.run(
            [str(executable), "-1"], capture_output=True, text=True, check=True
        )
        registrations = json.loads(process.stdout)["registrations"]
        return result, registrations

    def records(self, group, registration_error, observed):
        for index, request in enumerate(group.requests):
            record = {
                "bindings": bindings_label(group.domain, group.bindings),
                "domain": group.domain,
                "id": group.case(index),
                "registration": "ok",
                "registrations": None,
                "request": request,
                "selected": None,
                "selection": "compile_error",
                "setting": group.setting,
            }
            if not group.expressible:
                record["registration"] = "not_expressible"
                record["selection"] = "not_run"
            elif registration_error:
                record["registration"] = "compile_error"
                record["selection"] = "not_run"
            elif observed is not None:
                selections, registrations = observed
                record["registrations"] = registrations
                record.update(selections.get(index, {}))
            if record["registrations"] is not None:
                failed = [o for o in record["registrations"] if o != "ok"]
                record["registration"] = failed[0] if failed else "ok"
            yield record


def log(message, count):
    print(f"[{time.strftime('%H:%M:%S')}] {message}: {count}", file=sys.stderr, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "--include-dir",
        type=Path,
        default=PROJECT_ROOT / "include",
        help="include directory of the library to extract, include/ by default",
    )
    parser.add_argument(
        "--cxx-command",
        help="compiler command including the standard and include flags; by "
        "default $CXX -std=c++20 -I<include-dir>",
    )
    parser.add_argument(
        "--work-dir",
        type=Path,
        default=PROJECT_ROOT / "build" / "selection",
        help="directory for generated programs, executables, and the table",
    )
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 8))
    parser.add_argument("--domains", default=",".join(DOMAINS))
    parser.add_argument("--output", type=Path, default=None)
    parser.add_argument(
        "--mixed-traits",
        action="store_true",
        help="give the mixed container traits, container<bindings<...>, "
        "Traits>, as the next implementation takes them",
    )
    parser.add_argument(
        "--filter",
        help="regular expression selecting groups by domain/setting/bindings "
        "(development aid; the output then covers the selection only)",
    )
    args = parser.parse_args()

    global MIXED_TRAITS
    MIXED_TRAITS = args.mixed_traits
    domains = args.domains.split(",")
    unknown = [d for d in domains if d not in DOMAINS]
    if unknown:
        parser.error(f"unknown domains: {unknown}")

    include = f"-I{args.include_dir.resolve()}"
    command = args.cxx_command or (os.environ.get("CXX", "c++") + f" -std=c++20 {include}")
    toolchain = Toolchain(command)
    started = time.time()
    work_dir = args.work_dir.resolve()
    extraction = Extraction(toolchain, work_dir / "programs", args.jobs)
    records = extraction.run(domains, args.filter)
    output = args.output or work_dir / "observed" / f"{toolchain.key}.jsonl"
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w") as stream:
        for record in records:
            stream.write(json.dumps(record, sort_keys=True) + "\n")
    (extraction.work_dir / "registration-errors.txt").write_text(
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
