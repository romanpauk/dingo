"""Content of the routes section.

The routes are the rules of the algebra of shapes: the rules of a leaf, which
copy it, upcast it to a declared interface, or convert it to one, and the
route forms of the layer kinds, each a form of route_forms with the kind and
the kind it leads to filled in. A kind of the user has its routes as a kind of
the library does. The routes are in the order of the algebra, the forms in
their order with the rules of a leaf among them, and the kinds of a form in
their order: a derivation prefers, of two routes of as many rules, the one of
the rules that come first.
"""

from spec.content.layer_kinds import layer_kinds
from spec.content.patterns import (
    KIND,
    LVALUE,
    PRVALUE,
    RVALUE,
    TARGET,
    leaf_variable,
    substitute,
    variable,
)
from spec.content.route_forms import route_forms
from spec.schema import (
    Access,
    Predicate,
    RequestCategory,
    Requirement,
    Route,
    RouteForm,
    Via,
)

LEAF = (Requirement(predicate=Predicate.LEAF, subject="L"),)

# The rules of a leaf.
COPY = Route(
    id="copy",
    description="Copies a borrowed object.",
    from_shape=variable("X", LVALUE),
    to_shape=variable("X", PRVALUE),
    access=Access.BORROW,
    exposure=RequestCategory.VALUE,
    requires=(Requirement(predicate=Predicate.COPYABLE, subject="X"),),
)
UPCAST_REFERENCE = Route(
    id="upcast_reference",
    description="Refers to a borrowed leaf as one of its interfaces.",
    from_shape=leaf_variable(LVALUE),
    to_shape=leaf_variable(LVALUE),
    access=Access.INHERIT,
    requires=LEAF,
    via=Via.UPCAST,
)
CONVERT_BORROWED = Route(
    id="convert_borrowed",
    description=(
        "Makes an object of an interface from a borrowed leaf by the conversion "
        "the leaf declares to it."
    ),
    from_shape=leaf_variable(LVALUE),
    to_shape=leaf_variable(PRVALUE),
    access=Access.BORROW,
    exposure=RequestCategory.VALUE,
    requires=LEAF,
    via=Via.CONVERT,
)
CONVERT_CONSUMED = Route(
    id="convert_consumed",
    description=(
        "Makes an object of an interface from a consumed leaf by the conversion "
        "the leaf declares to it."
    ),
    from_shape=leaf_variable(RVALUE),
    to_shape=leaf_variable(PRVALUE),
    access=Access.CONSUME,
    requires=LEAF,
    via=Via.CONVERT,
)

# The order of the algebra: the forms, with the rules of a leaf among them.
ORDER = (
    RouteForm.BORROW,
    COPY,
    RouteForm.ADDRESS,
    RouteForm.DECAY,
    RouteForm.TAKE,
    RouteForm.WRAP,
    RouteForm.SHARE,
    RouteForm.ADOPT,
    RouteForm.RELEASE,
    RouteForm.RELEASE_WHOLE,
    UPCAST_REFERENCE,
    RouteForm.UPCAST_COPIED,
    RouteForm.UPCAST_CONSUMED,
    CONVERT_BORROWED,
    CONVERT_CONSUMED,
    RouteForm.RETAIN,
)


def kind_routes(kinds, form):
    """The routes of `form` of `kinds`, in their order."""
    template = next(t for t in route_forms if t.form is form)
    for kind in kinds:
        for kind_form in kind.forms:
            if kind_form.form is not form:
                continue
            names = {KIND: kind.id}
            if kind_form.target is not None:
                names[TARGET] = kind_form.target
            yield Route(
                id=kind_form.id,
                description=kind_form.description,
                from_shape=substitute(template.from_shape, names),
                to_shape=substitute(template.to_shape, names),
                access=template.access,
                exposure=template.exposure,
                requires=template.requires,
                via=template.via,
                cache=substitute(template.cache, names),
                form=form,
            )


def routes_of(kinds):
    """The routes of the algebra for the layer kinds `kinds`."""
    result = []
    for entry in ORDER:
        if isinstance(entry, Route):
            result.append(entry)
        else:
            result.extend(kind_routes(kinds, entry))
    return tuple(result)


routes: tuple[Route, ...] = routes_of(layer_kinds)
