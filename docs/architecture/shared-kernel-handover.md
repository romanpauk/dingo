# Shared-Kernel Handover

Status: `feature/shared-kernel`. The working plan stays untracked in
`docs/architecture/shared-kernel-next-steps.md`.

## Goal

Unify the static and runtime binding paths incrementally, without porting the
M9.2 spec-driven switch. One commit per cycle. Keep the static path
zero-overhead, keep behavior and generated data stable, and track cost.

## Current State

The execution path is shared:

- `detail::binding_core` owns source materialization, route application,
  conversion retention, and address/value shaping.
- `static_binding_policy` supplies inline storage and the source-lifetime
  ladder.
- `runtime_binding` and its `runtime_binding_policy` supply erased storage and
  rollback.
- `binding_core` has no static/runtime branch; it calls the policy.
- The direct static value path also uses the core.

The conversion path is shared:

- `conversion_descriptor` owns the execution steps and retained cache types.
- `conversion_walk` applies the step list.
- Leaf conversions remain direct primitives; every non-leaf conversion goes
  through the generic `type_conversion` dispatcher.

The model is richer than the C++ descriptor:

- `tools/model/semantics/routes/derivation.py` derives `RouteCandidate` objects
  with kind, retention, steps, and target shape.
- `test/support/model_probe.h` exposes non-recursive route/step accessors.
- `route_cells` asserts candidate presence, absence, kind, retention, step
  sequence, and target shape.
- `type_list_at` uses C++26 pack indexing, then `__type_pack_element`, then the
  portable inheritance selection.

Runtime lookup is split:

- `runtime/selection.h` owns lookup selection: the index types, routes,
  `select_binding`, `binding_status`, entry traits, and collection candidate
  iteration.
- `runtime/registration_store.h` owns the runtime key validation, lookup commit,
  and rollback; `runtime/registry.h` is execution and data ownership.
- The static and runtime traversals still share the status/ambiguity rules, not
  the traversal itself.

The singular resolution consumer is shared:

- `binding/container_selection.h` (`container_selection<Self>`) owns the source
  order for a singular request: the compile-time static source, the runtime
  selection, their combination, and the hand-off to the miss rule.
- `runtime_container_impl` and `static_container_impl` both derive from it and
  expose capability flags and non-template source accessors. The inherited entry
  replaces the two old resolve bodies, so the engine costs no template level per
  dependency link.
- The static-key collection rule is in the engine too: the source count, the
  explicit-empty lookup, the parent fallback, and the compile-time empty
  collection report are one branch for mixed and static-only containers.
- The static-binding half of `construct` is shared in `static/construction.h`:
  exact binding, normalized binding wrapped into the request, the
  rvalue-conversion diagnostic, and the factory fallback.
- The collection rule is in the engine for every mode: static-key, runtime-only,
  and runtime-key collections. The container exposes only the count, construct,
  and explicit-empty hooks.
- The runtime/mixed outer construct flow is the remaining mode-specific path;
  the static-binding construct rule is already shared.

The module files now use role-parallel names:

```text
runtime/                      static/
  container_traits.h            container_traits.h
  context.h                     context.h
  registry.h                    registry.h
  selection.h                   selection.h
  binding.h                     binding.h
  storage.h                     storage.h
  registration.h                registration.h
  transaction.h                 graph.h
  lookup_index.h                construction.h
                                resolution_policy.h
```

`static/bindings.h` is the compile-time binding source and its dependency rules;
the runtime side normalizes its bindings at registration instead.
`static/storage.h` owns the storage and conversion-cache slots. The activation
set is the registry state: it composes the storage and the local activation sets
a registration declares, and the registry owns the activation set, so the
storage no longer depends on the activation-set definition.

The state model is parallel:

```text
static_registry_state        runtime_registry_state
  static_binding_state         runtime_binding_state
    binding_slot                 binding_slot
    conversion cache             conversion cache
    static_local_scope_slot      runtime_scope_state
static_registry              runtime_registry
static_registry_state_view   (the runtime registry is already a view over its
                              state)
```

`binding/binding_resolution.h` defines `binding_slot<Storage>`; both
`static_binding_state` and `runtime_binding_state` contain one. `binding_core`
reads the storage through `Policy::slot()`. What remains different is the
storage policy (inline typed slots vs the arena) and the dispatch (the static
resolver is created on demand; the runtime binding is a stored erased object).
The source-lifetime ladder is still per-mode.

The container classes are paired:

