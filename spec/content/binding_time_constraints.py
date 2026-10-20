"""Content of the binding-time constraints section: what a domain cannot
offer because of when its bindings are known."""

from spec.schema import BindingTimeConstraint, Domain

binding_time_constraints: tuple[BindingTimeConstraint, ...] = (
    BindingTimeConstraint(
        id="static_bindings_carry_no_runtime_key",
        domain=Domain.STATIC,
        feature="a static binding with a runtime key value",
        justification=(
            "A static binding is known when the container type is formed, "
            "before any runtime value exists; it carries a fixed key instead, "
            "and a runtime key value of a request is compared with it."
        ),
    ),
)
