#!/usr/bin/env python3
"""Checks the scope descriptors of the spec against the baseline.

spec/content/scopes.py records the storage scope policies of the baseline, the
library of the commit in test/differential/baseline-commit. This script generates a program that prints, for every scope of
the spec, the properties of `dingo::storage_scope_policy` and the request
categories it exposes for every storage kind, compiles and runs it, and
compares the output with the spec. Every difference must be declared by a
behavior change of the spec whose `cases` match `scopes:<scope>/<property>`,
and every behavior change with a `scopes:` pattern must match a difference.
The reference form is held only by external storage and has no storage_kind
of its own in the implementation, so it is not compared.
"""

import argparse
import fnmatch
import os
import shlex
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
# Importing the spec writes no bytecode into the source tree.
sys.dont_write_bytecode = True
sys.path.insert(0, str(PROJECT_ROOT))

import spec  # noqa: E402
from spec.schema import (  # noqa: E402
    RecursionGuard,
    RequestCategory,
    SourceAccess,
    ValueCategory,
)

# The storage_kind enumerators of the baseline. The spec names
# the form of a storage-held array of runtime size unbounded_array.
CPP_FORMS = {
    "alternative": "alternative",
    "bounded_array": "bounded_array",
    "optional": "optional",
    "plain": "plain",
    "pointer": "pointer",
    "shared_array_handle": "shared_array_handle",
    "shared_handle": "shared_handle",
    "unbounded_array": "array",
    "unique_array_handle": "unique_array_handle",
    "unique_handle": "unique_handle",
}

CATEGORIES = {
    RequestCategory.VALUE: "value",
    RequestCategory.LVALUE: "lvalue",
    RequestCategory.RVALUE: "rvalue",
    RequestCategory.POINTER: "pointer",
}

GUARDS = {
    "none": RecursionGuard.NONE,
    "while_unresolved": RecursionGuard.WHILE_UNRESOLVED,
    "always": RecursionGuard.ALWAYS,
}


def program(loaded):
    headers = sorted({p.header for p in loaded.cpp.scope_projections})
    lines = [f"#include <{header}>" for header in headers]
    lines += [
        "#include <dingo/storage/storage_scope_policy.h>",
        "",
        "#include <cstdio>",
        "",
        "namespace {",
        "const char *guard_name(dingo::materialization_guard guard) {",
        "  switch (guard) {",
        "  case dingo::materialization_guard::none: return \"none\";",
        "  case dingo::materialization_guard::while_unresolved:",
        "    return \"while_unresolved\";",
        "  case dingo::materialization_guard::always: return \"always\";",
        "  }",
        "  return \"unknown\";",
        "}",
        "",
        "template <typename Tag> void print(const char *scope) {",
        "  using policy = dingo::storage_scope_policy<Tag>;",
        "  std::printf(\"%s stable %d\\n\", scope, policy::is_stable ? 1 : 0);",
        "  std::printf(\"%s consumes %d\\n\", scope, policy::consumes ? 1 : 0);",
        "  std::printf(\"%s guard %s\\n\", scope, guard_name(policy::guard));",
        "  std::printf(\"%s retains %d\\n\", scope,",
        "              policy::retains_unresolved_source ? 1 : 0);",
        "  struct form { const char *name; dingo::storage_kind value; };",
        "  const form forms[] = {",
    ]
    lines += [
        f"    {{\"{form}\", dingo::storage_kind::{cpp}}},"
        for form, cpp in sorted(CPP_FORMS.items())
    ]
    lines += [
        "  };",
        "  for (const form &f : forms) {",
        "    unsigned exposed = static_cast<unsigned>(policy::exposed(f.value));",
        "    std::printf(\"%s form %s %u\\n\", scope, f.name, exposed);",
        "  }",
        "}",
        "} // namespace",
        "",
        "int main() {",
    ]
    lines += [
        f"  print<{p.spelling}>(\"{p.scope}\");"
        for p in sorted(loaded.cpp.scope_projections, key=lambda p: p.scope)
    ]
    lines += ["  return 0;", "}", ""]
    return "\n".join(lines)


