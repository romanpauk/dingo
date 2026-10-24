"""Content of the routes section.

The routes are the rules of the algebra of shapes: the rules of a leaf, which
copy it, upcast it to a declared interface, or convert it to one, and the
operations of the wrapper semantics, each a form of operations with the kind and
the kind it leads to filled in. A kind of the user has its routes as a kind of
the library does. The routes are in the order of the algebra, the forms in
their order with the rules of a leaf among them, and the kinds of a form in
their order: a derivation prefers, of two routes of as many rules, the one of
the rules that come first.
"""

from spec.content.wrapper_semantics import wrapper_semantics
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
from spec.content.operations import operations
from spec.schema import (
    Access,
    Predicate,
    RequestCategory,
    Requirement,
    Rule,
    Operation,
    Via,
)

LEAF = (Requirement(predicate=Predicate.LEAF, subject="L"),)

# The rules of a leaf.
COPY = Rule(
    id="copy",
    description="Copies a borrowed object.",
    from_shape=variable("X", LVALUE),
    to_shape=variable("X", PRVALUE),
    access=Access.BORROW,
    exposure=RequestCategory.VALUE,
    requires=(Requirement(predicate=Predicate.COPYABLE, subject="X"),),
)
UPCAST_REFERENCE = Rule(
    id="upcast_reference",
    description="Refers to a borrowed leaf as one of its interfaces.",
    from_shape=leaf_variable(LVALUE),
    to_shape=leaf_variable(LVALUE),
    access=Access.INHERIT,
    requires=LEAF,
    via=Via.UPCAST,
)
CONVERT_BORROWED = Rule(
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
CONVERT_CONSUMED = Rule(
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
    Operation.BORROW,
    COPY,
    Operation.ADDRESS,
    Operation.DECAY,
    Operation.TAKE,
    Operation.WRAP,
    Operation.SHARE,
    Operation.ADOPT,
    Operation.RELEASE,
    Operation.RELEASE_WHOLE,
    UPCAST_REFERENCE,
    Operation.UPCAST_COPIED,
    Operation.UPCAST_CONSUMED,
    CONVERT_BORROWED,
    CONVERT_CONSUMED,
    Operation.RETAIN,
)


def semantics_rules(kinds, form):
    """The routes of `form` of `kinds`, in their order."""
    template = next(t for t in operations if t.form is form)
    for kind in kinds:
        for op in kind.forms:
            if op.form is not form:
                continue
            names = {KIND: kind.id}
            if op.target is not None:
                names[TARGET] = op.target
            yield Rule(
                id=op.id,
                description=op.description,
                from_shape=substitute(template.from_shape, names),
                to_shape=substitute(template.to_shape, names),
                access=template.access,
                exposure=template.exposure,
                requires=template.requires,
                via=template.via,
                cache=substitute(template.cache, names),
                form=form,
            )


def rules_of(kinds):
    """The routes of the algebra for the wrapper semantics `kinds`."""
    result = []
    for entry in ORDER:
        if isinstance(entry, Rule):
            result.append(entry)
        else:
            result.extend(semantics_rules(kinds, entry))
    return tuple(result)


rules: tuple[Rule, ...] = rules_of(wrapper_semantics)
