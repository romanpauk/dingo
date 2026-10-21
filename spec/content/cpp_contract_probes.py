"""Content of the C++ contract probes section.

Each entry links a contract to the metrics of test/contracts/probe.py that
observe it, or states why the probes cannot. Metric patterns are fnmatch
patterns over the metric paths of test/contracts/baseline.json, such as
`object_size/flat_resolved.static.32`.

The runtime cost of a resolution is the cost of the cheapest path through a
probe function that resolves a request resolved before. It is a lower bound of
every path, and the memoized path where that is the cheapest: the probes of a
reference to an object of a stable binding, whose names end in _reference.
"""

from spec.schema import CppContractProbe

NO_CONSTRUCTS = (
    "The current implementation has no such construct, and the recorded "
    "instantiation counts cover all Dingo templates together."
)


def observed(contract: str, *metrics: str, excluding: tuple[str, ...] = ()) -> CppContractProbe:
    return CppContractProbe(
        id=contract, contract=contract, metrics=metrics, excluding=excluding
    )


def unobserved(contract: str, reason: str) -> CppContractProbe:
    return CppContractProbe(id=contract, contract=contract, not_observable=reason)


HEADER_PATHS = (
    ("container", "dingo/container.h"),
    ("runtime_container", "dingo/runtime_container.h"),
    ("static_container", "dingo/static_container.h"),
    ("storage_external", "dingo/storage/external.h"),
    ("storage_shared", "dingo/storage/shared.h"),
    ("storage_shared_cyclical", "dingo/storage/shared_cyclical.h"),
    ("storage_unique", "dingo/storage/unique.h"),
    ("factory_callable", "dingo/factory/callable.h"),
    ("factory_constructor", "dingo/factory/constructor.h"),
    ("factory_function", "dingo/factory/function.h"),
)

cpp_contract_probes: tuple[CppContractProbe, ...] = (
    # Object size
    observed("object_size_static_container", "object_size/*.static.*"),
    unobserved(
        "object_size_static_unused_features",
        "No probe measures a static container next to the same container with an unused feature.",
    ),
    observed("object_size_mixed_container", "object_size/*.mixed.*"),
    observed("object_size_runtime_container", "object_size/runtime"),
    # Compile cost
    unobserved("compile_register_type_no_kernels", NO_CONSTRUCTS),
    unobserved("compile_register_type_no_static_kernels", NO_CONSTRUCTS),
    unobserved("compile_register_type_no_selections", NO_CONSTRUCTS),
    unobserved("compile_register_type_binding_thunks", NO_CONSTRUCTS),
    unobserved("compile_descriptor", NO_CONSTRUCTS),
    unobserved("compile_route_plan", NO_CONSTRUCTS),
    unobserved("compile_binding_facts", NO_CONSTRUCTS),
    unobserved("compile_selection", NO_CONSTRUCTS),
    unobserved("compile_kernel", NO_CONSTRUCTS),
    unobserved("compile_static_kernel", NO_CONSTRUCTS),
    observed(
        "compile_instantiations_bindings",
        "instantiations/flat_*",
        "instantiations/wrapper_resolved.*",
    ),
    observed("compile_instantiations_chain_depth", "instantiations/chain.*"),
    # Runtime cost
    observed(
        "runtime_static_resolve_indirect_calls",
        "runtime_cost/paths/probe_resolve_static_*/indirect_calls",
    ),
    observed(
        "runtime_static_resolve_allocations",
        "runtime_cost/allocations/static_*",
    ),
    unobserved(
        "runtime_runtime_resolve_lookups",
        "No probe counts lookups; a lookup makes no call, so the paths of the "
        "probes do not tell them apart.",
    ),
    observed(
        "runtime_runtime_resolve_indirect_calls",
        "runtime_cost/paths/probe_resolve_runtime_*_reference/indirect_calls",
    ),
    observed(
        "runtime_runtime_resolve_allocations",
        "runtime_cost/allocations/runtime_*.memoized",
    ),
    # Code size
    observed(
        "code_size_static_resolve_per_probe",
        "codegen/probes/probe_static_resolution_*/bytes",
    ),
    # Inlining
    observed(
        "inline_static_resolve",
        "codegen/probes/probe_static_resolution_*/dingo_calls",
        excluding=(
            "codegen/probes/probe_static_resolution_mixed_container_*",
            "codegen/probes/*.cold/*",
        ),
    ),
    observed(
        "inline_runtime_resolve_memoized",
        "runtime_cost/paths/probe_resolve_runtime_*_reference/dingo_calls",
    ),
    unobserved(
        "inline_runtime_resolve_first_resolution",
        "baseline.json counts the out-of-line Dingo functions of the whole object file, not the first resolution.",
    ),
    unobserved(
        "inline_register_type",
        "baseline.json counts the out-of-line Dingo functions of the whole object file, not register_type.",
    ),
    unobserved(
        "inline_diagnostics_throw_never",
        "No codegen probe covers a diagnostic throw function.",
    ),
    unobserved(
        "inline_diagnostics_throw_cold",
        "No codegen probe covers a diagnostic throw function, and section placement is not recorded.",
    ),
    # Header cost
    *(
        observed(f"header_cost_{name}", f"headers/{path}/dingo_lines")
        for name, path in HEADER_PATHS
    ),
)
