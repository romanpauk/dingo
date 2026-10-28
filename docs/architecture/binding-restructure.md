# Binding Restructure

Status: slices 1-8 applied on 2026-10-28. Cost and codegen probes are deferred:
the refactor targets source size first. One probe budget moved by three bytes
and is recorded as a cost follow-up.

This page is the functionality-level plan for the binding layer: which questions
the layer answers, where each answer lived, and how the layer was reduced to one
home per answer.

## Measurement

The binding, selection, and execution headers before and after the slices:

| metric | before | after | change | | --- | ---: | ---: | ---: | | binding
headers | 14 | 14 | one replaced | | lines | 3,767 | 3,390 | -377 (-10%) | |
code lines (no comments/blanks) | 3,028 | 2,685 | -343 (-11%) | | template
declarations | 317 | 277 | -40 (-13%) |

`binding/binding_resolution_policy.h` was deleted and `binding/routes.h` was
added, so the header count is unchanged. `routes.h` is not a new layer; it is
the one home for the route query that was spread across `static/binding.h` and
`binding_resolution.h`.

The layer is not tangled; it was **repeated per backend**, and several questions
were answered more than once inside one backend as fast paths. A static
`resolve<T&>()` passed through twelve named layers, a runtime resolve through
thirteen.

## The Eight Binding Functions

| # | function | home after the slices | | --- | --- | --- | | 1 | binding facts
| `binding/binding_model.h`, `registration/type_registration.h` | | 2 | route
query: does this binding serve the request, and on which route? |
`binding/routes.h` (`binding_route_query`, `binding_is_canonical_leaf_v`,
`binding_is_default_leaf_v`) | | 3 | conversion-cache computation |
`static/storage.h` (per registration), `binding/routes.h` (per interface),
`binding/binding_resolution.h` (`conversion_cache_slot`) | | 4 | selection:
found / missing / ambiguous | `binding/binding_selection.h`,
`static/selection.h`, `runtime/selection.h` | | 5 | source precedence and merge
| `binding/binding_selection.h` (`merge_binding_status`),
`binding/container_selection.h` | | 6 | execution: materialize, apply, convert,
retain | `binding/binding_resolution.h` (free functions), `static/binding.h`
(free functions and policy) | | 7 | collections | `binding/binding_collection.h`
\+ one `collect` per source | | 8 | miss and auto-construction |
`binding/container_miss.h` (`miss_policy`) |

## Applied Slices

1. **Dead surface and model merge.** Removed `forward_resolved_binding`,
   `registered_type_t`, the two default-constructibility traits, and the unused
   binding-model aliases; the two model specializations now share one alias
   base.
2. **One route query.** Added `binding/routes.h` with the route query, route
   matching, the canonical-leaf predicate, and the runtime descriptor match.
   `binding_supports_request` is now a thin wrapper over `binding_route_query`.
   The cache and the route share `binding_is_default_leaf_v`, so the "canonical
   leaf" condition exists once.
3. **One selection and merge.** Deleted `binding_resolution_policy.h` and the
   four-result `binding_result` vocabulary; `merge_binding_status` is the one
   precedence rule. Removed the unused runtime selection accumulator and the
   unused `State` parameter. The generated selection probes now assert the
   merged status directly, so the generator emits less code per row.
4. **One cache computation.** `conversion_cache_slot<TypeList>` replaces the
   enabled/disabled cache base and the `binding_cache_slot` alias; an empty type
   list disables the cache through the empty base. The runtime cache alias
   collapsed to one conditional.
5. **Static execution stack removed.** Deleted `static_binding_core`,
   `static_binding_resolver`, `make_static_binding_resolver`,
   `evaluate_static_binding`, and the dead policy `resolve_request`/`resolve`
   overloads. The static path now calls free functions
   (`resolve_static_binding`, `consume_static_binding`,
   `resolve_static_address`, `consume_static_request`) from the registry.
   `static_registry_state` exposes one `get<BindingModel>()` accessor instead of
   five.
6. **One collection builder.** `collection_traits` lost the static-only
   `has_fixed_size_construct`/`make_fixed_size`/`set` split; both backends
   reserve and append. The runtime registry uses `construct_binding_collection`,
   and `append_binding_collection` was inlined.
7. **One miss policy.** The two policy structs became `miss_policy` values
   (`runtime_only`, `static_only`) and one traits table.
