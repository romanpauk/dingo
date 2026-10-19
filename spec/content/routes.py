"""Content of the route rules section.

A route rule derives a term, a shape in a value category, from another term. A
binding starts from its stored shape in the category its scope materializes:
an LVALUE for scopes whose requests borrow, an RVALUE for scopes whose requests
consume. A request is served when a term it accepts can be derived:

- an lvalue reference request of shape X accepts the term (X, lvalue) when X
  is addressable
- an rvalue reference request of shape X accepts (X, rvalue)
- a value request of shape X accepts (X, rvalue) or (X, prvalue)

An array is never handed out by value or as an rvalue. A raw pointer is a
value, not an object: it is not addressable, so a borrowed pointer is reached
through but is never itself referred to or pointed to.

A rule with an `exposure` applies only when the scope exposes that request
category for the stored form; serving an lvalue reference request needs the
lvalue category. The rules are interpreted by spec/reference/routes.py.
"""

from spec.schema import (
    Access,
    PatternForm,
    Predicate,
    RequestCategory,
    Requirement,
    Route,
    ShapePattern,
    ValueCategory,
)

LVALUE = ValueCategory.LVALUE
RVALUE = ValueCategory.RVALUE
PRVALUE = ValueCategory.PRVALUE


def variable(name, category=None):
    return ShapePattern(form=PatternForm.VARIABLE, name=name, category=category)


def layer(name, *args, category=None):
    return ShapePattern(form=PatternForm.LAYER, name=name, args=args, category=category)


def reach(rule_id, layer_kind, description):
    return Route(
        id=rule_id,
        description=description,
        from_shape=layer(layer_kind, variable("X"), category=LVALUE),
        to_shape=variable("X", LVALUE),
        access=Access.INHERIT,
    )


def consume(rule_id, from_shape, to_shape, description, exposure=RequestCategory.RVALUE):
    return Route(
        id=rule_id,
        description=description,
        from_shape=from_shape,
        to_shape=to_shape,
        access=Access.CONSUME,
        exposure=exposure,
    )


routes: tuple[Route, ...] = (
    # Borrowing reaches the objects a borrowed object holds or refers to.
    reach("borrow_optional", "opt", "Borrows the element of an optional."),
    reach("borrow_owned", "own", "Borrows the element a sole-owner handle owns."),
    reach("borrow_shared", "share", "Borrows the element a sharing handle shares."),
    reach("deref_pointer", "ptr", "Borrows the element a pointer points to."),
    reach("deref_reference", "ref", "Borrows the element a reference aliases."),
    reach("select_alternative", "alt", "Borrows the held alternative."),
    # A borrowed object is copied, or its address is taken.
    Route(
        id="copy",
        description="Copies a borrowed object.",
        from_shape=variable("X", LVALUE),
        to_shape=variable("X", PRVALUE),
        access=Access.BORROW,
        exposure=RequestCategory.VALUE,
        requires=(Requirement(predicate=Predicate.COPYABLE, subject="X"),),
    ),
    Route(
        id="address_of",
        description="Takes the address of a borrowed object.",
        from_shape=variable("X", LVALUE),
        to_shape=layer("ptr", variable("X"), category=PRVALUE),
        access=Access.BORROW,
        exposure=RequestCategory.POINTER,
        requires=(Requirement(predicate=Predicate.ADDRESSABLE, subject="X"),),
    ),
    Route(
        id="decay_array",
        description="Takes the address of the first element of a borrowed array.",
        from_shape=layer("array", variable("X"), category=LVALUE),
        to_shape=layer("ptr", variable("X"), category=PRVALUE),
        access=Access.BORROW,
        exposure=RequestCategory.POINTER,
    ),
    Route(
        id="decay_unbounded_array",
        description=(
            "Takes the address of the first element of a borrowed array of "
            "runtime size."
        ),
        from_shape=layer("array_unbounded", variable("X"), category=LVALUE),
        to_shape=layer("ptr", variable("X"), category=PRVALUE),
        access=Access.BORROW,
        exposure=RequestCategory.POINTER,
    ),
    # Consuming moves an object out, wraps it, or transfers its ownership.
    consume(
        "take_alternative",
        layer("alt", variable("X"), category=RVALUE),
        variable("X", RVALUE),
        "Moves the held alternative out of a consumed alternative.",
    ),
    consume(
        "take_optional",
        layer("opt", variable("X"), category=RVALUE),
        variable("X", RVALUE),
        "Moves the element out of a consumed optional.",
    ),
    consume(
        "wrap_optional",
        variable("X", RVALUE),
        layer("opt", variable("X"), category=RVALUE),
        "Moves a consumed object into a new optional.",
    ),
    consume(
        "share_owned",
        layer("own", variable("X"), category=RVALUE),
        layer("share", variable("X"), category=RVALUE),
        "Transfers the element of a consumed sole-owner handle to a sharing handle.",
    ),
    consume(
        "adopt_pointer_owned",
        layer("ptr", variable("X"), category=RVALUE),
        layer("own", variable("X"), category=RVALUE),
        "Adopts the element of a consumed pointer into a sole-owner handle.",
    ),
    consume(
        "adopt_pointer_shared",
        layer("ptr", variable("X"), category=RVALUE),
        layer("share", variable("X"), category=RVALUE),
        "Adopts the element of a consumed pointer into a sharing handle.",
    ),
    consume(
        "release_array",
        layer("array", variable("X"), category=RVALUE),
        layer("ptr", variable("X"), category=PRVALUE),
        "Hands out the first element of a consumed array, whose ownership passes "
        "to the caller.",
        exposure=RequestCategory.POINTER,
    ),
    Route(
        id="release_whole_array",
        description=(
            "Hands out the address of a consumed array, whose ownership passes to "
            "the caller."
        ),
        from_shape=variable("A", RVALUE),
        to_shape=layer("ptr", variable("A"), category=PRVALUE),
        access=Access.CONSUME,
        exposure=RequestCategory.POINTER,
        requires=(Requirement(predicate=Predicate.ARRAY, subject="A"),),
    ),
)
