"""Verifies that the headers form a layered hierarchy.

The library is under include/dingo/core, whose modules are the directories
under core, layered as LAYERS lists them: a header may include headers of the
layers below its own and headers of its own module, and modules that share a
layer are independent of each other. The public include paths, every other
header under include/dingo, form the topmost layer: each includes the facade
it forwards to in the API layer, and core never includes one.
"""

import argparse
from pathlib import Path
import re
import sys

# The modules of the library, from the lowest layer to the highest. See
# docs/architecture/overview.md, Header Layers.
LAYERS = [
    ("core/base",),
    ("core/primitives",),
    ("core/vocabulary",),
    ("core/projection",),
    ("core/engine",),
    ("core/backend",),
    ("core/api",),
]

API = "core/api"

INCLUDE = re.compile(r"^\s*#\s*include\s*<dingo/([^>]+)>")


def module_of(path):
    parts = Path(path).parts
    if parts[0] == "core":
        return "/".join(parts[:2]) if len(parts) > 2 else "core"
    return "public"


def layer_of(module):
    for index, modules in enumerate(LAYERS):
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
        if source_module != "public" and source_layer is None:
            violations.append(f"{source}: module '{source_module}' has no layer")
            continue
        for number, line in enumerate(header.read_text().splitlines(), 1):
            match = INCLUDE.match(line)
            if not match:
                continue
            target = match.group(1)
            target_module = module_of(target)
            if source_module == "public":
                if target_module != API:
                    violations.append(
                        f"{source}:{number}: the public include path must not "
                        f"include {target}: it forwards to the API layer"
                    )
                continue
            if target_module == "public":
                violations.append(
                    f"{source}:{number}: {source_module} must not include the "
                    f"public include path {target}"
                )
                continue
            target_layer = layer_of(target_module)
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
