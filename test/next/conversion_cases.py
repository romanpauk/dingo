#!/usr/bin/env python3
"""Generates the conversion cases of the next implementation from the reference.

For every scope, leaf kind, declared interfaces, stored type_tree of up to two
wrappers in the standard and a user wrapper family, and request, and for a bare
leaf, requests of three wrappers, the reference
implementation in spec/reference/routes.py computes whether the binding
registers, whether the request finds it, whether it is served, and by which
rules. test/next/templates/conversion_cases.cpp.j2 lays the cases out as C++, and
test/next/conversion/conversion_conformance.cpp requires the C++ routes to compute the
same. The generated source is written into the build tree; it is not
committed.

Usage:
    conversion_cases.py --output FILE
"""

import argparse
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
# Importing the spec writes no bytecode into the source tree.
sys.dont_write_bytecode = True
sys.path.insert(0, str(PROJECT_ROOT))

import jinja2  # noqa: E402

import spec  # noqa: E402
from spec.reference.routes import (  # noqa: E402
    Leaf,
    Model,
    Request,
    RequestKind,
    TypeTree,
    resolve,
)

TEMPLATES = Path(__file__).resolve().parent / "templates"

LEAF = Leaf("leaf")
MOVE_ONLY = Leaf("move_only_leaf", copyable=False)
OTHER = Leaf("other_leaf")
IMPL = Leaf("impl", bases=("iface", "other_iface"))
IFACE = Leaf("iface", copyable=False, constructible=False)
OTHER_IFACE = Leaf("other_iface", copyable=False, constructible=False)

# (leaf, declared interfaces)
BINDING_LEAVES = (
    (LEAF, ()),
    (MOVE_ONLY, ()),
    (IMPL, (IFACE,)),
    (IMPL, (IFACE, OTHER_IFACE)),
)
FAMILIES = ("std", "user")
WRAPPERS = ("own", "share", "opt")
SCOPES = ("external", "shared", "shared_cyclical", "unique")
KINDS = (RequestKind.VALUE, RequestKind.LVALUE_REFERENCE, RequestKind.RVALUE_REFERENCE)


def leaf(value):
    return TypeTree(leaf=value)


def wrapper(kind, *args, family=None, extent=None):
    return TypeTree(wrapper=kind, args=args, family=family, extent=extent)


def stored_type_trees(value):
    base = leaf(value)
    yield base
    yield wrapper("ptr", base)
    yield wrapper("ref", base)
    yield wrapper("array", base, extent=2)
    yield wrapper("alt", base, leaf(OTHER), family="std")
    for family in FAMILIES:
        for outer in WRAPPERS:
            yield wrapper(outer, base, family=family)
            if outer != "opt":
                yield wrapper(outer, wrapper("array_unbounded", base), family=family)
            for inner in WRAPPERS:
                yield wrapper(outer, wrapper(inner, base, family="std"), family=family)


def chain(type_tree):
    while type_tree.wrapper is not None:
        yield type_tree
        type_tree = type_tree.args[0]
    yield type_tree


def request_type_trees(stored, interfaces):
    value = list(chain(stored))[-1]
    leaves = [value, *[leaf(interface) for interface in interfaces]]
    type_trees = set(chain(stored))
    for target in leaves:
        type_trees.add(target)
        for family in FAMILIES:
            for semantics in WRAPPERS:
                if semantics == "opt" and not target.leaf.constructible:
                    continue
                type_trees.add(wrapper(semantics, target, family=family))
    type_trees = {s for s in type_trees if s.wrapper != "array_unbounded"}
    return sorted(type_trees | {wrapper("ptr", s) for s in type_trees if s.wrapper != "ref"}, key=repr)


def deep_request_type_trees(stored, interfaces):
    """Requests that wrap the leaf of a binding of a bare leaf three times,
    which a request had to create more than two wrappers for."""
    if stored.wrapper is not None:
        return []
    targets = [stored, *[leaf(interface) for interface in interfaces]]
    type_trees = []
    for target in targets:
        for first in WRAPPERS:
            for second in WRAPPERS:
                for third in WRAPPERS:
                    if "opt" in (first, second, third) and not target.leaf.constructible:
                        continue
                    type_trees.append(
                        wrapper(
                            first,
                            wrapper(second, wrapper(third, target, family="std"), family="std"),
                            family="std",
                        )
                    )
    return type_trees


def as_data(type_tree):
    return {
        "wrapper": type_tree.wrapper,
        "family": type_tree.family,
        "leaf": type_tree.leaf.name if type_tree.leaf else None,
        "extent": type_tree.extent,
        "args": [as_data(arg) for arg in type_tree.args],
    }


def valid_request(type_tree, kind):
    if type_tree.wrapper == "ref":
        return False
    abstract = type_tree.wrapper is None and not type_tree.leaf.constructible
    return not (abstract and kind != RequestKind.LVALUE_REFERENCE)


def steps_of(model):
    """The operation and the C++ tag of the kind of each route id."""
    tags = {p.semantics: p.spelling for p in model.spec.cpp.semantics_tags}
    kinds = {
        form.id: kind.id for kind in model.spec.wrapper_semantics for form in kind.operations
    }
    return {
        route.id: {
            "form": route.operation.value if route.operation is not None else route.id,
            "kind": tags[kinds[route.id]] if route.id in kinds else None,
        }
        for route in model.rules
    }


def cases(model):
    steps = steps_of(model)
    for scope in SCOPES:
        for value, interfaces in BINDING_LEAVES:
            for stored in stored_type_trees(value):
                for request_type_tree in [
                    *request_type_trees(stored, interfaces),
                    *deep_request_type_trees(stored, interfaces),
                ]:
                    for kind in KINDS:
                        if not valid_request(request_type_tree, kind):
                            continue
                        request = Request(kind, request_type_tree)
                        outcome = resolve(model, scope, stored, request, interfaces)
                        yield {
                            "scope": scope,
                            "interfaces": [i.name for i in interfaces],
                            "stored": as_data(stored),
                            "kind": kind,
                            "request": as_data(request_type_tree),
                            "registered": outcome.registered,
                            "found": outcome.found,
                            "served": outcome.served,
                            "rules": [steps[rule] for rule in outcome.rules],
                        }


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    environment = jinja2.Environment(
        loader=jinja2.FileSystemLoader(str(TEMPLATES)),
        undefined=jinja2.StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
        keep_trailing_newline=True,
    )
    model = Model(spec.load_spec())
    rendered = environment.get_template("conversion_cases.cpp.j2").render(
        cases=list(cases(model))
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != rendered:
        args.output.write_text(rendered)
    return 0


if __name__ == "__main__":
    sys.exit(main())
