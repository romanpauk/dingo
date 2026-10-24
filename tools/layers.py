"""Verifies that the headers form a layered hierarchy.

The modules of the library are the directories under include/dingo that LAYERS
lists, from the lowest layer to the highest. The public headers, every other
header under include/dingo, form the topmost layer: they hold the public include
paths, and each may include the others. A header may include headers of the
layers below its own and headers of its own module, and modules that share a
layer are independent of each other, so no lower module includes the public
headers.
"""

import argparse
from pathlib import Path
import re
import sys

# The modules of the library, from the lowest layer to the highest. See
# docs/architecture/overview.md, Header Layers. PUBLIC is the module of the
# headers that no other entry lists.
PUBLIC = "public"
LAYERS = [
    ("base",),
    ("primitives",),
    ("conversion",),
    ("backend",),
    (PUBLIC,),
]

INCLUDE = re.compile(r"^\s*#\s*include\s*<dingo/([^>]+)>")


def module_of(path):
    parts = Path(path).parts
    if len(parts) > 1 and any(parts[0] in modules for modules in LAYERS):
        return parts[0]
    return PUBLIC


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