- `container` (combined), `runtime_container` and `static_container` (facades);
- `runtime_container_impl` and `static_container_impl` (implementations);
- `container_base` (parameter dispatch), `container_collections` (collection
  spellings), `container_selection` (shared source order).

The module classes are named by role:

- runtime: `runtime_registry`, `lookup_selection`, `runtime_storage`,
  `runtime_binding`, `runtime_registration`;
- static: `static_registry`, `static_registry_state`, `static_binding_state`,
  `static_binding_slot`, `static_bindings`, `static_selection`,
  `static_local_scope_slot`, `static_binding_resolver`,
  `static_binding_activation`, `static_binding_policy`,
  `static_registration_requirements`;
- shared: `binding_slot`, `container_invoke`, `resolve_missing_request`,
  `construct_from_static_source`.

Off-chain operations are shared too:

- `detail::container_invoke` in `factory/invoke.h` owns callable dispatch for
  both containers.
- `binding/container_miss.h` owns the miss rule, with runtime/mixed and static
  capability policies.
- `test/container/dependency_chain.cpp` pins both template-depth budgets.

## Recent Commits

- `add623a` Move registration commit and rollback into registration_store
- `9dfe57b` Extract the registration key validation into registration_store
- `5683810` Move the runtime collection branches into the engine
- `db7cbbb` Fold the static binding activation into its policy
- `8863db0` Read binding storage through a shared slot
- `4df7286` Record the static storage split in the handover
- `495273a` Extract the static storage and let the activation set own local
  scopes
- `ecbe0d2` Name the runtime storage, selection, and registration classes by
  role
- `4347641` Name the detail container components after their roles
- `9b03b73` Name the runtime container implementation after its static twin
- `8335dc2` Split the static bindings and registry by role
- `f7036f7` Extract the static selection into static/selection.h
- `1bc6516` Move the static requirements to static/registration.h
- `dfa7bf4` Align the runtime and static module file names
- `86ecb23` Share the static-binding construct rule
- `094ac84` Share the static-key collection rule
- `d3f48c6` Move the static-only resolve onto the shared selection consumer
- `43585b7` Move singular resolution to a shared selection consumer
- `73201c3` Add a runtime-only cost probe
- `204a236` Format the shared-kernel handover
- `3b0a0a1` Share the container miss rule
- `0dfde92` Share the container invoke operation
- `d303c83` Guard the static-only dependency chain
- `dc78f48` Extract runtime lookup selection into lookup_selection
- `c66fde5` Add shared-kernel handover
- `779ce91` Extract runtime lookup storage types
- `e93d5c6` Share selection status across static and runtime lookup
- `dadb23e` Make the binding core mode-agnostic
- `8c61ebd` Thin runtime container construction and aliases
- `d0d861c` Thin runtime binding layers
- `144a22c` Route static value evaluation through binding_core
- `192a8b9` Share binding execution through binding_core

## Verification

Run the configured quality gate:

```bash
cmake --build build -t format
cmake --build build -t cells-check
cmake --build build -j8
ctest --test-dir build -j8
./.venv/bin/python -m pytest -q tools/model tools/cost test/matrix/generator_test.py
cmake --build build -t check
```

Current status:

- `ctest`: 77/77
- pytest: 224 passed
- `check`: passes
- Generated behavior source and `test/baselines/model.json`: unchanged

Current probe record: `build/cost/collections.json`

```text
text:                232228
object:              589752
preprocessed bytes:  2577366
```

Relative to the pre-engine record (`build/cost/miss2.json`):

```text
text:                -2020
object:              -1696
preprocessed bytes:  +5300
```

The runtime-only probe (`build/cost/collections-runtime.json`) records 147523
text, 383488 object, and 2535009 preprocessed bytes, which is -2027 text and
-408 object against the pre-engine record. The shared consumer and collection
rule remove the duplicated bodies; the preprocessed growth is the new header and
the generic mode branches. Wall time is not meaningful.

## Next Work

### 1. Finish construct, then extract `registration_store`

- The singular selection consumer, the collection rule for all modes, and the
  static-binding construct rule landed; both containers use them and the
  static-only depth budget is unchanged.
- The runtime/mixed outer construct flow is runtime-specific: runtime binding
  checks, transactions, normalized requests, factories. It has no static twin;
  folding it is a mode-branch cleanup, not a dedup.
- The registration split is complete: `registration_store` owns validation,
  commit, and rollback; `registry_store` is execution-only.

### 2. Runtime cost probe

`tools/cost/probes/runtime.cpp` and the `cost-record-runtime` target now record
a runtime-only translation unit (`build/cost/current-runtime.json`): text, data,
bss, preprocessed bytes, and wall time. `test/registration/object_sizes.cpp`
already bounds the runtime binding, state, container, and lookup sizes.

