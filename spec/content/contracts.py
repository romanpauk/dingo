"""Content of the contracts section.

Contracts bind the operations and components of the design, in reachable types of
abstract operations, constructs, and variables. Nothing here names a C++
entity or a toolchain, except where noted: the header cost limits are the
largest preprocessed sizes over the toolchains recorded in
test/contracts/baseline.json. Which contract is observable by which probe is
in the C++ projection (cpp_contract_probes.py).

Operations: `register_type`, `resolve` (`resolve.memoized` and
`resolve.first_resolution` for the runtime domain), `static_container`,
`mixed_container`, `runtime_container`, `container`, `diagnostics.throw`, and
`header.<name>`. Constructs are the internal constructs of the Instantiation
Contracts table: descriptor, route_plan, binding_facts, binding_thunk,
selection, kernel, and static_kernel.
"""

from spec.schema import Bound, BoundKind, Contract, ContractDimension, Domain

OBJECT_SIZE = ContractDimension.OBJECT_SIZE
COMPILE_COST = ContractDimension.COMPILE_COST
RUNTIME_COST = ContractDimension.RUNTIME_COST
CODE_SIZE = ContractDimension.CODE_SIZE
INLINING = ContractDimension.INLINING
HEADER_COST = ContractDimension.HEADER_COST


def at_most(value: int, unit: str) -> Bound:
    return Bound(kind=BoundKind.AT_MOST, value=value, unit=unit)


def exactly(value: int, unit: str) -> Bound:
    return Bound(kind=BoundKind.EXACTLY, value=value, unit=unit)


def linear(variable: str, unit: str, per: int | None = None) -> Bound:
    return Bound(kind=BoundKind.LINEAR, variable=variable, unit=unit, value=per)


# Preprocessed Dingo lines of each public header, as the maximum over the
# recorded toolchains (gcc13, gcc16, clang18, and clang22 on x86_64 with C++20).
HEADER_LINES = (
    ("container", 7776),
    ("runtime_container", 7781),
    ("static_container", 6756),
    ("storage_external", 4071),
    ("storage_shared", 4071),
    ("storage_shared_cyclical", 4071),
    ("storage_unique", 4071),
    ("factory_callable", 1756),
    ("factory_constructor", 4087),
    ("factory_function", 1747),
)

