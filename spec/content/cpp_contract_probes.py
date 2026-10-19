"""Content of the C++ contract probes section.

Each entry links a contract to the metrics of test/contracts/probe.py that
observe it on the current implementation, or states why the current probes
cannot. Metric patterns are fnmatch patterns over the metric paths of
test/contracts/baseline.json, such as `object_size/flat_resolved.static.32`.
"""

from spec.schema import CppContractProbe

NO_CONSTRUCTS = (
    "The current implementation has no such construct, and the recorded "
    "instantiation counts cover all Dingo templates together."
)

NO_PER_RESOLVE_COUNT = (
    "The codegen probe functions include container construction and "
    "registration, so a per-resolve count of {what} is not isolated."
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
    unobserved(
        "runtime_static_resolve_indirect_calls",
        NO_PER_RESOLVE_COUNT.format(what="indirect calls"),
    ),
    unobserved(
        "runtime_static_resolve_allocations",
        "No probe counts allocations; the conformance tests need a counting allocator.",
    ),
    unobserved(
        "runtime_runtime_resolve_lookups",
        "No probe counts lookups, and the codegen probes resolve once, so a memoized lookup is not isolated.",
    ),
    unobserved(
        "runtime_runtime_resolve_indirect_calls",
        NO_PER_RESOLVE_COUNT.format(what="indirect calls"),
    ),
    unobserved(
        "runtime_runtime_resolve_allocations",
        "No probe counts allocations; the conformance tests need a counting allocator.",
    ),
    # Code size
    unobserved(
        "code_size_static_resolve_per_probe",
        "baseline.json records calls, not code size; the limits are in test/lit/check_codegen_probe_sizes.py.",
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
    unobserved(
        "inline_runtime_resolve_memoized",
        "The codegen probes resolve once, so the first resolution is in every runtime probe and a memoized resolution is not isolated.",
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
