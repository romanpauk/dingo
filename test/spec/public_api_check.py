#!/usr/bin/env python3
"""Compiles the public API recorded in the spec against the library.

The public API entries and their C++ bindings (see spec/content/public_api.py
and spec/content/cpp_api_bindings.py, and docs/architecture/spec-driven-design.md)
are turned into translation units that are compiled with -fsyntax-only:

- include paths: one translation unit per include path that contains only that
  include, so every public header is self-contained
- names and extension points: one translation unit that includes the declaring
  headers and uses every public name and extension point as a type, a template,
  or an expression, as its binding says
- configuration macros: one translation unit that checks that every macro is
  defined after including dingo/container.h

Every entry compiles against the library. With --implementation baseline, the
command names the include directory of the baseline, the commit of
test/differential/baseline-commit, which lacks the entries a behavior change
declares with an `api:` case pattern, and the check requires them to fail
there.

A failure is reported with the id of the public API entry. When a combined
translation unit fails, its entries are compiled one by one to find the ones
that fail. The exit status is nonzero when anything fails. The units are
written to --work-dir, so a failing unit can be compiled by hand.
"""

import argparse
import concurrent.futures
import fnmatch
import os
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

sys.dont_write_bytecode = True

PROJECT_ROOT = Path(__file__).resolve().parents[2]
# Importing the spec writes no bytecode into the source tree.
sys.dont_write_bytecode = True
sys.path.insert(0, str(PROJECT_ROOT))

import spec as dingo_spec  # noqa: E402
from spec.schema import ApiKind  # noqa: E402

CONFIGURATION_HEADER = "dingo/container.h"
NAME_PLACEHOLDER = "{name}"
MAX_REPORTED_LINES = 12

STANDARD_HEADERS = (
    "cstddef",
    "functional",
    "map",
    "memory",
    "new",
    "optional",
    "string",
    "type_traits",
    "typeindex",
    "utility",
    "variant",
    "vector",
)


@dataclass(frozen=True)
class Unit:
    """The code that checks one public API entry."""

    api: str
    spelling: str
    code: str


@dataclass(frozen=True)
class Failure:
    api: str
    spelling: str
    source: Path
    output: str


def default_cxx_command():
    return os.environ.get("CXX", "c++") + f" -std=c++17 -I{PROJECT_ROOT / 'include'}"


def syntax_only_flags(command):
    name = Path(command[0]).name.lower()
    if name in ("cl", "cl.exe"):
        return ["/nologo", "/EHsc", "/Zs", "/Tp"]
    return ["-fsyntax-only", "-x", "c++"]


def compile_syntax(command, source):
    result = subprocess.run(
        [*command, *syntax_only_flags(command), str(source)],
        capture_output=True,
        text=True,
    )
    return result.returncode == 0, result.stdout + result.stderr


def include_directive(path):
    return f"#include <{path}>\n"


def macro_code(api, macro):
    return (
        f"#if !defined({macro})\n"
        f'#error "public API entry {api}: macro {macro} is not defined"\n'
        f"#endif\n"
    )


def name_code(binding):
    """The declarations of a binding, and a function that uses its name."""

    def expand(text):
        return text.replace(NAME_PLACEHOLDER, binding.spelling).strip("\n")

    code = []
    if binding.declarations is not None:
        code.append(expand(binding.declarations))
    if binding.usage is not None:
        code.append(f"inline void api_check_{binding.api}() {{\n{expand(binding.usage)}\n}}")
    return "\n".join(code) + "\n"


def collect_units(loaded):
    """Returns (include paths, code units, macro units) of the spec."""
    entries = {entry.id: entry for entry in loaded.public_api}
    includes, code, macros = [], [], []
    for binding in loaded.cpp.api_bindings:
        kind = entries[binding.api].kind
        if kind is ApiKind.INCLUDE_PATH:
            includes.append(Unit(binding.api, binding.spelling, include_directive(binding.spelling)))
        elif kind is ApiKind.CONFIGURATION_MACRO:
            macros.append(
                Unit(binding.api, binding.spelling, macro_code(binding.api, binding.spelling))
            )
        else:
            code.append(Unit(binding.api, binding.spelling, name_code(binding)))
    return includes, code, macros


def declaring_headers(loaded):
    return sorted(
        {binding.header for binding in loaded.cpp.api_bindings if binding.header is not None}
    )


def names_prelude(headers):
    return "".join(include_directive(header) for header in (*headers, *STANDARD_HEADERS))


def combined_source(prelude, units):
    sections = [f"// public API entry {unit.api}: {unit.spelling}\n{unit.code}" for unit in units]
    return prelude + "\n" + "\n".join(sections)


def macros_prelude():
    return include_directive(CONFIGURATION_HEADER)