Still open: a stable instantiation count for the representative runtime requests
from `-ftime-trace`, so a runtime layer change can be compared by instantiation
as well as by bytes.

### 3. Decide runtime state ownership

`runtime_binding_state` is shared across interface bindings, while conversion
cache types are per interface. A single state object therefore cannot own both
without either:

- a shared storage state plus per-interface cache slots, or
- a state parameterized by interface.

Do not merge these blindly.

### 4. Container mode policies

The source protocol and the singular selection engine are in place. What remains
mode-specific is collection resolution and `construct`; they still use `void`
parameters and `if constexpr` in their containers. The static-only depth canary
(45 links on GCC) is unchanged, which confirms the engine replaced the static
consumer rather than wrapping it.

### 5. Route-list algebra

`type_storage_traits.h` still composes route lists with many hand-written
`*_resolutions` structs. A small route-set algebra could reduce them:

- make / filter by conversion availability
- merge and deduplicate
- reject routes already provided by shared routes
- transform requests to routes

Keep `pair_object_requests` and shared-route semantics explicit. This is lower
priority than the registry split.

## Constraints and Gotchas

- **Template depth**: measured and pinned in `dependency_chain_test`. The mixed
  `container<bindings<...>>` resolves 48 links on GCC and 100 on Clang; the
  static `static_container` resolves 45 on GCC (46 fails) and 100 on Clang. The
  limit is 900. A shared per-link function template is not affordable on the
  static side; replace the existing entry or free a layer first. Do not raise
  the limit as a fix.
- **Static zero-overhead**: no vtable, no erased request, no runtime cache
  members on the static path. The static policy must stay compile-time.
- **Lookup unification**: static candidate is a type, runtime candidate is a
  pointer/state value. A single traversal would require type erasure. Share the
  status/ambiguity rules and predicates, not the traversal.
- **Model/C++ asymmetry**: C++ still constructs the candidate list; Python
  derives the expected list and full step/target contract. This is intentional
  differential verification. Do not generate C++ construction from Python
  without an independent behavior oracle.
- **Unsupported combinations**: keep gaps explicit in `INTERFACE_GAPS`,
  `ANNOTATION_GAPS`, `ABSENCE_PROBE_GAPS`, and `BEHAVIOR_GAPS`. Do not silently
  drop a shape.
- **Cost comparison**: the current zero-tolerance header gate can flag tiny byte
  changes. Record the number and explain it; do not hide it.
- **Untracked plan**: `docs/architecture/shared-kernel-next-steps.md` must stay
  untracked.

## File Map

- `include/dingo/binding/binding_core.h` — shared typed execution kernel
- `include/dingo/static/binding.h` — static binding, activation, and resolver
- `include/dingo/static/storage.h` — `static_binding_slot` and cache primitives
- `include/dingo/static/bindings.h` — static binding types and dependency rules
- `include/dingo/static/selection.h` — `static_selection`, type-level
- `include/dingo/static/registry.h` — static registry
- `include/dingo/static/registration.h` — `static_registration_requirements`
- `include/dingo/runtime/binding.h` — runtime binding and
  `runtime_binding_policy`
- `include/dingo/runtime/storage.h` — `runtime_storage` arena and object store
- `include/dingo/runtime/registration.h` — `runtime_registration` forwarding
- `include/dingo/binding/binding_selection.h` — shared selection status
- `include/dingo/runtime/selection.h` — `lookup_selection`
- `include/dingo/runtime/registry.h` — runtime execution and commit
- `include/dingo/runtime/registration_store.h` — registration validation,
  commit, and rollback
- `include/dingo/binding/container_selection.h` — shared singular selection
  consumer
- `include/dingo/binding/container_miss.h` — miss rule and capability policies
- `include/dingo/static/construction.h` — shared static-binding construct rule
- `include/dingo/factory/invoke.h` — shared container callable dispatch
- `tools/cost/probes/runtime.cpp` — runtime-only cost probe
- `test/container/dependency_chain.cpp` — mixed/static depth budgets
- `include/dingo/resolution/resolution_operation.h` — conversion
  descriptor/walker
- `include/dingo/base/type_list.h` — non-recursive type-list indexing
- `tools/model/semantics/routes/derivation.py` — route candidate rules
- `test/support/model_probe.h` — generated-probe accessors
- `tools/model/templates/route_cells.cpp.j2` — candidate/absence/step/target
  asserts
