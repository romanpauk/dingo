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
- `runtime_binding::kernel_policy` supplies erased storage and rollback.
- `binding_core` has no static/runtime branch; it calls the policy.
- The direct static value path also uses the core.

The conversion path is shared:

- `conversion_descriptor` owns the execution steps and retained cache types.
- `conversion_walk` applies the step list.
- Leaf conversions remain direct primitives; every non-leaf conversion goes
  through the generic `type_conversion` dispatcher.

The model is richer than the C++ descriptor:

- `tools/model/route_derivation.py` derives `RouteCandidate` objects with kind,
  retention, steps, and target shape.
- `test/support/model_probe.h` exposes non-recursive route/step accessors.
- `route_cells` asserts candidate presence, absence, kind, retention, step
  sequence, and target shape.
- `type_list_at` uses C++26 pack indexing, then `__type_pack_element`, then the
  portable inheritance selection.

Runtime lookup is split:

- `runtime/lookup_store.h` owns lookup selection: the index types, routes,
  `select_binding`, `binding_status`, entry traits, and collection candidate
  iteration.
- `runtime/registry.h` keeps execution, registration validation and commit,
  rollback, and data ownership.
- The static and runtime traversals still share the status/ambiguity rules, not
  the traversal itself.

Container operations are shared where they are off the per-link path:

- `detail::container_invoke` in `factory/invoke.h` owns callable dispatch for
  both containers.
- `binding/container_miss.h` owns the miss rule, with runtime/mixed and static
  capability policies.
- `test/container/dependency_chain.cpp` pins both template-depth budgets.

## Recent Commits

- `204a236` Format the shared-kernel handover
- `3b0a0a1` Share the container miss rule
- `0dfde92` Share the container invoke operation
- `d303c83` Guard the static-only dependency chain
- `dc78f48` Extract runtime lookup selection into lookup_store
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

Current probe record: `build/cost/miss2.json`

```text
text:                234248
object:              591448
preprocessed bytes:  2572066
```

Relative to the lookup-split record (`build/cost/split1.json`):

```text
text:                +146
object:              +144
preprocessed bytes:  +4870
```

The growth is the new header boundaries, the shared `invoke` and miss helpers,
and their explicit capability policies. Text growth is flat within measurement
noise. Do not treat wall time from three runs as meaningful.

## Next Work

### 1. Share the collection rule, then the singular engine

- Share collection construct/count/append behind a mode policy. It is off the
  singular per-link path, so it is depth-safe and removes the last non-chain
  duplication.
- Then replace the two singular resolve consumers with one selection component.
  The static-only path tops out at 45 links on GCC, so a per-link wrapper is not
  affordable; the component must replace the existing entry. The M9.2-derived
  target shape and six slices are in the untracked plan under "Container
  unification (M9.2 reference)".
- Then extract `registration_store`.

### 2. Add a runtime-specific cost probe

The current probe mixes static and runtime functions. Add a runtime container
probe that records:

- `runtime_binding` / `runtime_binding_state` object size
- representative runtime container size
- dispatch symbol/text size
- instantiation count from `-ftime-trace`

Without it, runtime layer changes are only measured indirectly.

### 3. Decide runtime state ownership

`runtime_binding_state` is shared across interface bindings, while conversion
cache types are per interface. A single state object therefore cannot own both
without either:

- a shared storage state plus per-interface cache slots, or
- a state parameterized by interface.

Do not merge these blindly.

### 4. Container mode policies

`container_with_static_bindings` still serves static, runtime, and mixed modes
through `void` parameters and `if constexpr`, and `static_container` keeps a
separate resolve/construct consumer. The M9.2 reference is a source protocol
plus one selection engine over capability flags. The static-only depth canary
(45 links on GCC) is the gate: the engine must replace the static consumer, not
wrap it.

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
- `include/dingo/static/activation_set.h` — static policy, source-lifetime
  ladder
- `include/dingo/runtime/runtime_binding.h` — runtime adapter and kernel policy
- `include/dingo/binding/binding_selection.h` — shared selection status
- `include/dingo/runtime/lookup_store.h` — runtime lookup selection
- `include/dingo/runtime/registry.h` — runtime execution and registration
- `include/dingo/binding/container_miss.h` — miss rule and capability policies
- `include/dingo/factory/invoke.h` — shared container callable dispatch
- `test/container/dependency_chain.cpp` — mixed/static depth budgets
- `include/dingo/resolution/resolution_operation.h` — conversion
  descriptor/walker
- `include/dingo/base/type_list.h` — non-recursive type-list indexing
- `tools/model/route_derivation.py` — route candidate rules
- `test/support/model_probe.h` — generated-probe accessors
- `tools/model/templates/route_cells.cpp.j2` — candidate/absence/step/target
  asserts
