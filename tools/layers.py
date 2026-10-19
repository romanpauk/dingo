"""Verifies that the public headers form a layered hierarchy.

A header may include headers of the layers below its own and headers of its
own module. Modules that share a layer are independent of each other. The
headers directly under include/dingo form the topmost layer.

The runtime and static modules solve the same problems with different means,
so they are kept mirrored: a header in one has a header of the same name in the
other. The headers that exist on one side only are listed in ONE_SIDED.
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

MIRRORED = ("runtime", "static")

# Headers without a counterpart: the runtime side owns its storage and rolls it
# back, the static side proves its resolution graph at compile time.
ONE_SIDED = {
    "runtime": {"container_runtime.h", "lookup_index.h", "transaction.h"},
    "static": {"graph.h"},
}

INCLUDE = re.compile(r"^\s*#\s*include\s*<dingo/([^>]+)>")


def module_of(path):
    parts = Path(path).parts
    return parts[0] if len(parts) > 1 else "."


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


def check_mirror(include_directory):
    root = Path(include_directory) / "dingo"
    headers = {
        module: {
            path.name
            for path in (root / module).iterdir()
            if path.suffix in (".h", ".hpp")
        }
        for module in MIRRORED
    }
    violations = []
    for module, other in zip(MIRRORED, reversed(MIRRORED)):
        for name in sorted(headers[module] - headers[other] - ONE_SIDED[module]):
            violations.append(
                f"{module}/{name}: has no counterpart {other}/{name}; "
                f"add it or list it in ONE_SIDED"
            )
        for name in sorted(ONE_SIDED[module] - headers[module]):
            violations.append(f"{module}/{name}: listed in ONE_SIDED but missing")
        for name in sorted(ONE_SIDED[module] & headers[other]):
            violations.append(
                f"{module}/{name}: listed in ONE_SIDED but {other}/{name} exists"
            )
    return violations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("include_directory")
    arguments = parser.parse_args()
    violations = check(arguments.include_directory)
    violations += check_mirror(arguments.include_directory)
    for violation in violations:
        print(violation)
    if violations:
        print(f"{len(violations)} layering violation(s)")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
