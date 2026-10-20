"""Content of the graph rules section."""

from spec.schema import EdgeTime, GraphRule

graph_rules: tuple[GraphRule, ...] = (
    GraphRule(
        id="cycle",
        description=(
            "A cycle of dependencies is legal if and only if every binding in "
            "it is shared_cyclical, which publishes its object before it is "
            "constructed. The dependencies a factory declares, by "
            "dependencies<>, a declared constructor, or the signature of a "
            "function or a callable, are known when a static container is "
            "compiled, which rejects a cycle among its static bindings then; "
            "the dependencies of a detected constructor are known when the "
            "object is constructed, which reports a cycle then."
        ),
        legal_scopes=("shared_cyclical",),
        checked_at=(EdgeTime.CONTAINER_FORMATION, EdgeTime.RESOLUTION),
        diagnostic="type_recursion",
    ),
)
