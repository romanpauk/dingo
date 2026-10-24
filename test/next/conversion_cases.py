#!/usr/bin/env python3
"""Generates the engine cases of the next implementation from the reference.

For every scope, leaf kind, declared interfaces, stored shape of up to two
layers in the standard and a user wrapper family, and request, the reference
implementation in spec/reference/routes.py computes whether the binding
registers, whether the request finds it, whether it is served, and by which
rules. test/next/templates/conversion_cases.cpp.j2 lays the cases out as C++, and
test/next/conversion/conversion_conformance.cpp requires the C++ engine to compute the
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
    Shape,
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
    return Shape(leaf=value)


def layer(kind, *args, family=None, extent=None):
    return Shape(layer=kind, args=args, family=family, extent=extent)


def stored_shapes(value):
    base = leaf(value)
    yield base
    yield layer("ptr", base)
    yield layer("ref", base)
    yield layer("array", base, extent=2)
    yield layer("alt", base, leaf(OTHER), family="std")
    for family in FAMILIES:
        for outer in WRAPPERS:
            yield layer(outer, base, family=family)
            if outer != "opt":
                yield layer(outer, layer("array_unbounded", base), family=family)
            for inner in WRAPPERS:
                yield layer(outer, layer(inner, base, family="std"), family=family)


def chain(shape):
    while shape.layer is not None:
        yield shape
        shape = shape.args[0]
    yield shape


def request_shapes(stored, interfaces):
    value = list(chain(stored))[-1]
    leaves = [value, *[leaf(interface) for interface in interfaces]]
    shapes = set(chain(stored))
    for target in leaves:
        shapes.add(target)
        for family in FAMILIES:
            for wrapper in WRAPPERS:
                if wrapper == "opt" and not target.leaf.constructible:
                    continue
                shapes.add(layer(wrapper, target, family=family))
    shapes = {s for s in shapes if s.layer != "array_unbounded"}
    return sorted(shapes | {layer("ptr", s) for s in shapes if s.layer != "ref"}, key=repr)


def as_data(shape):
    return {
        "layer": shape.layer,
        "family": shape.family,
        "leaf": shape.leaf.name if shape.leaf else None,
        "extent": shape.extent,
        "args": [as_data(arg) for arg in shape.args],
    }


def valid_request(shape, kind):
    if shape.layer == "ref":
        return False
    abstract = shape.layer is None and not shape.leaf.constructible
    return not (abstract and kind != RequestKind.LVALUE_REFERENCE)


def steps_of(model):
    """The route form and the C++ tag of the kind of each route id."""
    tags = {p.kind: p.spelling for p in model.spec.cpp.kind_projections}
    kinds = {
        form.id: kind.id for kind in model.spec.layer_kinds for form in kind.forms
    }
    return {
        route.id: {
            "form": route.form.value if route.form is not None else route.id,
            "kind": tags[kinds[route.id]] if route.id in kinds else None,
        }
        for route in model.routes
    }


def cases(model):
    steps = steps_of(model)
    for scope in SCOPES:
        for value, interfaces in BINDING_LEAVES:
            for stored in stored_shapes(value):
                for request_shape in request_shapes(stored, interfaces):
                    for kind in KINDS:
                        if not valid_request(request_shape, kind):
                            continue
                        request = Request(kind, request_shape)
                        outcome = resolve(model, scope, stored, request, interfaces)
                        yield {
                            "scope": scope,
                            "interfaces": [i.name for i in interfaces],
                            "stored": as_data(stored),
                            "kind": kind,
                            "request": as_data(request_shape),
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
