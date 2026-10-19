"""Verifies that the public headers form a layered hierarchy.

A header may include headers of the layers below its own and headers of its
own module. Modules that share a layer are independent of each other. The
headers directly under include/dingo form the topmost layer. Headers must not
include each other in a cycle, must stay below a size limit so that a header
keeps to a single concept, and must not repeat the name of their folder.
"""

import argparse
from pathlib import Path
import re
import sys

LAYERS = [
    ("base",),
    ("memory", "rtti", "type"),
    ("key", "conversion"),
    ("dependency",),
    ("resolution", "lookup"),
    ("factory",),
    ("context",),
    ("exposure",),
    ("storage",),
    ("registration",),
    ("binding",),
    ("runtime", "static"),
    ("container",),
    (".",),
]

# Headers whose name is that of the concept they define and starts with the
# folder, as the entry point of a documented extension point.
NAME_EXCEPTIONS = {"type/type_traits.h"}
MAX_LINES = 800
# Headers that are one cohesive class and exceed the limit.
MAX_LINES_EXCEPTIONS = {"container.h": 1100}

INCLUDE = re.compile(r"^\s*#\s*include\s*<dingo/([^>]+)>")


def module_of(path):
    parts = Path(path).parts
    return parts[0] if len(parts) > 1 else "."


def layer_of(module):
    for index, modules in enumerate(LAYERS):
        if module in modules:
            return index
    return None


def find_cycles(graph):
    """Returns the include cycles of the graph as strongly connected components."""
    index = {}
    lowlink = {}
    stack = []
    on_stack = set()
    cycles = []
    counter = [0]

    def visit(node):
        index[node] = lowlink[node] = counter[0]
        counter[0] += 1
        stack.append(node)
        on_stack.add(node)
        for target in graph.get(node, ()):
            if target not in index:
                visit(target)
                lowlink[node] = min(lowlink[node], lowlink[target])
            elif target in on_stack:
                lowlink[node] = min(lowlink[node], index[target])
        if lowlink[node] == index[node]:
            component = []
            while True:
                member = stack.pop()
                on_stack.discard(member)
                component.append(member)
                if member == node:
                    break
            if len(component) > 1:
                cycles.append(sorted(component))

    for node in sorted(graph):
        if node not in index:
            visit(node)
    return cycles


def check(include_directory):
    root = Path(include_directory) / "dingo"
    violations = []
    graph = {}
    for header in sorted(root.rglob("*")):
        if header.suffix not in (".h", ".hpp"):
            continue
        source = header.relative_to(root)
        source_module = module_of(source)
        source_layer = layer_of(source_module)
        lines = header.read_text().splitlines()
        limit = MAX_LINES_EXCEPTIONS.get(str(source), MAX_LINES)
        if len(lines) > limit:
            violations.append(
                f"{source}: {len(lines)} lines exceed the limit of {limit}"
            )
        graph[str(source)] = set()
        if (
            source.parent != Path(".")
            and source.stem.startswith(source.parts[0] + "_")
            and str(source) not in NAME_EXCEPTIONS
        ):
            violations.append(
                f"{source}: the name repeats the folder, the folder already "
                "gives the context"
            )
        if source_layer is None:
            violations.append(f"{source}: module '{source_module}' has no layer")
            continue
        for number, line in enumerate(lines, 1):
            match = INCLUDE.match(line)
            if not match:
                continue
            target = match.group(1)
            graph[str(source)].add(target)
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
    for cycle in find_cycles(graph):
        violations.append("include cycle: " + " -> ".join(cycle))
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