8. **Flatter `container_selection`.** The singular and collection decisions are
   separate, and the selection/miss/runtime/static steps are helpers. The
   singular path keeps its miss calls inline: a helper there costs one template
   level per dependency link, which the static-only graph cannot afford.

## Cost Note

The only probe that moved is `probe_static_resolution_collection_sum` (0x520 to
0x523 on GCC 16), caused by the unified reserve-and-append collection path. The
budget records the three bytes. This is a declared follow-up, not a silent
change: if the fixed-size path is worth restoring, it should come back as a
property of `collection_traits`, not as a second construction function.

## Remaining Work

The next reductions, in order of expected source savings:

1. **`static/bindings.h` validation.** The dependency and key-uniqueness checks
   are ~300 lines of one-time configuration logic. `selected_bindings` and
   `selected_binding` now share `lookup_keys`, but the dependency diagnostic
   family (`binding_declared_dependencies_resolved`,
   `declared_dependency_diagnostic`, `first_missing_declared_dependency`) still
   has several encodings.
2. **Cache computation split.** The per-registration union in `static/storage.h`
   and the per-interface list in `routes.h` are still two views. Passing the
   binding model to the runtime binding would let both use one cacheless-aware
   query.
3. **Selection dual encoding.** Static selection is a type list plus an overload
   index; runtime selection is a pointer and status. The status/merge vocabulary
   is shared, the enumeration is not. This is the representation difference, so
   only the remaining helpers (not the enumeration) should be collapsed.
4. **Runtime registry miss.** `runtime_registry::source_missing` still encodes
   parent fallback and auto-construction separately from `container_miss.h`.
   Unifying it needs an explicit "construction origin" argument, because the
   registry auto-constructs through its root container rather than through
   itself.
5. **`static/graph.h`** is the next functionality after binding. It is the
   largest remaining compile-time header and it repeats some dependency walks
   that `static/bindings.h` also performs.

## Mixed Containers Removed

On 2026-10-28 the mixed mode was removed. A container has exactly one
registration source:

- `container<>` / `runtime_container<>` for runtime registrations;
- `static_container<bindings<...>>` for compile-time bindings.

`container<bindings<...>>` now produces a diagnostic that points at those two
shapes. Registration-local `bindings<...>` are rejected for the same reason: a
registration cannot carry a static source inside a runtime container.

What was removed with the mode:

- the static registry and static resolve/construct paths inside
  `runtime_container_impl`;
- the static/runtime combination branches in `container_selection` and
  `container_operations`;
- the local scope slot, `resolve_local_binding`, and the local-binding universe
  in the static graph;
- the `mixed` miss policy and the two-source selection merge;
- the generated mixed resolution suite and the selection-merge suite.

Source effect:

| area | before | after | | --- | ---: | ---: | | binding/selection/execution
headers | 3,390 | 3,135 | | `container.h` | 818 | 412 | | container front-end
total (`container*`, `static_container`) | 2,103 | 1,587 |

`static_container.h` no longer includes `container.h`, so a static-only
translation unit no longer parses the runtime container implementation.

## Rules

- **One question, one trait.** A shortcut is a branch in the one answer, not a
  new `*_has_direct_*` family.
- **A backend difference is a policy value or an accessor**, never a parallel
  class. `static_binding_policy` versus `runtime_binding` is real state;
  `static_binding_resolver` versus `runtime_binding` was not.
- **No forwarding function with one caller.** Inline it.
- **Diagnostics live with the rule**, not in every caller.
- **Do not touch** the erasure interface, the lookup index, the graph and
  context, the storage traits, or the conversion machinery. They are the parts
  that are expensive to reproduce.
- **Template depth is a hard gate.** A helper on the dependency-resolution path
  costs one level per link. Keep those calls inline; put helpers on the
  collection and static-selection paths, which do not recurse per link.

## Progress Log

- 2026-10-28: plan recorded.
- 2026-10-28: slices 1-8 applied. Full build and 39/39 tests pass. Source
  measurement: 3,767 to 3,390 lines and 317 to 277 template declarations.
  `binding_resolution_policy.h` deleted; `routes.h` added.
- 2026-10-28: the selection generator and its template now assert
  `merge_binding_status` per row instead of the old four-result rule, so the
  generated selection probes are smaller as well.
- 2026-10-28: collection probe budget updated by three bytes; cost work is
  deferred by request.
- 2026-10-28: mixed containers and registration-local bindings removed. Build,
  39/39 tests, `check`, and 211 spec tests pass. `static_container` is now
  include-separated from `container.h`.
