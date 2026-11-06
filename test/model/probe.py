"""Builds the model baseline: the outcome of every cell, asked from the library.

A cell is container x scope x stored form x leaf x binding x request. The
library answers each cell; nothing here decides what should work:

- invalid: the request cannot be spelled in C++, such as an abstract type
  held by value
- not_registrable: the library rejects the registration (binding_model::valid),
  so the probe does not register it
- rejected: the library rejects the request or the registration at compile
  time with a static_assert; its message is kept
- does_not_compile: the request fails to compile otherwise, which is a defect;
  the first error is kept
- crashed: the probe of the request did not exit, which is a defect
- violated: the request resolved, and the result is wrong: it does not reach
  the registered leaf, a kept instance is not kept, a unique one is, or the
  probe leaks
- resolved: the request resolved and the result is right
- type_not_found, type_not_convertible, type_ambiguous, exception: the
  exception the request threw

The containers must agree: a request that one resolves the other resolves,
unless one of them does not accept the registration. The probe fails when a
cell is a defect (does_not_compile, crashed, violated) or the containers
disagree.

Every registration gets its own translation unit, so a failure is confined to
its cells. A unit that fails to compile is split into one unit per request, to
find the requests that fail. A static container rejects at compile time, so
each of its requests is a unit of its own.
"""

import argparse
import concurrent.futures
from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

CONTAINERS = ("runtime", "static")
SCOPES = ("unique", "shared", "external", "shared_cyclical")

# The leaf every cell stores, by the operations it supports.
LEAVES = {
    "copyable": "leaf",
    "move_only": "move_only_leaf",
}

# A stored type of each storage form, around the leaf L.
STORED = (
    ("plain", "L"),
    ("reference", "L &"),
    ("pointer", "L *"),
    ("bounded_array", "L[2]"),
    ("unique_handle", "std::unique_ptr<L>"),
    ("unique_array_handle", "std::unique_ptr<L[]>"),
    ("shared_handle", "std::shared_ptr<L>"),
    ("shared_array_handle", "std::shared_ptr<L[]>"),
    ("optional", "std::optional<L>"),
    ("alternative", "std::variant<L, other>"),
)

# The requests of a binding requested as T, and whether a request holds T by
# value, which an abstract T cannot be. A resolved rvalue reference is
# returned by value.
REQUESTS = (
    ("T", True),
    ("const T", True),
    ("T &", False),
    ("const T &", False),
    ("T &&", True),
    ("T *", False),
    ("const T *", False),
    ("std::shared_ptr<T>", False),
    ("std::shared_ptr<T> &", False),
    ("const std::shared_ptr<T> &", False),
    ("std::shared_ptr<T> *", False),
    ("std::unique_ptr<T>", False),
    ("std::unique_ptr<T> &", False),
    ("std::unique_ptr<T> &&", False),
    ("std::optional<T>", True),
    ("std::optional<T> &", True),
    ("std::optional<T> *", True),
    ("std::variant<T, other>", True),
    ("std::variant<T, other> &", True),
)

# How a binding is registered and requested:
# - interface: the interfaces<> of the registration, or None
# - target: the type T of the requests
# - abstract: whether T is abstract
BINDINGS = {
    "concrete": dict(interface=None, target="L", abstract=False),
    "interface": dict(interface="leaf_interface", target="leaf_interface",
                      abstract=True),
    "annotated": dict(interface="dingo::annotated<L, tag>", target="L",
                      abstract=False),
    "keyed": dict(interface="L", target="L", abstract=False),
}

KEY = "dingo::key_type<std::size_t, 1>"


