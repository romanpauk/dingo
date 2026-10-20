"""Content of the route forms section.

The route of each form of a layer kind: `K` is the kind that has the form and
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
    Predicate,
    RequestCategory,
    Requirement,
    RouteForm,
    RouteFormTemplate,
    Via,
)

LEAF = (Requirement(predicate=Predicate.LEAF, subject="L"),)


def upcast(form, category, access=Access.INHERIT):
    return RouteFormTemplate(
        id=form.value,
        form=form,
        from_shape=layer(KIND, leaf_variable(None), category=category),
        to_shape=layer(KIND, leaf_variable(None), category=category),
        access=access,
        requires=LEAF,
        via=Via.UPCAST,
    )


route_forms: tuple[RouteFormTemplate, ...] = (
    RouteFormTemplate(
        id="borrow",
        form=RouteForm.BORROW,
        from_shape=layer(KIND, variable("X"), category=LVALUE),
        to_shape=variable("X", LVALUE),
        access=Access.INHERIT,
    ),
    RouteFormTemplate(
        id="take",
        form=RouteForm.TAKE,
        from_shape=layer(KIND, variable("X"), category=RVALUE),
        to_shape=variable("X", RVALUE),
        access=Access.CONSUME,
        exposure=RequestCategory.RVALUE,
    ),
    RouteFormTemplate(
        id="wrap",
        form=RouteForm.WRAP,
        from_shape=variable("X", RVALUE),
        to_shape=layer(KIND, variable("X"), category=RVALUE),
        access=Access.CONSUME,
        exposure=RequestCategory.RVALUE,
    ),
    RouteFormTemplate(
        id="adopt",
        form=RouteForm.ADOPT,
        from_shape=layer(TARGET, variable("X"), category=RVALUE),
        to_shape=layer(KIND, variable("X"), category=RVALUE),
        access=Access.CONSUME,
        exposure=RequestCategory.RVALUE,
    ),
    RouteFormTemplate(
        id="share",
        form=RouteForm.SHARE,
        from_shape=layer(KIND, variable("X"), category=RVALUE),
        to_shape=layer(TARGET, variable("X"), category=RVALUE),
        access=Access.CONSUME,
        exposure=RequestCategory.RVALUE,
    ),
    RouteFormTemplate(
        id="address",
        form=RouteForm.ADDRESS,
        from_shape=variable("X", LVALUE),
        to_shape=layer(KIND, variable("X"), category=PRVALUE),
        access=Access.BORROW,
        exposure=RequestCategory.POINTER,
        requires=(Requirement(predicate=Predicate.ADDRESSABLE, subject="X"),),
    ),
    RouteFormTemplate(
        id="decay",
        form=RouteForm.DECAY,
        from_shape=layer(KIND, variable("X"), category=LVALUE),
        to_shape=layer(TARGET, variable("X"), category=PRVALUE),
        access=Access.BORROW,
        exposure=RequestCategory.POINTER,
    ),
    RouteFormTemplate(
        id="release",
        form=RouteForm.RELEASE,
        from_shape=layer(KIND, variable("X"), category=RVALUE),
        to_shape=layer(TARGET, variable("X"), category=PRVALUE),
        access=Access.CONSUME,
        exposure=RequestCategory.POINTER,
    ),
    RouteFormTemplate(
        id="release_whole",
        form=RouteForm.RELEASE_WHOLE,
        from_shape=variable("A", RVALUE),
        to_shape=layer(KIND, variable("A"), category=PRVALUE),
        access=Access.CONSUME,
        exposure=RequestCategory.POINTER,
        requires=(Requirement(predicate=Predicate.ARRAY, subject="A"),),
    ),
    upcast(RouteForm.UPCAST_COPIED, PRVALUE),
    upcast(RouteForm.UPCAST_CONSUMED, RVALUE),
    RouteFormTemplate(
        id="retain",
        form=RouteForm.RETAIN,
        from_shape=layer(KIND, leaf_variable(None), category=LVALUE),
        to_shape=layer(KIND, leaf_variable(None), category=LVALUE),
        access=Access.BORROW,
        requires=LEAF,
        via=Via.UPCAST,
        cache=layer(KIND, leaf_variable(None)),
    ),
)
