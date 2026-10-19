"""Verifies that the public headers form a layered hierarchy.

A header may include headers of the layers below its own and headers of its
own module. Modules that share a layer are independent of each other. The
headers directly under include/dingo form the topmost layer.
"""

import argparse
from pathlib import Path
import re
import sys

LAYERS = [
    ("base",),
    ("memory", "rtti", "type"),
    ("resolution", "lookup"),
    ("factory",),
    ("context",),
    ("storage",),
    ("registration",),
    ("binding",),
    ("runtime", "static"),
    (".",),
]

# core hosts the new implementation of the spec-driven design, see
# docs/architecture/spec-driven-design.md. Its modules are the directories under
# core, layered on their own: core includes only core, and the current
# implementation never includes core.
CORE_LAYERS = [
    ("core/base",),
    ("core/primitives",),
    ("core/vocabulary",),
    ("core/projection",),
    ("core/engine",),
    ("core/backend",),
    ("core/api",),
]

INCLUDE = re.compile(r"^\s*#\s*include\s*<dingo/([^>]+)>")


def module_of(path):
    parts = Path(path).parts
    if parts[0] == "core":
        return "/".join(parts[:2]) if len(parts) > 2 else "core"
    return parts[0] if len(parts) > 1 else "."


def is_core(module):
    return module == "core" or module.startswith("core/")


def layer_of(module):
    layers = CORE_LAYERS if is_core(module) else LAYERS
    for index, modules in enumerate(layers):
        if module in modules:
            return index
    return None


def check(include_directory):
    root = Path(include_directory) / "dingo"
    violations = []
    for header in sorted(root.rglob("*")):
        if header.suffix not in (".h", ".hpp"):
            continue
        source = header.relative_to(root)
        source_module = module_of(source)
        source_layer = layer_of(source_module)
        if source_layer is None:
            violations.append(f"{source}: module '{source_module}' has no layer")
            continue
        for number, line in enumerate(header.read_text().splitlines(), 1):
            match = INCLUDE.match(line)
            if not match:
                continue
            target = match.group(1)
            target_module = module_of(target)
            target_layer = layer_of(target_module)
            if is_core(source_module) != is_core(target_module):
                violations.append(
                    f"{source}:{number}: {source_module} must not include {target}: "
                    "the current and the next implementation are independent"
                )
                continue
            if target_layer is None:
                violations.append(
                    f"{source}:{number}: includes {target} of module "
                    f"'{target_module}' that has no layer"
                )
            elif target_layer > source_layer or (
                target_layer == source_layer and target_module != source_module
            ):
                violations.append(
                    f"{source}:{number}: {source_module} (layer {source_layer}) "
                    f"must not include {target} ({target_module}, "
                    f"layer {target_layer})"
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