@dataclass(frozen=True)
class Registration:
    container: str
    scope: str
    form: str
    leaf: str
    binding: str

    @property
    def name(self):
        return "__".join(
            (self.container, self.scope, self.form, self.leaf, self.binding))

    @property
    def leaf_type(self):
        return LEAVES[self.leaf]

    @property
    def stored(self):
        return dict(STORED)[self.form].replace("L", self.leaf_type)

    @property
    def traits(self):
        return BINDINGS[self.binding]

    def spell(self, text):
        return text.replace("L", self.leaf_type)

    @property
    def arguments(self):
        arguments = (
            f"dingo::scope<dingo::{self.scope}>, dingo::storage<{self.stored}>")
        if self.traits["interface"]:
            interface = self.spell(self.traits["interface"])
            arguments += f", dingo::interfaces<{interface}>"
        return arguments

    @property
    def static_arguments(self):
        if self.binding == "keyed":
            return f"{self.arguments}, {KEY}"
        return self.arguments

    def requests(self):
        """The requests, each with whether it is invalid C++."""
        target = self.spell(self.traits["target"])
        return [
            (spelling.replace("T", target), self.traits["abstract"] and by_value)
            for spelling, by_value in REQUESTS
        ]

    def cell(self, request):
        return "/".join((self.container, self.scope, self.form, self.leaf,
                         self.binding, request))

    def container_type(self):
        base = f"dingo::{self.container}_container_traits"
        traits = (f"keyed_traits<{self.leaf_type}, {base}>"
                  if self.binding == "keyed" else base)
        if self.container == "runtime":
            return f"dingo::container<{traits}>"
        return (f"dingo::static_container<dingo::bindings<dingo::bind<"
                f"{self.static_arguments}>>, {traits}>")

    def given(self):
        """The instance an external scope is given, or None."""
        if self.scope != "external":
            return None
        if self.form in ("reference", "bounded_array"):
            object_type = self.stored.replace("&", "").strip()
            return f"external_object<{object_type}>()"
        return f"instance<{self.stored}>::make()"

    def register(self):
        """The statement that registers the stored type in container c."""
        if self.container == "static":
            return ""
        args = [self.given()] if self.given() else []
        if self.binding == "keyed":
            args.append("dingo::key_value{std::size_t(1)}")
        return (f"c.template register_type<{self.arguments}>"
                f"({', '.join(args)});")

    def resolve(self, request):
        if self.binding == "annotated":
            request = f"dingo::annotated<{request}, tag>"
        if self.binding == "keyed":
            if self.container == "runtime":
                return f"c.template resolve<{request}>(std::size_t(1))"
            request = f"dingo::dependency<{request}, {KEY}>"
        return f"c.template resolve<{request}>()"

    def source(self, requests):
        static = "true" if self.container == "static" else "false"
        lines = [
            "// Generated by test/model/probe.py. Do not edit.",
            '#include "model/probe.h"',
            "",
            "using namespace dingo::model;",
            "",
            "// The container is a template parameter, so that the calls on it",
            "// are dependent and the branch not taken is discarded.",
            "template <typename Container> void run() {",
            "  if constexpr (registrable<dingo::type_registration<"
            f"{self.static_arguments}>, {static}>()) {{",
        ]
        keeps = "nothing" if self.scope == "unique" else "instance"
        if self.given():
            # What the scope is given outlives the probes; make it first, so
            # that they do not count it as leaked.
            lines.append(f"    (void){self.given()};")
        for request in requests:
            resolve = self.resolve(request)
            lines += [
                f"    probe<Container, decltype(std::declval<Container &>()"
                f".{resolve.removeprefix('c.')})>(",
                f'        "{self.cell(request)}", keeps::{keeps},',
                f"        [](auto &c) {{ {self.register()} }},",
                f"        [](auto &c) -> decltype(auto) {{ return {resolve}; }});",
            ]
        lines.append("  } else {")
        lines += [
            f'    record("{self.cell(request)}", "not_registrable");'
            for request in requests
        ]
        lines += [
            "  }",
            "}",
            "",
            f"int main() {{ run<{self.container_type()}>(); }}",
            "",
        ]
        return "\n".join(lines)


def registrations():
    for container in CONTAINERS:
        for scope in SCOPES:
            for form, _ in STORED:
                for leaf in LEAVES:
                    for binding in BINDINGS:
                        yield Registration(container, scope, form, leaf,
                                           binding)


