"""Content of the operations section.

The route of each form of a wrapper semantics: `K` is the kind that has the form and
`T` the kind it leads to. A kind's routes are its forms with `K` and `T`
replaced; a kind of the user has them as the kinds of the library do.
"""

from spec.content.patterns import (
    KIND,
    LVALUE,
    PRVALUE,
    RVALUE,
    TARGET,
    layer,
    leaf_variable,
    variable,
)
from spec.schema import (
    Access,
    LayerTrait,
    Predicate,
    RequestCategory,
    Requirement,
    Operation,
    OperationTemplate,
    Via,
)

LEAF = (Requirement(predicate=Predicate.LEAF, subject="L"),)


def upcast(form, category, access=Access.INHERIT):
    return OperationTemplate(
        id=form.value,
        form=form,
        from_shape=layer(KIND, leaf_variable(None), category=category),
        to_shape=layer(KIND, leaf_variable(None), category=category),
        access=access,
        requires=LEAF,
        via=Via.UPCAST,
    )


operations: tuple[OperationTemplate, ...] = (
    OperationTemplate(
        id="borrow",
        form=Operation.BORROW,
        from_shape=layer(KIND, variable("X"), category=LVALUE),
        to_shape=variable("X", LVALUE),
        access=Access.INHERIT,
    ),
    OperationTemplate(
        id="take",
        form=Operation.TAKE,
        from_shape=layer(KIND, variable("X"), category=RVALUE),
        to_shape=variable("X", RVALUE),
        access=Access.CONSUME,
        exposure=RequestCategory.RVALUE,
    ),
    OperationTemplate(
        id="wrap",
        form=Operation.WRAP,
        from_shape=variable("X", RVALUE),
        to_shape=layer(KIND, variable("X"), category=RVALUE),
        access=Access.CONSUME,
        exposure=RequestCategory.RVALUE,
    ),
    OperationTemplate(
        id="adopt",
        form=Operation.ADOPT,
        from_shape=layer(TARGET, variable("X"), category=RVALUE),
        to_shape=layer(KIND, variable("X"), category=RVALUE),
        access=Access.CONSUME,
        exposure=RequestCategory.RVALUE,
    ),
    OperationTemplate(
        id="share",
        form=Operation.SHARE,
        from_shape=layer(KIND, variable("X"), category=RVALUE),
        to_shape=layer(TARGET, variable("X"), category=RVALUE),
        access=Access.CONSUME,
        exposure=RequestCategory.RVALUE,
    ),
    OperationTemplate(
        id="address",
        form=Operation.ADDRESS,
        from_shape=variable("X", LVALUE),
        to_shape=layer(KIND, variable("X"), category=PRVALUE),
        access=Access.BORROW,
        exposure=RequestCategory.POINTER,
        requires=(Requirement(predicate=Predicate.ADDRESSABLE, subject="X"),),
    ),
    OperationTemplate(
        id="decay",
        form=Operation.DECAY,
        from_shape=layer(KIND, variable("X"), category=LVALUE),
        to_shape=layer(TARGET, variable("X"), category=PRVALUE),
        access=Access.BORROW,
        exposure=RequestCategory.POINTER,
    ),
    OperationTemplate(
        id="release",
        form=Operation.RELEASE,
        from_shape=layer(KIND, variable("X"), category=RVALUE),
        to_shape=layer(TARGET, variable("X"), category=PRVALUE),
        access=Access.CONSUME,
        exposure=RequestCategory.POINTER,
        # Only an array the scope allocated for the request, which is never
        # a value, hands out its elements: one that moves dies with them.
        kind_traits=(LayerTrait.IMMOVABLE,),
    ),
    OperationTemplate(
        id="release_whole",
        form=Operation.RELEASE_WHOLE,
        from_shape=variable("A", RVALUE),
        to_shape=layer(KIND, variable("A"), category=PRVALUE),
        access=Access.CONSUME,
        exposure=RequestCategory.POINTER,
        requires=(Requirement(predicate=Predicate.ARRAY, subject="A"),),
    ),
    upcast(Operation.UPCAST_COPIED, PRVALUE),
    upcast(Operation.UPCAST_CONSUMED, RVALUE),
    OperationTemplate(
        id="retain",
        form=Operation.RETAIN,
        from_shape=layer(KIND, leaf_variable(None), category=LVALUE),
        to_shape=layer(KIND, leaf_variable(None), category=LVALUE),
        access=Access.BORROW,
        requires=LEAF,
        via=Via.UPCAST,
        cache=layer(KIND, leaf_variable(None)),
        # The kept handle is a copy of the borrowed one.
        kind_traits=(LayerTrait.COPYABLE,),
    ),
)
