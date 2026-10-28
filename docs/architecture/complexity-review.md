# Complexity Review

Status: review snapshot at `d099513` (2026-10-28). Counts, line references, and
ranks describe that tree and will drift.

This page records where accidental complexity lives under `include/dingo/`, what
is worth keeping, and in which order the rest is worth changing. It is a report,
not a gate, and it proposes no behavior change.

## Measurement

The review used the existing reports plus a reading of every header:

```bash
python3 tools/complexity.py --top 30
python3 tools/complexity.py --clusters --neighbours
python3 tools/deadcode.py
cmake --build build -t layers-check
```

The tree has 103 `.h` headers (22,620 lines) plus six compiler backend `.hpp`
files. The layer check passes: the include hierarchy described in
[Overview](overview.md#header-layers) is enforced in practice, not just on
paper.

The complexity score ranks churn against fan-in, so it separates a public
surface from an accumulation. The top of the report:

- `runtime/registry.h` — score 319.0, churn 44, 24 branches, 7 specializations,
  25 forwarders, 1,044 lines, fan-in 7.
- `container.h` — 122.6, churn 116, 36 branches, 2 specializations, 47
  forwarders, 1,033 lines, fan-in 79.
- `storage/shared_cyclical.h` — 92.8, churn 53, 4 branches, 9 specializations, 6
  forwarders, 382 lines, fan-in 7.
- `binding/binding_resolution.h` — 92.6, churn 11, 19 branches, 6
  specializations, 30 forwarders, 718 lines, fan-in 6.
- `resolution/resolution_operation.h` — 88.5, churn 9, 22 branches, 20
  specializations, 32 forwarders, 602 lines, fan-in 6.
- `binding/container_selection.h` — 70.0, churn 4, 16 branches, no
  specializations, 19 forwarders, 281 lines, fan-in 2.
- `type/type_conversion_traits.h` — 52.1, churn 9, 28 branches, 42
  specializations, 2 forwarders, 1,138 lines, fan-in 7.
- `static_container.h` — 47.6, churn 36, 10 branches, 11 specializations, 15
  forwarders, 622 lines, fan-in 21.
- `static/graph.h` — 44.1, churn 18, 6 branches, 13 specializations, 3
  forwarders, 1,115 lines, fan-in 5.
- `type/type_traits.h` — 42.2, churn 15, 12 branches, 32 specializations, 25
  forwarders, 917 lines, fan-in 16.

`registry.h` and `container.h` are not candidates for deletion: they carry the
runtime ownership model and the public orchestration. Their score is high
because their *interfaces* accumulated, not because the work inside is fake.

## Summary

The architecture is over-factored rather than tangled. Three different problems
account for almost all of the accidental complexity:

1. **Dead and speculative surface.** More than twenty declarations are
   unreachable or never read. Removing them changes no behavior and can be
   validated by the build alone.
2. **Forwarding layers.** Deliberate class splits leak a long chain of one-line
   adapters and over a hundred re-exported aliases. The splits are justified;
   the adapters are not.
3. **Rules with more than one encoding.** Seven decisions are written two to
   four times in different vocabularies. This is the only category that needs
   design work, and it is concentrated in route selection.

Only the third category justifies a rewrite. A wholesale reimplementation would
throw away the parts that are hard to reproduce elsewhere: the transaction
journal, the allocation-free static context, and the generated coverage model.

## 1. Dead And Speculative Surface

`tools/deadcode.py` reports type declarations only, which understates this list.
Symbols below were confirmed by `grep` across `include/`, `test/`, `examples/`,
`benchmark/`, and `docs/`.

**`type/type_traits.h`**

- `type_traits<T>::resolve_type` — eight definitions, no call sites anywhere.
- `type_traits<T>::is_rebindable` — eight definitions, never read.
  `is_handle_rebindable` is the member that is actually used.
- `has_type_from_pointer_v` and `is_pointer_like_type_v` — never read.

**`binding/`**

- `count_binding_collection`
  ([binding_collection.h](../../include/dingo/binding/binding_collection.h)) —
  no callers.
- `source_lifetime`
  ([binding_resolution.h](../../include/dingo/binding/binding_resolution.h)) —
  enum with no uses; the dispatchers name lifetimes directly.
- `construct_conversion_for`
  ([binding_resolution.h](../../include/dingo/binding/binding_resolution.h)) —
  one caller, forwards to `construct_conversion_with`.

**`runtime/`**

- `registration_base` ([registry.h](../../include/dingo/runtime/registry.h)) —
  declared alias, never referenced.
- `invalid_registration_return`
  ([registration_store.h](../../include/dingo/runtime/registration_store.h)) —
  declared, never defined; it keeps the diagnostic branch of `prepare_binding`
  well-formed, so keep it with the branch.
- `direct_source_lifetime` ([binding.h](../../include/dingo/runtime/binding.h))
  — constant set to `false`, never read.
- `runtime_binding_policy::storage()` and `static_binding_policy::storage()` —
  never called; `binding_core` reads `slot().storage()`.

**`static/`**

- `static_graph` and `execution_traits<S, true>`
  ([graph.h](../../include/dingo/static/graph.h)) — reachable only from
  `test/registration/binding_graph.cpp`. This is test-covered introspection and
  the alternate bounds analysis, so dropping it is an API decision rather than a
  cleanup. The `RuntimeDependencies` argument of the topology analysis has no
  effect and was removed.
- `static_bindings::model<I>` and `dependencies<I>`
  ([bindings.h](../../include/dingo/static/bindings.h)) — introspection aliases
  used only by `test/registration/static_bindings_source.cpp`; keep as
  test-covered surface.
- `binding_dependency_resolution::status`
  ([bindings.h](../../include/dingo/static/bindings.h)) — computed and never
  read; removed with its enum and `dependency_bindings_are_resolved` helper.
- `collection_count_v`
  ([static_container.h](../../include/dingo/static_container.h)) — never read.
- `static_registry_state*::get_storage`
  ([binding.h](../../include/dingo/static/binding.h)) — reachable only from
  itself.
- `binding_selection` alias
  ([selection.h](../../include/dingo/static/selection.h)) — no users.

One item needs a decision rather than a deletion: `is_owning`, `is_sharing`,
`is_nullable`, and `is_borrowable` are documented as wrapper semantics in
[Extending Dingo](extending-dingo.md), declared in every enabled `type_traits`
specialization, and read by no library code. They are either part of the
extension contract or they are not. The model work in
[Shared-Kernel Next Steps](shared-kernel-next-steps.md) moves toward exactly
these properties, so wiring them into `request_lift` is probably the better
direction; until then the documentation overstates what the library uses.

### Tool gap

`deadcode.py` cannot see member functions, constants, or aliases inside a class,
which is why it reports eight findings while this list has more than twenty. A
second pass over class members would have produced the list above mechanically.

## 2. Forwarding Layers

These add indirection without behavior. Each is safe to collapse.

- **`get_container`, five hops.** `container_proxy::get`
  ([registry.h](../../include/dingo/runtime/registry.h)) →
  `runtime_binding_value_impl_base::get_container`
  ([registration_store.h](../../include/dingo/runtime/registration_store.h)) →
  `runtime_binding_member::get_container` → `runtime_binding::get_container` →
  `runtime_binding_state::container`
  ([binding.h](../../include/dingo/runtime/binding.h)). Four of the five are
  one-line forwards.
- **Identity adapters.** `binding_store()` in the registry returns `*this`; the
  container's version returns its member registry. The former
  `runtime_registration_parent()` and `container()` identity hooks were removed
  (see [Applied Changes](#applied-changes)).
- **Duplicate selectors.** `select_binding` and `source_select` in
  [registry.h](../../include/dingo/runtime/registry.h) have identical bodies.
- **Duplicate resolution relays.** `resolve` and `resolve_collection_type` in
  [registry.h](../../include/dingo/runtime/registry.h) both forward to
  `resolve_binding_request`.
- **`binding_cache_slot`**
  ([binding_resolution.h](../../include/dingo/binding/binding_resolution.h)) is
  an alias of `binding_conversion_cache_base`.
- **`static_binding_resolver`**
  ([static/binding.h](../../include/dingo/static/binding.h)) re-declared its
  policy's aliases and duplicated `registered_type()`.
- **One alias, four names.** `dependency_selector_t` → `dependency_type` →
  `selected` → `dependency`
  ([dependency.h](../../include/dingo/type/dependency.h)).

The runtime registry's base chain (`runtime_registry` → `registry_store` →
`registration_store` → `lookup_selection`) is **not** on this list. The split
keeps instantiation cost proportional to traits and allocator instead of the
full container specialization.

The alias block of over 100 `using` declarations is the price of that split, not
an accident. Each layer's base is dependent, and C++ does not find names in a
dependent base by unqualified lookup, so every layer must re-export the names it
uses. A shared `runtime_registry_types` bundle does not change that: the base of
the bundle is still dependent, so the re-exports would move rather than
disappear. Prune entries when a layer stops using them; do not restructure for
the aliases alone.

## 3. Rules With More Than One Encoding

- **Route precedence** — four encodings: `conversion_kind`
  ([type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)),
  the fallback-candidate chain in the same file, the step list
  ([resolution_operation.h](../../include/dingo/resolution/resolution_operation.h)),
  and storage pairing
  ([type_storage_traits.h](../../include/dingo/storage/type_storage_traits.h)).
- **"Exact binding, else normalized binding"** — `static/bindings.h` (list form
  and choice form), `runtime/selection.h`, and `container.h`'s
  `has_static_construct_v`.
- **"Try exact, on `type_not_convertible` retry normalized and `make`"** — the
  rule lived in `binding_resolution.h` as a helper and in `container.h` four
  more times. It now has one implementation; `container.h` calls the helper. See
  [Applied Changes](#applied-changes).
- **Status merging** — `resolve_binding` is wrapped by `resolve_binding_status`
  and `resolve_combined_selection`, and `container.h::binding_status` merges
  static and runtime states again.
- **Collection construction** — `binding_collection.h` (one user),
  `runtime/registry.h`, `static/binding.h`, and `container.h`'s collection
  relays.
- **"Can this binding serve this request?"** — `static/binding.h` scans a type
  list; `runtime/binding.h` compares a runtime index.
- **Dependency-bound check** — `static/bindings.h` scans bindings for the first
  missing dependency; `static/graph.h` scans graph edges for the same answer.

Some of this split is deliberate. The static path cannot afford the runtime
traversal, and the comment in
[container_selection.h](../../include/dingo/binding/container_selection.h) is
accurate about the template-level cost of wrapping it. The duplication that
matters is the one that is *not* a representation difference: route precedence.
Adding a conversion kind currently requires editing the enum, the fallback
chain, the step list, and the storage pairing. A single `conversion_path`
descriptor computed once in `type_conversion_traits`, consumed by both the
walker and storage pairing, is the one real rewrite this review recommends.

The second rewrite was smaller: `container.h::construct` was 126 lines with a
nested lambda that repeated the default-factory decision. It is now a 31-line
dispatch over `construct_rvalue_request`, `construct_static_transaction`,
`construct_runtime_transaction`, and `construct_mixed_request`. The mixed block
keeps the existing `construct_resolved_request` helper. See
[Applied Changes](#applied-changes).

## 4. What To Keep

- **Runtime machinery.** `runtime_transaction`, `runtime_storage`, the object
  store and arena, and `runtime_binding_state`'s child containers and rollback
  are real journal and lifetime code. Their sizes are pinned by
  [object_sizes.cpp](../../test/registration/object_sizes.cpp).
- **Lookup backends.** `lookup_index` contains real one/many implementations
  with erasure and iteration.
- **Registration store.** Key uniqueness, interface requirements, commit, and
  rollback are load-bearing.
- **`container_selection`.** The one place source order and the miss hand-off
  live; shared by both container shapes.
- **`binding_core`.** The shared materialize → apply route → retain conversion
  execution.
- **Static graph and context.** `static/graph.h` and `static/context.h` are the
  only reason the static path allocates nothing; capacities come from graph
  bounds.
- **Static diagnostics.** `static/bindings.h` produces the missing-dependency
  messages that make the compile-time API usable.
- **Storage policy data.** `storage_scope_policy` plus `scopes.gen.h` describe
  scopes as data rather than per-scope code.
- **Factory detection and MSVC backends.** About 100 duplicated lines out of
  1,250, justified by documented compiler behavior.
- **Three public facades.** `runtime_container` is a stable-address handle by
  contract; `container<>` and `static_container<...>` are documented entry
  points.
- **The type protocol.** `type_traits` wrapper operations, `rebind_type`,
  `normalized_type`, and `key` are the public extension contract and the
  normalization every path depends on.

## 5. Structural Findings

### The static-only facade is not include-separated

[static_container.h](../../include/dingo/static_container.h) includes
[container.h](../../include/dingo/container.h), so a static-only translation
unit still parses 102 of the 103 headers (about 22,900 lines). The three facades
are one monolith at include level; only the resolution entry point differs. If
static-only compilation cost is a goal, this has to be fixed in the include
graph, and no amount of runtime/static unification substitutes for it.

### Two container front-ends, one planned unification

`static_container_impl` repeats the public operations of
`runtime_container_impl`: `resolve_parent`, `resolve_missing`, `unresolved`, the
collection relays, the construct branch tree, and the registration requirement
`static_assert` block. The shared engine under `binding/` covers selection and
execution, not the outer operations. The target design and its slices already
exist in [Shared-Kernel Next Steps](shared-kernel-next-steps.md); this review
agrees with the sequencing there, in particular that the unified entry must
*replace* the static resolve entry rather than wrap it. Template depth is the
hard gate: the static-only container has 45 dependency links of headroom on GCC.

### The runtime registry split costs aliases, not clarity

The four-level chain is justified and the re-export blocks are structural: each
layer's base is dependent, so unqualified lookup cannot see the names and the
`using` declarations are required. `registry_store` importing from its
grandparent (`lookup_selection`) rather than its parent is cosmetic. The
remaining improvement is to remove entries that no layer uses, not to reshape
the chain.

### Lifetime slots (applied)

`storage/shared.h` and `storage/shared_cyclical.h` each split a storage slot
into trivially and non-trivially destructible forms.
`storage/destructible_slot.h` now owns the aligned storage, the constructed
flag, `std::launder`, and destruction, with the same triviality split. `shared`
keeps the resolved flag in its storage object and `shared_cyclical` sets it
before construction, so the cycle contract is unchanged.

## 6. Proposed Order

Each step is independently reviewable. Steps 1–4 change no behavior.

1. **Delete the dead surface** from §1 and decide the `type_traits`
   semantic-flag contract. *(Applied — see
   [Applied Changes](#applied-changes).)* Gate: build and tests.
2. **Collapse the forwarding chains** from §2. *(Applied — see
   [Applied Changes](#applied-changes).)* The registry type bundle is not viable
   (see §5); the alias blocks stay. Gate: build, tests, `object_sizes.cpp`.
3. **Rewrite `container.h::construct`** around the existing
   `construct_resolved_request` helper. *(Applied — see
   [Applied Changes](#applied-changes).)* Gate: behavior baseline unchanged,
   `test-cost` unchanged or better.
4. **Deduplicate the storage slots** and the `lookup.h` trait pairs. *(Applied —
   see [Applied Changes](#applied-changes).)* Gate: `object_sizes.cpp`, resolve
   probes.
5. **Single conversion-path source of truth** (§3), coordinated with the wrapper
   semantics model in `spec/`. *(Slice 1 applied — see
   [Conversion Path](conversion-path.md).)* Gate: model baseline, behavior and
   type-table outcomes unchanged, declared baseline update only if a behavior
   change is intended.
6. **Unify the container front-ends** following the slices in
   [Shared-Kernel Next Steps](shared-kernel-next-steps.md). Gate: 45/48-link
   dependency chains, `test-cost`, and `object_sizes.cpp`.

Steps 1–2 are mechanical and can be one or two commits. Step 5 is the only one
that needs a design note of its own.

## Applied Changes

### Step 1: dead surface

Removed:

- `type_traits<T>::resolve_type` and `is_rebindable`, `has_type_from_pointer`,
  and `is_pointer_like_type_v`, including the test wrapper specializations that
  mirrored the first two
  ([type_traits.h](../../include/dingo/type/type_traits.h)).
- `count_binding_collection`, `source_lifetime`, and `construct_conversion_for`
  ([binding/](../../include/dingo/binding)); the one call site now calls
  `construct_conversion_with` directly.
- `registration_base`, `direct_source_lifetime`, and both policy `storage()`
  accessors ([runtime/](../../include/dingo/runtime),
  [static/binding.h](../../include/dingo/static/binding.h)).
- The unused `static_registry_state*::get_storage` accessors,
  `collection_count_v`, the `binding_selection` alias,
  `dependency_resolution_status`, its `status` members, and
  `dependency_bindings_are_resolved`.
- The no-op `RuntimeDependencies` parameter of the topology analysis and its
  `true` arguments.

Kept deliberately:

- `invalid_registration_return`: the declaration keeps the diagnostic branch of
  `prepare_binding` well-formed.
- `static_graph`, `execution_traits<S, true>`, `model<I>`, and
  `dependencies<I>`: introspection and analysis surface with direct test
  coverage. Removing them is an API decision, not a cleanup.
- `is_owning`, `is_sharing`, `is_nullable`, and `is_borrowable`: documented
  extension contract. Wiring them into `request_lift` belongs to step 5.

Include hygiene: `type_traits.h` no longer uses `base/exceptions.h`, so the
header was dropped there.
[request_lift.h](../../include/dingo/type/request_lift.h) depended on that
transitive include and now includes it directly.

Verification: full build and 39/39 tests.

### Step 2: forwarding layers

Removed:

- The unused identity `runtime_container_impl::container()` overloads
  ([container.h](../../include/dingo/container.h)).
- The identity `runtime_registration_parent()` hooks in the container and the
  registry; the registration base now passes `&self()` directly
  ([runtime/registration.h](../../include/dingo/runtime/registration.h)).
- The duplicate `runtime_registry::source_select`; selection now calls the one
  inherited `select_binding`. The `registry_store` re-export remains because the
  qualified call needs the name.
- `static_registry_state_view`: `static_registry` now calls the state directly,
  removing one object and five forwarders per static call
  ([static/registry.h](../../include/dingo/static/registry.h)).
- The duplicate `static_binding_resolver::registered_type()`; the resolver now
  reaches the policy's through `this->`.
- The redundant `runtime_binding_member::get_container` re-declaration; the
  binding's public accessor is inherited.

Kept deliberately:

- `container_proxy` and the `get_container` chain: it defers the lazily
  constructed child container until first use, so it is semantic, not
  forwarding.
- `binding_cache_slot`: a named alias, zero cost, and three call sites read
  better with it.
- The registry alias blocks: required by dependent-base lookup (see §5).
- `dependency_selector_t` → `dependency_type` → `dependency`: the outer names
  are public composition points and the aliases carry the selector logic.

Verification: full build, 39/39 tests, `format-check`, `layers-check`,
`includes-check`.

### Step 3: construct dispatch

`runtime_container_impl::construct` is now a 31-line dispatch. The three
repeated runtime transactions collapse into `construct_runtime_transaction`, the
two static ones into `construct_static_transaction`, and the nested
default-factory lambda becomes `construct_mixed_request`. The rvalue decision
moves to `construct_rvalue_request`, which also takes the function below the
nesting threshold without a `NOLINT` suppression.

The transaction helpers are member function templates rather than local lambdas:
a non-generic lambda body is instantiated whenever the enclosing `construct` is,
which would instantiate the static path for runtime-only containers. The
templates instantiate only when called.

The two remaining manual retry bodies in `container.h` now call the shared
`construct_resolved_request` helper, so the "exact, then normalized" rule exists
once.

Verification: full build, 39/39 tests, `check` (including `includes-tidy` with
the suppression removed), and `includes-check`.

### Step 4: storage slots and lookup traits

`storage/destructible_slot.h` now owns the inline object storage, the
constructed flag, `std::launder`, and destruction for the storage instances. The
trivial specialization keeps the slot trivially destructible, so the static
graph counts the same destructible slots. `storage_instance_base` and its dtor
split collapse into one base, the wrapper and array copies share the slot, and
the two `cyclical_storage_instance_impl` specializations become one; the
resolved flag stays where the cycle contract needs it.

`lookup.h` merges the key-type and definition variants of contains, duplicate
detection, and matching into one implementation parameterized by whether the
cardinality takes part. Only the two `selected_lookup_entry*_t` aliases are used
outside the header, and both keep their spelling.

Verification: full build, 39/39 tests (including `object_sizes.cpp`),
`resolve-check`, `check`, and `includes-check`.

## Non-Goals

- No big-bang reimplementation. The transaction, storage, graph, and generated
  coverage machinery is the part a rewrite would most likely lose.
- No generated rule tables on the static path.
- No behavior change hidden inside a refactor. Any route or lifetime change is a
  declared baseline update.
- No rename-only churn. The vocabulary in the architecture pages is consistent;
  the problem is duplication, not naming.