contracts: tuple[Contract, ...] = (
    # Object size
    Contract(
        id="object_size_static_container",
        description="The size of a static container is linear in the number of bindings.",
        operation="static_container",
        dimension=OBJECT_SIZE,
        bound=linear("bindings", "bytes"),
        domain=Domain.STATIC,
    ),
    Contract(
        id="object_size_static_unused_features",
        description="A feature that no binding uses adds nothing to the size of a static container.",
        operation="static_container.unused_feature",
        dimension=OBJECT_SIZE,
        bound=exactly(0, "bytes"),
        domain=Domain.STATIC,
    ),
    Contract(
        id="object_size_mixed_container",
        description="The size of a mixed container is linear in the number of static bindings.",
        operation="mixed_container",
        dimension=OBJECT_SIZE,
        bound=linear("bindings", "bytes"),
        domain=Domain.MIXED,
    ),
    Contract(
        id="object_size_runtime_container",
        description="The size of a runtime container is independent of the number of registrations.",
        operation="runtime_container",
        dimension=OBJECT_SIZE,
        bound=Bound(kind=BoundKind.CONSTANT),
        domain=Domain.RUNTIME,
    ),
    # Compile cost
    Contract(
        id="compile_register_type_no_kernels",
        description="Registering a type produces no request kernel; kernels appear only when a request is resolved.",
        operation="register_type",
        dimension=COMPILE_COST,
        bound=at_most(0, "instantiations"),
        construct="kernel",
    ),
    Contract(
        id="compile_register_type_no_static_kernels",
        description="Registering a type produces no static request kernel.",
        operation="register_type",
        dimension=COMPILE_COST,
        bound=at_most(0, "instantiations"),
        domain=Domain.STATIC,
        construct="static_kernel",
    ),
    Contract(
        id="compile_register_type_no_selections",
        description="Registering a type produces no selection; selections appear only when a request is resolved.",
        operation="register_type",
        dimension=COMPILE_COST,
        bound=at_most(0, "instantiations"),
        construct="selection",
    ),
    Contract(
        id="compile_register_type_binding_thunks",
        description="Registering a type produces binding-side thunks that grow linearly in the number of bindings.",
        operation="register_type",
        dimension=COMPILE_COST,
        bound=linear("bindings", "instantiations"),
        construct="binding_thunk",
    ),
    Contract(
        id="compile_descriptor",
        description="A descriptor is instantiated once per distinct type.",
        operation="container",
        dimension=COMPILE_COST,
        bound=linear("distinct_types", "instantiations", per=1),
        construct="descriptor",
    ),
    Contract(
        id="compile_route_plan",
        description="A route plan is instantiated once per distinct type tree, not once per binding.",
        operation="container",
        dimension=COMPILE_COST,
        bound=linear("distinct_type_trees", "instantiations", per=1),
        construct="route_plan",
    ),
    Contract(
        id="compile_binding_facts",
        description="The binding facts are instantiated once per binding.",
        operation="register_type",
        dimension=COMPILE_COST,
        bound=linear("bindings", "instantiations", per=1),
        construct="binding_facts",
    ),
    Contract(
        id="compile_selection",
        description="A selection is instantiated once per request type.",
        operation="resolve",
        dimension=COMPILE_COST,
        bound=linear("request_types", "instantiations", per=1),
        construct="selection",
    ),
    Contract(
        id="compile_kernel",
        description="A kernel is instantiated once per used route, and only when the route is used.",
        operation="resolve",
        dimension=COMPILE_COST,
        bound=linear("used_routes", "instantiations", per=1),
        construct="kernel",
    ),
    Contract(
        id="compile_static_kernel",
        description="A static kernel is instantiated once per used binding, route, and request combination.",
        operation="resolve",
        dimension=COMPILE_COST,
        bound=linear("used_pairs", "instantiations", per=1),
        domain=Domain.STATIC,
        construct="static_kernel",
    ),
    Contract(
        id="compile_instantiations_bindings",
        description="The instantiations of a container grow at most linearly in the number of bindings.",
        operation="container",
        dimension=COMPILE_COST,
        bound=linear("bindings", "instantiations"),
    ),
    Contract(
        id="compile_instantiations_chain_depth",
        description="The instantiations of a container grow at most linearly in the depth of a dependency chain.",
        operation="container",
        dimension=COMPILE_COST,
        bound=linear("chain_depth", "instantiations"),
    ),
    # Runtime cost
    Contract(
        id="runtime_static_resolve_indirect_calls",
        description="A static resolution makes no indirect call.",
        operation="resolve",
        dimension=RUNTIME_COST,
        bound=at_most(0, "indirect_calls"),
        domain=Domain.STATIC,
    ),
    Contract(
        id="runtime_static_resolve_allocations",
        description="A static resolution makes no allocation.",
        operation="resolve",
        dimension=RUNTIME_COST,
        bound=at_most(0, "allocations"),
        domain=Domain.STATIC,
    ),
    Contract(
        id="runtime_runtime_resolve_lookups",
        description="A memoized runtime resolution performs one lookup.",
        operation="resolve.memoized",
        dimension=RUNTIME_COST,
        bound=exactly(1, "lookups"),
        domain=Domain.RUNTIME,
    ),
    Contract(
        id="runtime_runtime_resolve_indirect_calls",
        description=(
            "A memoized runtime resolution makes at most one indirect call; one "
            "that returns an object of a stable binding makes none."
        ),
        operation="resolve.memoized",
        dimension=RUNTIME_COST,
        bound=at_most(1, "indirect_calls"),
        domain=Domain.RUNTIME,
    ),
    Contract(
        id="runtime_runtime_resolve_allocations",
        description="A runtime resolution makes no allocation after the first resolution.",
        operation="resolve.memoized",
        dimension=RUNTIME_COST,
        bound=at_most(0, "allocations"),
        domain=Domain.RUNTIME,
    ),
    # Code size
    Contract(
        id="code_size_static_resolve_per_probe",
        description=(
            "The code of each static resolution codegen probe stays within the limit recorded "
            "for the probe and toolchain. Runtime resolution probes have no limit yet."
        ),
        operation="resolve",
        dimension=CODE_SIZE,
        bound=Bound(kind=BoundKind.RECORDED, unit="bytes"),
        domain=Domain.STATIC,
    ),
    # Inlining
    Contract(
        id="inline_static_resolve",
        description="A static resolution is always inlined into its caller.",
        operation="resolve",
        dimension=INLINING,
        bound=Bound(kind=BoundKind.ALWAYS),
        domain=Domain.STATIC,
    ),
    Contract(
        id="inline_runtime_resolve_memoized",
        description="A memoized runtime resolution is always inlined into its caller.",
        operation="resolve.memoized",
        dimension=INLINING,
        bound=Bound(kind=BoundKind.ALWAYS),
        domain=Domain.RUNTIME,
    ),
    Contract(
        id="inline_runtime_resolve_first_resolution",
        description="The first runtime resolution of a request stays out of line.",
        operation="resolve.first_resolution",
        dimension=INLINING,
        bound=Bound(kind=BoundKind.NEVER),
        domain=Domain.RUNTIME,
    ),
    Contract(
        id="inline_register_type",
        description="Registering a type stays out of line.",
        operation="register_type",
        dimension=INLINING,
        bound=Bound(kind=BoundKind.NEVER),
    ),
    Contract(
        id="inline_diagnostics_throw_never",
        description="Throwing a diagnostic stays out of line.",
        operation="diagnostics.throw",
        dimension=INLINING,
        bound=Bound(kind=BoundKind.NEVER),
    ),
    Contract(
        id="inline_diagnostics_throw_cold",
        description="Throwing a diagnostic is placed in a cold section.",
        operation="diagnostics.throw",
        dimension=INLINING,
        bound=Bound(kind=BoundKind.COLD),
    ),
    # Header cost
    *(
        Contract(
            id=f"header_cost_{name}",
            description=f"The preprocessed Dingo lines of the {name.replace('_', ' ')} header stay within the seeded limit.",
            operation=f"header.{name}",
            dimension=HEADER_COST,
            bound=at_most(lines, "preprocessed_lines"),
        )
        for name, lines in HEADER_LINES
    ),
)