ERROR = re.compile(r"error: (.*)")
ASSERTION = "static assertion failed:"
CRASH = "exited with"


def first_error(output):
    for line in output.splitlines():
        match = ERROR.search(line)
        if match:
            return match.group(1).strip()
    return output.strip().splitlines()[-1] if output.strip() else "unknown"


def build_and_run(compiler, flags, directory, name, source):
    """Compiles and runs one unit; returns (records, error), where the error
    is the first compile error, or how the unit exited when it crashed."""
    cpp = directory / f"{name}.cpp"
    exe = directory / name
    cpp.write_text(source)
    result = subprocess.run(
        [compiler, *flags, str(cpp), "-o", str(exe)],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return None, first_error(result.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    exe.unlink()
    records = [json.loads(line) for line in run.stdout.splitlines() if line]
    if run.returncode != 0:
        return records, f"{CRASH} {run.returncode}"
    return records, None


def failure(registration, request, error):
    if error.startswith(CRASH):
        outcome = "crashed"
    elif error.startswith(ASSERTION):
        outcome = "rejected"
    else:
        outcome = "does_not_compile"
    return {
        "cell": registration.cell(request),
        "outcome": outcome,
        "error": error.removeprefix(ASSERTION).strip(),
    }


def probe_registration(compiler, flags, directory, registration):
    outcomes = []
    requests = []
    for request, invalid in registration.requests():
        if invalid:
            outcomes.append({
                "cell": registration.cell(request),
                "outcome": "invalid",
                "error": "an abstract type cannot be held by value",
            })
        else:
            requests.append(request)
    if registration.container == "runtime":
        records, error = build_and_run(
            compiler, flags, directory, registration.name,
            registration.source(requests),
        )
        if error is None:
            return outcomes + records
    for index, request in enumerate(requests):
        records, error = build_and_run(
            compiler, flags, directory, f"{registration.name}__{index}",
            registration.source((request,)),
        )
        if error is None:
            outcomes += records
        else:
            outcomes.append(failure(registration, request, error))
    return outcomes


MARKS = {
    "resolved": "ok",
    "type_not_convertible": "nc",
    "type_not_found": "nf",
    "type_ambiguous": "amb",
    "rejected": "rej",
    "not_registrable": "-",
    "invalid": "inv",
    "does_not_compile": "DNC",
    "crashed": "CRASH",
    "violated": "BAD",
    "exception": "exc",
}

DIMENSIONS = ("container", "scope", "form", "leaf", "binding", "request")

DEFECTS = ("does_not_compile", "crashed", "violated")


def disagreements(cells):
    """The cells one container resolves and the other does not, though both
    accept the registration."""
    outcomes = {}
    for cell in cells:
        container, rest = cell["cell"].split("/", 1)
        outcomes.setdefault(rest, {})[container] = cell["outcome"]
    found = []
    for rest, by_container in sorted(outcomes.items()):
        if len(by_container) != len(CONTAINERS) or any(
                outcome in ("not_registrable", "invalid")
                for outcome in by_container.values()):
            continue
        if len({outcome == "resolved"
                for outcome in by_container.values()}) > 1:
            found.append((rest, by_container))
    return found


def counts_table(cells, dimension):
    outcomes = [o for o in MARKS if any(c["outcome"] == o for c in cells)]
    totals = {}
    for cell in cells:
        value = cell["cell"].split("/")[DIMENSIONS.index(dimension)]
        row = totals.setdefault(value, {})
        row[cell["outcome"]] = row.get(cell["outcome"], 0) + 1
    lines = [
        f"| {dimension} | " + " | ".join(outcomes) + " |",
        "|---|" + "---:|" * len(outcomes),
    ]
    for value, row in totals.items():
        lines.append(f"| `{value}` | " +
                     " | ".join(str(row.get(o, "")) for o in outcomes) + " |")
    return lines + [""]


def write_report(path, cells):
    """Counts by dimension, a table per container, leaf and binding, and the
    compile-time outcomes with their messages."""
    table = {}
    errors = {}
    for cell in cells:
        table[cell["cell"]] = MARKS.get(cell["outcome"], cell["outcome"])
        if cell["outcome"] in ("does_not_compile", "rejected"):
            errors.setdefault((cell["outcome"], cell["error"]), []).append(
                cell["cell"])
    lines = [
        "# Model baseline",
        "",
        "Generated by test/model/probe.py. Legend: "
        + ", ".join(f"`{mark}` {outcome}" for outcome, mark in MARKS.items()),
        "",
        f"{len(cells)} cells.",
        "",
    ]
    for dimension in DIMENSIONS[:-1]:
        lines += counts_table(cells, dimension)
    for container in CONTAINERS:
        for leaf in LEAVES:
            for binding, traits in BINDINGS.items():
                lines += [
                    f"## {container} container, {leaf} leaf, {binding} "
                    "binding", "",
                    "| scope / stored | " +
                    " | ".join(f"`{r}`" for r, _ in REQUESTS) + " |",
                    "|---|" + "---|" * len(REQUESTS),
                ]
                for scope in SCOPES:
                    for form, _ in STORED:
                        unit = Registration(container, scope, form, leaf,
                                            binding)
                        row = [table.get(unit.cell(r), "?")
                               for r, _ in unit.requests()]
                        lines.append(f"| {scope} / {form} | " +
                                     " | ".join(row) + " |")
                lines.append("")
    lines += ["## Container disagreements", ""]
    lines += [
        f"- `{rest}`: " + ", ".join(f"{c} {o}" for c, o in by.items())
        for rest, by in disagreements(cells)
    ] or ["None."]
    lines += ["", "## Compile-time outcomes", ""]
    for (outcome, error), where in sorted(
            errors.items(), key=lambda item: (item[0][0], -len(item[1]))):
        lines.append(f"- {outcome}, {len(where)} cells: `{error[:200]}`")
        lines += [f"  - {cell}" for cell in where[:4]]
        if len(where) > 4:
            lines.append(f"  - ... {len(where) - 4} more")
    path.write_text("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("output", type=Path, help="the baseline JSON")
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument(
        "--flags", default="-std=c++17 -O0 -Iinclude -Itest",
        help="compiler flags, with include paths",
    )
    parser.add_argument("--jobs", type=int, default=os.cpu_count())
    parser.add_argument(
        "--only", default="",
        help="probe only the registrations whose name contains this text",
    )
    options = parser.parse_args()
    options.work.mkdir(parents=True, exist_ok=True)
    flags = shlex.split(options.flags)

    units = [unit for unit in registrations() if options.only in unit.name]
    cells = []
    with concurrent.futures.ThreadPoolExecutor(options.jobs) as executor:
        for outcomes in executor.map(
            lambda unit: probe_registration(
                options.compiler, flags, options.work, unit),
            units,
        ):
            cells += outcomes
    cells.sort(key=lambda cell: cell["cell"])
    options.output.write_text(json.dumps(cells, indent=1) + "\n")
    write_report(options.output.with_suffix(".md"), cells)

    counts = {}
    for cell in cells:
        counts[cell["outcome"]] = counts.get(cell["outcome"], 0) + 1
    print(f"{len(cells)} cells:", file=sys.stderr)
    for outcome, count in sorted(counts.items(), key=lambda item: -item[1]):
        print(f"  {count:6} {outcome}", file=sys.stderr)
    defects = [cell for cell in cells if cell["outcome"] in DEFECTS]
    disagreeing = disagreements(cells)
    for cell in defects:
        print(f"defect: {cell['cell']}: {cell['outcome']}: "
              f"{cell.get('error', '')}", file=sys.stderr)
    for rest, by_container in disagreeing:
        print(f"disagreement: {rest}: {by_container}", file=sys.stderr)
    print(f"report: {options.output.with_suffix('.md')}", file=sys.stderr)
    return 1 if defects or disagreeing else 0


if __name__ == "__main__":
    sys.exit(main())
