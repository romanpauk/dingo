"""Content of the factory kinds section."""

from spec.schema import DependencyVisibility, FactoryKind

factory_kinds: tuple[FactoryKind, ...] = (
    FactoryKind(
        id="detected_constructor",
        description=(
            "Constructs the leaf by the constructor detected from its type: the "
            "one with the most parameters, up to the configured largest arity, "
            "found by list-initializing it with arguments that convert to any "
            "parameter type except the leaf itself, so the copy constructor is "
            "never selected. A leaf that also accepts arguments converting to "
            "nothing has a generic constructor and needs a declared factory. "
            "Every argument resolves the type its parameter asks for, and every "
            "layer of the stored shape is made from its element, constructed in "
            "place."
        ),
        dependencies=DependencyVisibility.DETECTED,
    ),
    FactoryKind(
        id="declared_constructor",
        description=(
            "Constructs the leaf by the constructor whose parameter types the "
            "binding declares."
        ),
        dependencies=DependencyVisibility.DECLARED,
    ),
    FactoryKind(
        id="function",
        description=(
            "Calls a function whose parameter types are its dependencies and "
            "whose result is the stored object."
        ),
        dependencies=DependencyVisibility.DECLARED,
    ),
    FactoryKind(
        id="callable",
        description=(
            "Calls a callable object supplied at registration, whose parameter "
            "types are its dependencies and whose result is the stored object."
        ),
        dependencies=DependencyVisibility.DECLARED,
    ),
)
