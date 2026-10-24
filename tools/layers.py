"""Verifies that the headers form the component structure of the library.

The library is a set of components, each a directory of include/dingo that holds
the generated declarations, the extension points, and the functions of one
subject, see docs/architecture/overview.md. A component may include only
headers of itself and of the components it depends on, which COMPONENTS lists,
so the dependencies form a graph without cycles. The public headers, every
header that no component holds, form the top component and may include any
other.

A header that has not moved into the directory of its component yet is listed in
MOVING with the component it belongs to; the entry goes away when the header
moves. An include that is a known violation of the structure, which a planned
change removes, is listed in KNOWN.
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

# The headers that are not in the directory of their component yet.
MOVING = {
    "conversion/storage_forms.gen": "storage",
    "conversion/scopes.gen": "storage",
    "conversion/construct": "factory",
    "conversion/route": "conversion",
    "conversion/convert": "conversion",
    "conversion/selection_rules.gen": "selection",
    "conversion/request": "selection",
    "backend/selection": "selection",
    "backend/diagnose": "selection",
    "backend/slot": "backend",
    "backend/memo": "backend",
    "backend/local": "backend",
    "backend/static_backend": "backend",
    "backend/runtime_backend": "backend",
    # Public headers that are in the directory of a component and include
    # registration.h, so they belong to the top component.
    "storage/external": PUBLIC,
    "storage/shared": PUBLIC,
    "storage/shared_cyclical": PUBLIC,
    "storage/unique": PUBLIC,
    "factory/constructor": PUBLIC,
    "lookup/lookup": PUBLIC,
}

# Includes that violate the structure and that a planned change removes.
KNOWN = set()

INCLUDE = re.compile(r"^\s*#\s*include\s*<dingo/([^>]+)\.(?:h|hpp)>")


def component_of(stem):
    if stem in MOVING:
        return MOVING[stem]
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