# The bits of dingo::storage_exposure.
EXPOSURE_BITS = {"value": 1, "lvalue": 2, "rvalue": 4, "pointer": 8}


def expected(loaded):
    result = {}
    for scope in loaded.scopes:
        result[(scope.id, "stable")] = "1" if scope.stable else "0"
        consumes = scope.source_access is SourceAccess.CONSUME
        result[(scope.id, "consumes")] = "1" if consumes else "0"
        result[(scope.id, "guard")] = scope.guard.value
        result[(scope.id, "retains")] = "1" if scope.retains_unresolved_source else "0"
        exposed = {e.form: e.categories for e in scope.exposures if e.form != "reference"}
        for form in CPP_FORMS:
            bits = sum(
                EXPOSURE_BITS[CATEGORIES[category]]
                for category in exposed.get(form, ())
            )
            result[(scope.id, f"form {form}")] = str(bits)
    return result


def consistency(loaded):
    """Checks properties of the descriptors that do not need the compiler."""
    errors = []
    for scope in loaded.scopes:
        consumes = scope.source_access is SourceAccess.CONSUME
        rvalue = scope.materializes is ValueCategory.RVALUE
        if consumes != rvalue:
            errors.append(
                f"{scope.id}: a scope materializes an rvalue exactly when it "
                "consumes its source"
            )
        if set(GUARDS.values()) and scope.guard not in GUARDS.values():
            errors.append(f"{scope.id}: unknown guard {scope.guard}")
    forms = {form.id for form in loaded.storage_kinds} - {"reference"}
    if forms != set(CPP_FORMS):
        errors.append(
            f"storage kinds {sorted(forms)} differ from the implementation's "
            f"{sorted(CPP_FORMS)}"
        )
    return errors


def observed(output):
    result = {}
    for line in output.splitlines():
        scope, rest = line.split(" ", 1)
        key, value = rest.rsplit(" ", 1)
        result[(scope, key)] = value
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--cxx-command",
        required=True,
        help="compiler command including the standard and the include flag of "
        "the include directory of the baseline",
    )
    parser.add_argument(
        "--work-dir",
        type=Path,
        required=True,
        help="directory for the generated program, such as build/spec-check",
    )
    args = parser.parse_args()

    loaded = spec.load_spec()
    errors = consistency(loaded)

    args.work_dir.mkdir(parents=True, exist_ok=True)
    source = args.work_dir / "scopes.cpp"
    executable = args.work_dir / "scopes"
    source.write_text(program(loaded))
    command = [*shlex.split(args.cxx_command), str(source), "-o", str(executable)]
    compiled = subprocess.run(command, capture_output=True, text=True)
    if compiled.returncode != 0:
        print(f"compilation failed: {shlex.join(command)}", file=sys.stderr)
        print(compiled.stderr, file=sys.stderr)
        return 1
    output = subprocess.run(
        [str(executable)], check=True, capture_output=True, text=True
    ).stdout

    want = expected(loaded)
    have = observed(output)
    changes = [
        (change.id, [p for p in change.cases if p.startswith("scopes:")])
        for change in loaded.behavior_changes
    ]
    matched = set()
    for key in sorted(want.keys() | have.keys()):
        if want.get(key) == have.get(key):
            continue
        scope, what = key
        path = f"scopes:{scope}/{what}"
        owners = [
            change_id
            for change_id, patterns in changes
            if any(fnmatch.fnmatchcase(path, p) for p in patterns)
        ]
        matched.update(owners)
        if not owners:
            errors.append(
                f"{scope}: {what}: spec {want.get(key)}, implementation {have.get(key)}"
            )
    for change_id, patterns in changes:
        if patterns and change_id not in matched:
            errors.append(f"stale behavior change: {change_id}")
    for error in errors:
        print(error, file=sys.stderr)
    if errors:
        return 1
    print(
        f"{len(loaded.scopes)} scopes match the implementation up to "
        f"{len(matched)} declared behavior changes"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
