"""Content of the selection rules section.

A request is answered from the sources of a resolution in steps: the local
bindings of the registration whose object is being constructed, the bindings
of the container, its parent, and auto-construction of an unregistered type,
or the factory of a request to construct an object.
The static and runtime bindings of one level are united, so a static and a
runtime binding of one key are two candidates. The first step that names a
binding answers the request: a child shadows its parent, and an ambiguous
level stops the search. Both domains follow the same rules; a static container
decides them when the request is compiled.
"""

from spec.schema import (
    Cardinality,
    Combine,
    SelectionMode,
    SelectionOutcome,
    SelectionRule,
    SelectionSource,
    SelectionStep,
    Status,
    StepCondition,
)

LOCAL = (SelectionSource.LOCAL_STATIC, SelectionSource.LOCAL_RUNTIME)
CONTAINER = (SelectionSource.CONTAINER_STATIC, SelectionSource.CONTAINER_RUNTIME)

selection_rules: tuple[SelectionRule, ...] = (
    SelectionRule(
        id="singular",
        description=(
            "A request of one object: the local bindings shadow the container, "
            "the container its parent, and an unregistered type that can be "
            "auto-constructed is constructed when no level names a binding."
        ),
        mode=SelectionMode.SINGULAR,
        combine=Combine.FIRST_NON_EMPTY,
        steps=(
            SelectionStep(sources=LOCAL),
            SelectionStep(sources=CONTAINER),
            SelectionStep(sources=(SelectionSource.PARENT,)),
            SelectionStep(
                sources=(SelectionSource.AUTO_CONSTRUCT,),
                when=StepCondition.CONSTRUCTIBLE,
            ),
        ),
        outcomes=(
            SelectionOutcome(count=Cardinality.ONE, status=Status.OK),
            SelectionOutcome(count=Cardinality.MANY, status=Status.AMBIGUOUS),
            SelectionOutcome(count=Cardinality.EMPTY, status=Status.NOT_FOUND),
        ),
    ),
    SelectionRule(
        id="collection",
        description=(
            "A request of a collection: the bindings of the container and the "
            "local ones extend each other, in the order of the sources, runtime "
            "before static, and the parent is asked when they name none. No "
            "binding is an empty collection."
        ),
        mode=SelectionMode.COLLECTION,
        combine=Combine.FIRST_NON_EMPTY,
        steps=(
            SelectionStep(
                sources=(
                    SelectionSource.CONTAINER_RUNTIME,
                    SelectionSource.CONTAINER_STATIC,
                    SelectionSource.LOCAL_RUNTIME,
                    SelectionSource.LOCAL_STATIC,
                )
            ),
            SelectionStep(sources=(SelectionSource.PARENT,)),
        ),
        outcomes=(
            SelectionOutcome(count=Cardinality.EMPTY, status=Status.OK),
            SelectionOutcome(count=Cardinality.ONE, status=Status.OK),
            SelectionOutcome(count=Cardinality.MANY, status=Status.OK),
        ),
    ),
    SelectionRule(
        id="construct",
        description=(
            "A request to construct an object: a binding of its type answers "
            "it as a singular request does, and when no level names one, its "
            "factory makes the object, whether or not the type could be "
            "auto-constructed."
        ),
        mode=SelectionMode.CONSTRUCT,
        combine=Combine.FIRST_NON_EMPTY,
        steps=(
            SelectionStep(sources=LOCAL),
            SelectionStep(sources=CONTAINER),
            SelectionStep(sources=(SelectionSource.PARENT,)),
            SelectionStep(
                sources=(SelectionSource.FACTORY,),
                when=StepCondition.HAS_FACTORY,
            ),
        ),
        outcomes=(
            SelectionOutcome(count=Cardinality.ONE, status=Status.OK),
            SelectionOutcome(count=Cardinality.MANY, status=Status.AMBIGUOUS),
            SelectionOutcome(count=Cardinality.EMPTY, status=Status.NOT_FOUND),
        ),
    ),
)
