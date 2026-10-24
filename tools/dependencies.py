"""Verifies that the headers form the component structure of the library.

The library is a set of components, each a directory of include/dingo that holds
the generated declarations, the extension points, and the functions of one
subject, see docs/architecture/overview.md. A component may include only
headers of itself and of the components it depends on, which COMPONENTS lists,
so the dependencies form a graph without cycles. The public headers, every
header that no component holds, form the top component and may include any
other.

A public header in the directory of a component that includes registration.h
belongs to the top component, and IN_COMPONENT_DIRECTORIES lists it. An include
that is a known violation of the structure, which a planned change removes, is
listed in KNOWN.
"""

import argparse
from pathlib import Path
import re
import sys

PUBLIC = "public"

# The components, with the components each may depend on.
COMPONENTS = {
    "base": (),
    "diagnostics": ("base",),
    "memory": ("base",),
    "constructor": ("base",),
    "type": ("base", "diagnostics", "constructor"),
    "lookup": ("base", "diagnostics", "type"),
    "storage": ("base", "type"),
    "factory": ("base", "diagnostics", "type", "constructor"),
    "conversion": (
        "base",
        "diagnostics",
        "memory",
        "type",
        "lookup",
        "storage",
        "factory",
    ),
    "selection": (
        "base",
        "diagnostics",
        "memory",
        "constructor",
        "type",
        "lookup",
        "storage",
        "factory",
        "conversion",
    ),
    "backend": (
        "base",
        "diagnostics",
        "memory",
        "constructor",
        "type",
        "lookup",
        "storage",
        "factory",
        "conversion",
        "selection",
    ),
}

# The public headers that are in the directory of a component and include
# registration.h, so they belong to the top component.
IN_COMPONENT_DIRECTORIES = {
    "storage/external",
    "storage/shared",
    "storage/shared_cyclical",
    "storage/unique",
    "factory/constructor",
    "lookup/lookup",
}

# Includes that violate the structure and that a planned change removes.
KNOWN = set()

INCLUDE = re.compile(r"^\s*#\s*include\s*<dingo/([^>]+)\.(?:h|hpp)>")


def component_of(stem):
    if stem in IN_COMPONENT_DIRECTORIES:
        return PUBLIC
    parts = Path(stem).parts
    if len(parts) > 1 and parts[0] in COMPONENTS:
        return parts[0]
    return PUBLIC


def allowed(source, target):
    return (
        source == target
        or source == PUBLIC
        or target in COMPONENTS.get(source, ())
    )


def check(include_directory):
    root = Path(include_directory) / "dingo"
    violations = []
    for header in sorted(root.rglob("*")):
        if header.suffix not in (".h", ".hpp"):
            continue
        stem = header.relative_to(root).with_suffix("").as_posix()
        source = component_of(stem)
        for number, line in enumerate(header.read_text().splitlines(), 1):
            match = INCLUDE.match(line)
            if not match:
                continue
            target = match.group(1)
            if allowed(source, component_of(target)) or (stem, target) in KNOWN:
                continue
            violations.append(
                f"{stem}.h:{number}: {source} must not include {target}.h "
                f"({component_of(target)})"
            )
    return violations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("include_directory")
    arguments = parser.parse_args()
    violations = check(arguments.include_directory)
    for violation in violations:
        print(violation)
    if violations:
        print(f"{len(violations)} layering violation(s)")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
