# Shared-Kernel Handover

Status: `feature/shared-kernel` tip `779ce91`.
The working plan stays untracked in `docs/architecture/shared-kernel-next-steps.md`.

## Goal

Unify the static and runtime binding paths incrementally, without porting the
M9.2 spec-driven switch. One commit per cycle. Keep the static path
zero-overhead, keep behavior and generated data stable, and track cost.

## Current State

The execution path is shared:

- `detail::binding_core` owns source materialization, route application,
  conversion retention, and address/value shaping.
- `static_binding_policy` supplies inline storage and the source-lifetime ladder.
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

Runtime lookup work is staged:

- `runtime/lookup_store.h` now owns `runtime_lookup_binding_view`,
  `runtime_lookup_value`, and `runtime_bindings_state`.
- Selection routes, collection iteration, and registration validation still
  live in `runtime/registry.h`.
- Selection status and the runtime accumulator are shared; full traversal is
  not unified.

## Recent Commits

- `779ce91` Extract runtime lookup storage types
- `e93d5c6` Share selection status across static and runtime lookup
- `dadb23e` Make the binding core mode-agnostic
- `8c61ebd` Thin runtime container construction and aliases
- `d0d861c` Thin runtime binding layers
- `144a22c` Route static value evaluation through binding_core
- `192a8b9` Share binding execution through binding_core
- `3d21e09` Dispatch composite conversions through one template
- `8aa843a` Unify conversion route and cache metadata
- `f0ca627` Unify route candidate metadata and probe accessors

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

Current probe record: `build/cost/split1.json`

```text
text:                234102
object:              591304
preprocessed bytes:  2567196
```

Relative to the pre-kernel dispatch record:

```text
text:                +1521
object:              +19448
preprocessed bytes:  +2033
```

The runtime additions are not worse than the previous runtime slice; remaining
text growth is in the static core path. Do not treat wall time from three runs
as meaningful.

## Next Work

### 1. Finish the lookup split

Move from `registry_store` into `lookup_store`:

- `lookup_index_route`
- `runtime_lookup_entry_traits`
- `runtime_lookup_entries_for_interface`
- `select_binding`
- `select_binding_at_interface`
- `select_lookup`
- `make_selection`
- collection candidate iteration

Keep in `registry_store`:

- execution (`resolve_binding`, cache lookup)
- registration validation and commit
- rollback

Then extract `registration_store`.

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
through `void` parameters and `if constexpr`. Extract the mode-specific bodies
behind a thin facade after the registry split.

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

- **Template depth**: the dependency chain is 48 links and the limit is 900.
  Adding one template layer per link can exceed it. The mode-agnostic core only
  fits because the source-lifetime ladder moved into the static policy.
  Do not raise the limit as a fix.
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
- **Cost comparison**: the current zero-tolerance header gate can flag tiny
  byte changes. Record the number and explain it; do not hide it.
- **Untracked plan**: `docs/architecture/shared-kernel-next-steps.md` must stay
  untracked.

## File Map

- `include/dingo/binding/binding_core.h` — shared typed execution kernel
- `include/dingo/static/activation_set.h` — static policy, source-lifetime ladder
- `include/dingo/runtime/runtime_binding.h` — runtime adapter and kernel policy
- `include/dingo/binding/binding_selection.h` — shared selection status
- `include/dingo/runtime/lookup_store.h` — runtime lookup storage
- `include/dingo/runtime/registry.h` — selection/registration (split in progress)
- `include/dingo/resolution/resolution_operation.h` — conversion descriptor/walker
- `include/dingo/base/type_list.h` — non-recursive type-list indexing
- `tools/model/route_derivation.py` — route candidate rules
- `test/support/model_probe.h` — generated-probe accessors
- `tools/model/templates/route_cells.cpp.j2` — candidate/absence/step/target asserts
