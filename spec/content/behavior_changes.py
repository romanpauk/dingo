"""Content of the behavior changes section.

Every intended difference between the target semantics of the spec and the
baseline library. A change lists the case paths of the checks that observe it,
see `BehaviorChange` in spec/schema.py. The route changes are observed by
test/routes/conform.py, the scope changes by test/spec/scopes_check.py.
"""

from spec.schema import BehaviorChange

DECISION = "docs/architecture/spec-driven-design.md, Decision Log, 2026-10-19"


def both(pattern):
    """The pattern in the static and the mixed container."""
    return (f"routes:static/{pattern}", f"routes:mixed/{pattern}")


behavior_changes: tuple[BehaviorChange, ...] = (
    BehaviorChange(
        id="borrowing_is_compositional",
        description=(
            "A borrowing scope reaches every object its stored shape holds, through "
            "any combination of layers, and copies every copyable one."
        ),
        cases=tuple(
            f"routes:*/*/{scope}/{stored}/*#served"
            for scope in ("external", "shared")
            for stored in (
                "std::optional<*>",
                "std::shared_ptr<std::optional<*>>",
                "std::unique_ptr<std::optional<*>>",
                "std::unique_ptr<std::shared_ptr<*>>",
            )
        ),
        baseline=(
            "Copies of optionals are refused, the leaf under a nested optional and "
            "the handle inside an optional are not reachable, and a sharing handle "
            "inside a sole-owner handle is not copied."
        ),
        now="Every reachable object is borrowed, and every copyable one is copied.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="borrowing_never_moves",
        description="A borrowing scope never serves an rvalue reference request.",
        cases=(
            "routes:mixed/*/shared/*&&#served",
            "routes:mixed/*/shared_cyclical/*&&#served",
        ),
        baseline=(
            "The mixed container serves an rvalue reference to a copy of a shared "
            "object, while the runtime and static containers refuse it."
        ),
        now="Every container refuses it.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="consuming_is_compositional",
        description=(
            "A consuming scope composes its rules: it takes the element out of an "
            "optional, and wraps or transfers what it took."
        ),
        cases=(
            "routes:*/*/unique/std::optional<*>/*#served",
            "routes:*/*/unique/std::variant<*>/std::optional<*>*#served",
            "routes:*/*/unique/*leaf[*]/*leaf[*]&&#served",
        ),
        baseline=(
            "The element of a consumed optional cannot be taken, an alternative "
            "taken from a variant cannot be wrapped, and a consumed pointer is not "
            "served as an rvalue reference."
        ),
        now="Every composition of consuming rules is served.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="consumed_objects_are_never_borrowed",
        description=(
            "A consuming scope never serves an lvalue reference to the object it "
            "consumed."
        ),
        cases=(
            "routes:*/*/unique/*leaf/*leaf&#served",
            "scopes:unique/form plain",
        ),
        baseline=(
            "The runtime and mixed containers return an lvalue reference to the "
            "constructed object, which is destroyed before the call returns."
        ),
        now="Every container refuses the request.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="consuming_moves_minimally",
        description=(
            "Serving a consumed object makes no more moves and destroys no more "
            "temporaries than the container that makes the fewest."
        ),
        cases=("routes:*/*/unique/*#moves", "routes:*/*/unique/*#temporaries"),
        baseline=(
            "The runtime container moves a consumed value one more time than the "
            "static container, and the mixed container one more time for rvalue "
            "references."
        ),
        now="Every container makes the fewest moves any of them makes.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="eager_registration_check",
        description=(
            "A static or mixed container rejects a binding its scope cannot store "
            "when the container type is formed, as the runtime container rejects "
            "it at registration."
        ),
        cases=(
            *both("*/shared/std::*_ptr<*[]>/*#registration"),
            *both("*/unique/std::*_ptr<*[]>/*#registration"),
            *both("*/unique/*leaf&/*#registration"),
        ),
        baseline=(
            "The static and mixed containers accept the binding and fail only "
            "when a request reaches it."
        ),
        now="Every container rejects the binding when it is declared.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="shared_cyclical_stores_flat_shapes",
        description=(
            "shared_cyclical stores only shapes whose element is a leaf, because "
            "the element must exist before it is published."
        ),
        cases=("routes:*/*/shared_cyclical/std::shared_ptr<std::*>/*#registration",),
        baseline=(
            "A nested sharing handle is accepted, and an lvalue reference request "
            "dereferences a null inner handle and aborts."
        ),
        now="The binding is rejected.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="shared_cyclical_stores_no_pointers",
        description=(
            "shared_cyclical does not store raw pointers, because a pointer cannot "
            "be published before the object it points to exists."
        ),
        cases=(
            "routes:*/*/shared_cyclical/*leaf[*]/*#registration",
            "scopes:shared_cyclical/form pointer",
        ),
        baseline="The binding is accepted and publishes a null pointer.",
        now="The binding is rejected.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="shared_cyclical_exposes_no_arrays",
        description="The shared_cyclical descriptor exposes no array form.",
        cases=(
            "scopes:shared_cyclical/form bounded_array",
            "scopes:shared_cyclical/form shared_array_handle",
            "scopes:shared_cyclical/form unbounded_array",
        ),
        baseline=(
            "The scope policy exposes the array forms, but a binding of an array "
            "form does not compile."
        ),
        now="The forms are not exposed, so the binding is rejected by the descriptor.",
        decision=DECISION,
    ),
    BehaviorChange(
        id="static_external_instances",
        description=(
            "A static or mixed container accepts an external binding, whose "
            "instance it takes as a constructor argument."
        ),
        cases=both("*/external/*#registration"),
        baseline="Static storage must be default constructible, so it is rejected.",
        now=(
            "The binding is accepted. This follows the proposal in Open Decisions "
            "and is revised if the decision differs."
        ),
        decision=DECISION,
    ),
)