def trimmed(output):
    lines = [line for line in output.splitlines() if line.strip()]
    errors = [line for line in lines if "error" in line]
    shown = (errors or lines)[:MAX_REPORTED_LINES]
    more = len(errors or lines) - len(shown)
    if more > 0:
        shown.append(f"... and {more} more lines")
    return "\n".join(shown)


class Checker:
    def __init__(self, command, work_dir, jobs):
        self.command = command
        self.work_dir = work_dir
        self.jobs = jobs
        self.pool = concurrent.futures.ThreadPoolExecutor(max_workers=jobs)

    def write(self, name, text):
        path = self.work_dir / name
        path.write_text(text)
        return path

    def compile_units(self, prefix, prelude, units):
        """Compiles each unit alone and returns the failures."""
        sources = [
            (unit, self.write(f"{prefix}-{unit.api}.cpp", prelude + "\n" + unit.code))
            for unit in units
        ]
        results = list(self.pool.map(lambda item: compile_syntax(self.command, item[1]), sources))
        return [
            Failure(unit.api, unit.spelling, source, output)
            for (unit, source), (ok, output) in zip(sources, results)
            if not ok
        ]

    def check_includes(self, includes):
        return self.compile_units("include", "", includes)

    def check_combined(self, prefix, prelude, units):
        """Compiles all units in one translation unit, and one by one if that fails."""
        if not units:
            return []
        source = self.write(f"{prefix}.cpp", combined_source(prelude, units))
        ok, output = compile_syntax(self.command, source)
        if ok:
            return []
        failures = self.compile_units(f"{prefix}-unit", prelude, units)
        if not failures:
            failures = [Failure(prefix, "combined translation unit", source, output)]
        return failures


def changed_entries(loaded):
    """{entry id: behavior change id} of the entries a behavior change declares
    with an `api:<entry id>` case pattern.

    The spec records the target public API. A changed entry is not provided by
    the baseline, so the check requires it to fail there.
    """
    changed = {}
    for change in loaded.behavior_changes:
        patterns = [p[len("api:") :] for p in change.cases if p.startswith("api:")]
        for entry in loaded.public_api:
            if any(fnmatch.fnmatchcase(entry.id, pattern) for pattern in patterns):
                changed[entry.id] = change.id
    return changed


def check(loaded, command, work_dir, jobs, implementation="next"):
    """Returns (number of entries checked, failures). The library provides the
    target public API, so every entry, the changed ones too, must compile
    against it; the baseline lacks the changed ones."""
    includes, code, macros = collect_units(loaded)
    changed = changed_entries(loaded) if implementation == "baseline" else {}
    work_dir.mkdir(parents=True, exist_ok=True)
    checker = Checker(command, work_dir, jobs)
    prelude = names_prelude(declaring_headers(loaded))
    kept = lambda units: [unit for unit in units if unit.api not in changed]  # noqa: E731
    failures = []
    failures += checker.check_includes(kept(includes))
    failures += checker.check_combined("names", prelude, kept(code))
    failures += checker.check_combined("macros", macros_prelude(), kept(macros))
    # A changed entry must not be provided yet, or its behavior change is stale.
    changed_units = [unit for unit in (*includes, *code, *macros) if unit.api in changed]
    still_provided = {
        unit.api for unit in changed_units
    } - {failure.api for failure in checker.compile_units("changed", prelude, changed_units)}
    for api in sorted(still_provided):
        failures.append(
            Failure(api, "", None, f"stale behavior change {changed[api]}: the entry is provided")
        )
    return len(includes) + len(code) + len(macros), failures


def report(failures):
    for failure in failures:
        print(f"FAIL {failure.api} ({failure.spelling}): {failure.source}", file=sys.stderr)
        for line in trimmed(failure.output).splitlines():
            print(f"    {line}", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--cxx-command",
        default=default_cxx_command(),
        help="compiler command including the standard and include flags "
        "(default: $CXX -std=c++17 with the include paths of the library)",
    )
    parser.add_argument(
        "--work-dir",
        type=Path,
        required=True,
        help="directory for the generated translation units, such as build/public-api",
    )
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 8))
    parser.add_argument(
        "--implementation",
        choices=("next", "baseline"),
        default="next",
        help="baseline checks the public API of the baseline, given by "
        "--cxx-command, which lacks the entries a behavior change declares",
    )
    args = parser.parse_args()

    try:
        loaded = dingo_spec.load_spec()
    except dingo_spec.SpecError as error:
        print(f"the spec is invalid:\n{error}", file=sys.stderr)
        return 2
    command = shlex.split(args.cxx_command)
    checked, failures = check(
        loaded, command, args.work_dir, args.jobs, args.implementation
    )
    if failures:
        report(failures)
        print(f"public API check failed: {len(failures)} of {checked} entries", file=sys.stderr)
        return 1
    print(f"public API check passed: {checked} entries")
    return 0


if __name__ == "__main__":
    sys.exit(main())
