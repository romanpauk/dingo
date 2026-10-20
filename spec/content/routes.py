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

A derived shape is at most one layer larger than the stored shape, so a
binding can publish every term it serves before any request is known.

A binding that declares interfaces is found only by requests whose leaf is one
of them; the upcast rules convert the stored leaf to each, and the conversion
rules make an object of each the leaf declares a user conversion to. A layer a rule
rebuilds from a layer of the same kind keeps its wrapper family, and a layer a
rule creates may be of any family whose wrapper provides that layer kind.
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
    Via,
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


def leaf_variable(category):
    return variable("L", category)


def upcast(
    rule_id,
    description,
    from_shape,
    to_shape,
    access=Access.INHERIT,
    via=Via.UPCAST,
    exposure=None,
):
    return Route(
        id=rule_id,
        description=description,
        from_shape=from_shape,
        to_shape=to_shape,
        access=access,
        exposure=exposure,
        requires=(Requirement(predicate=Predicate.LEAF, subject="L"),),
        via=via,
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
    # A declared interface is reached by converting the leaf.
    upcast(
        "upcast_reference",
        "Refers to a borrowed leaf as one of its interfaces.",
        leaf_variable(LVALUE),
        leaf_variable(LVALUE),
    ),
    upcast(
        "upcast_pointer",
        "Converts a pointer to a leaf to a pointer to one of its interfaces.",
        layer("ptr", leaf_variable(None), category=PRVALUE),
        layer("ptr", leaf_variable(None), category=PRVALUE),
    ),
    upcast(
        "upcast_consumed_pointer",
        "Converts a consumed pointer to a leaf to one to one of its interfaces.",
        layer("ptr", leaf_variable(None), category=RVALUE),
        layer("ptr", leaf_variable(None), category=RVALUE),
    ),
    upcast(
        "upcast_owned",
        "Converts a consumed sole-owner handle of a leaf to one of an interface.",
        layer("own", leaf_variable(None), category=RVALUE),
        layer("own", leaf_variable(None), category=RVALUE),
    ),
    upcast(
        "upcast_shared",
        "Converts a consumed sharing handle of a leaf to one of an interface.",
        layer("share", leaf_variable(None), category=RVALUE),
        layer("share", leaf_variable(None), category=RVALUE),
    ),
    upcast(
        "upcast_copied_shared",
        "Converts a copied sharing handle of a leaf to one of an interface.",
        layer("share", leaf_variable(None), category=PRVALUE),
        layer("share", leaf_variable(None), category=PRVALUE),
    ),
    # A declared interface the leaf declares a user conversion to is made from
    # the leaf.
    upcast(
        "convert_borrowed",
        "Makes an object of an interface from a borrowed leaf by the conversion "
        "the leaf declares to it.",
        leaf_variable(LVALUE),
        leaf_variable(PRVALUE),
        access=Access.BORROW,
        via=Via.CONVERT,
        exposure=RequestCategory.VALUE,
    ),
    upcast(
        "convert_consumed",
        "Makes an object of an interface from a consumed leaf by the conversion "
        "the leaf declares to it.",
        leaf_variable(RVALUE),
        leaf_variable(PRVALUE),
        access=Access.CONSUME,
        via=Via.CONVERT,
    ),
    Route(
        id="retain_shared_upcast",
        description=(
            "Keeps a sharing handle of an interface converted from a borrowed "
            "sharing handle of the leaf, so it can be referred to."
        ),
        from_shape=layer("share", leaf_variable(None), category=LVALUE),
        to_shape=layer("share", leaf_variable(None), category=LVALUE),
        access=Access.BORROW,
        requires=(Requirement(predicate=Predicate.LEAF, subject="L"),),
        via=Via.UPCAST,
        cache=layer("share", leaf_variable(None)),
    ),
)
