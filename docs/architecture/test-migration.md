# Test Migration Ledger

The switch to the next implementation deletes the current one, its internal
headers, and the tests that use them; this is the step M9.2 of the
[spec-driven design](spec-driven-design.md#m92-switch-and-removal). The ledger
has one entry per test case the switch removes or changes: every case of the
unit tests of `dingo_unit_test`, and every lit test that compiles against the
current implementation. Group H adds one entry per other test family, tool, and
target that builds or probes the current implementation. The removal loses no
coverage when every entry names where its property is tested now, or why it no
longer exists.

Each entry has one disposition:

- **kept**: the test compiles and passes against the next implementation,
  unchanged or with a fixed include, and uses only the public API.
- **ported**: the test is rewritten against the public API. The entry names the
  new test.
- **replaced**: the test is rewritten as a test of the next implementation that
  does the same job, such as a test of an index instead of a lookup backend. The
  entry names the new test and the header of `include/dingo` it tests.
- **covered**: another test asserts the same property. The entry names it.
- **changed**: the behavior changed on purpose. The entry names the behavior
  change and the test of the new behavior.
- **obsolete**: the behavior no longer exists in any form. The entry says why.

A test is named `Suite.Name`, as GoogleTest names it. The `next_*` suites are in
`dingo_next_test`, built from `test/next/`. A behavior change is named by its id
in `spec/content/behavior_changes.py`, such as
`consumed_objects_are_never_borrowed`; the entry there states the baseline, the
new behavior, and the cases that observe it. A diagnostic is named by its id in
`spec/content/diagnostics.py`, such as `lookup_declared_twice`; its generated
lit test is the file of its id under `test/lit/generated/diagnostics/`, written
`gen:<id>` in Group G. An entry is one test case; a typed test is one entry, and
the file-scope `static_assert`s of a file are one entry per property.

## Totals

The 359 entries of Groups A to G are the test cases of the current
implementation; the entries of Group H are its other families, tools, and
targets. One entry counts once, a typed test included.

```text
Group  kept  ported  replaced  covered  changed  obsolete  total
A         9      22        11        0        0         0     42
B         0      30         7        4       10         0     51
C         0       5        48        0        0        21     74
D         0       9        17        0        3         0     29
E         0      40        16        3        9         3     71
F         0      11        24        1        5         0     41
G         0      24         0       15        9         3     51
H        17       0         4        0        4         0     25
all      26     141       127       23       40        27    384
```

## Unresolved

The ledger leaves these open:

- **The container allocator is used for bindings only.** The public `allocator`
  entry says it allocates "the internal data of a container", but the next
  implementation allocates only bindings by it (`runtime_backend::make_entry`);
  the indexes, the memos, the retained objects, and the growth of the
  transaction log use the global allocator. The configuration macros
  `DINGO_CONTEXT_ARENA_BUFFER_SIZE` and `DINGO_RUNTIME_ARENA_BLOCK_SIZE`
  (`config_context_arena_buffer_size`, `config_runtime_arena_block_size`) are
  defined in `base/config.h` and read by nothing.
- **`dense_index::build` writes without a bound check.** A key at or past the
  bound of the index is undefined behavior. A container cannot reach it, as the
  runtime backend rejects such a key before it indexes it, but a direct use of
  the index can.
- **The header cost is not checked.** `probe.py check` measures the preprocessed
  lines of the public headers but does not compare them with the baseline, and
  the spec test that pins the `header_cost` contracts to the baseline is
  commented out, until the contracts are revisited.
- **The contract baselines of GCC 16 and Clang 22 are those of the
  implementation before the switch.** The baselines of GCC 13 and Clang 18 are
  recorded after it; the others need `probe.py update` on their toolchains.
- **MSVC is not built.** The msvc backend of constructor detection is compiled
  and tested with GCC and Clang only; the next implementation has not been built
  with MSVC.
- **Claims not fully confirmed.**
  - `resolution_operation_test.rvalue_source_destroys_materialized_value` is
    covered by `next_unique.retains_the_object_of_an_rvalue_reference`, which
    asserts that the retained objects are destroyed, but not that this happens
    at the end of the scope of the source: next keeps them until the container
    is destroyed. It is not mutation-checked.
  - The entries of `collections_of_a_domain_of_one` in Group B cite the
    selection case
    `selection:runtime/default/impl_0:runtime7/all:runtime7#collection` from the
    case pattern of the behavior change; its observed record was not read.
    `next_lookups.a_collection_of_a_declared_domain_of_one_holds_its_binding`
    tests the same property.
  - Two lit entries of Group G are attributed to `keys_in_every_container`,
    whose baseline text lists neither case; see the note of Group G.
  - `next_diagnostics.names_the_key_and_the_path_of_a_missing_dependency` pins
    the spelling `dingo::key_type<int, 3>` of a fixed key in "index type: ...".
    The spec says only "whose type is `key`", and the baseline cannot be
    compared, as it rejects a fixed key in a runtime registration.
- **A long chain of declared constructors exceeds the constexpr steps of Clang
  18.** The cycle search of the static dependency graph
  (`dependency_graph::find` in `backend/static_backend.h`) grows with the cube
  of the length of a chain, so with `-ftemplate-depth=4096` Clang 18 fails a
  chain of more than 86 links by `-fconstexpr-steps`, not by its depth.

## Group A: container and factory

The cases of `test/container/` and `test/factory/`. Making detection try arity 0
first, in a scratch copy of `include/dingo`, fails the `static_assert`s of
`test/next/constructor/constructor.cpp` and four `next_recursion` tests, and
ignoring `dingo_constructor_type` in `invoke_constructor` fails
`next_construct.auto_constructs_a_dependency_by_its_declared_constructor`. The
other new tests are not mutation-checked.

### test/container/construct_dependency.cpp

- `construct_dependency_test.auto_constructible_trait_defaults_and_opt_in`:
  `dingo::is_auto_constructible` is true for an aggregate, false for a
  non-aggregate class, and true for a class that opts in. **kept**: unchanged;
  it includes only `dingo/type/auto_constructible.h` (public,
  `include_auto_constructible` / `auto_construction`)

### test/container/container_move.cpp

The typed cases run on `container<>`, `container<static_container_traits>`,
`container<static_container_with_dynamic_rtti_traits>`,
`container<dynamic_container_with_static_rtti_traits>`,
`container<dynamic_container_traits, move_test_allocator<char>>`,
`container<dynamic_container_traits, std::allocator<char>, container<>>` and
`runtime_container<>`, as before. The port adds
`#include <dingo/runtime_container.h>`, which next provides
(`include/next/dingo/runtime_container.h`), and copies in the two traits that
came from `test/support/containers.h`, because `dingo_next_test` does not have
`test/` on its include path. Apart from that the test bodies are unchanged.

- `container_move_test.is_movable_but_not_copyable` (typed, 7 types): every
  runtime container is move-constructible and move-assignable, and not copyable.
  **ported**: `test/next/containers/container_move.cpp`
  `next_container_move.is_movable_but_not_copyable`.
- `container_move_test.move_construction_keeps_bindings_and_instances` (typed):
  the moved-to container serves the same instances and bindings after the source
  slot is reused. **ported**: `test/next/containers/container_move.cpp`
  `next_container_move.move_construction_keeps_bindings_and_instances`.
- `container_move_test.move_assignment_replaces_bindings` (typed): move
  assignment takes over the source's bindings and instances and drops the
  target's old ones. **ported**: `test/next/containers/container_move.cpp`
  `next_container_move.move_assignment_replaces_bindings`.
- `container_move_test.moved_container_keeps_registration_containers` (typed):
  the child container returned by `register_type` stays at its address, and
  registering into it after the move still works. **ported**:
  `test/next/containers/container_move.cpp`
  `next_container_move.moved_container_keeps_registration_containers`.
- `container_move_test.moved_container_keeps_static_registration_containers`
  (typed): the same, for a registration that has local compile-time bindings.
  **ported**: `test/next/containers/container_move.cpp`
  `next_container_move.moved_container_keeps_static_registration_containers`.
- `container_move_test.moved_container_keeps_collection_factories`: a collection
  registered with an insertion function still collects after the move.
  **ported**: `test/next/containers/container_move.cpp`
  `next_container_move_untyped.moved_container_keeps_collection_factories`.
- `container_move_test.builder_hands_container_to_host`: a builder moves its
  container into a host, and instances made before the move stay valid.
  **ported**: `test/next/containers/container_move.cpp`
  `next_container_move_untyped.builder_hands_container_to_host`.
- `container_move_parent_test.child_follows_moved_parent` (typed: runtime child,
  `container<bindings, container<>>` child): a child resolves through its parent
  after the parent is moved and the old slot is reused. **ported**:
  `test/next/containers/container_move.cpp`
  `next_container_move_parent.child_follows_moved_parent`. The guard on
  `std::is_move_constructible_v<TypeParam>` became a trait that names the child
  with a compile-time binding of the service, which registers none.
- `container_move_test.compile_time_binding_containers_are_not_movable`:
  `container<bindings<...>>` and `static_container<bindings<...>>` are neither
  movable nor copyable. **ported**: `test/next/containers/container_move.cpp`
  `next_container_move_untyped.compile_time_binding_containers_are_not_movable`
  (the `static_assert`s are `EXPECT_FALSE`s). A mixed container without static
  bindings, `container<bindings<>>`, moves and its children follow it: behavior
  change `mixed_containers_without_static_bindings_move`, tested by
  `next_container_move_untyped.a_mixed_container_without_static_bindings_moves`.

### test/container/dependency_chain.cpp

- `dependency_chain_test.static_chain`: a static chain of detected-constructor
  shared links compiles at the default template depth and resolves to one
  instance. **ported**: `test/next/containers/dependency_chain.cpp`
  `next_dependency_chain.resolves_a_static_chain`. Accepted limit (decision of
  the user): at the default template depth Clang 18 compiles 69 links (baseline
  144\) and GCC 13 52 (baseline 53), so the test uses 64 links on Clang (was 100)
  and 48 on GCC (unchanged)
- `dependency_chain_test.runtime_chain`: a runtime-registered chain of the same
  length resolves to one instance. **ported**:
  `test/next/containers/dependency_chain.cpp`
  `next_dependency_chain.resolves_a_runtime_chain`. It shares `chain_length`, so
  on Clang it now has 64 links instead of 100. Runtime resolution is type-erased
  and does not depend on depth.

### test/container/dingo.cpp

- `dingo_exception_test.make_key_value_type_not_found_exception_context`: the
  not-found message of a keyed request with a path is
  `type not found: R (index type: K) (required by P)` (old internal
  `detail::make_type_not_found_exception`, old header `dingo/base/type_name.h`)
  **replaced**: `test/next/containers/keyed_diagnostics.cpp`
  `next_diagnostics.names_the_key_and_the_path_of_a_missing_dependency`. It goes
  through the public API to `raise_not_found` in `backend/diagnose.h` and the
  generated `raise_type_not_found_keyed_required_by` (spec
  `diagnostics.type_not_found_keyed_required_by`), and checks the whole message.

### test/container/incomplete_resolve.cpp (+ incomplete_resolve_fixture.cpp, incomplete_resolve_shared.h)

- `container_test.resolve_incomplete_pointer_and_reference`: `resolve<T*>` and
  `resolve<T&>` compile and agree where `T` is incomplete, when `T` is
  registered in a TU where it is complete. **ported**:
  `test/next/containers/incomplete_resolve.cpp`
  `next_incomplete_resolve.resolves_a_pointer_and_a_reference_to_an_incomplete_type`
  (+ `test/next/containers/incomplete_resolve_fixture.cpp`,
  `test/next/containers/incomplete_resolve_shared.h`)
- `container_test.construct_registered_type_with_incomplete_reference_dependency`:
  a registered type whose constructor takes a reference to an incomplete type is
  constructed and gets the registered instance. **ported**:
  `test/next/containers/incomplete_resolve.cpp`
  `next_incomplete_resolve.injects_a_reference_to_an_incomplete_type`.

### test/container/parent_container_resolution.cpp

All eight compile and pass against next unchanged, on both compilers and both
standards. They use only public includes (`dingo/container.h`,
`dingo/factory/callable.h`, `dingo/storage/shared.h`) and public names
(`container_parent`, `callable`, `key_value`, `dependency`, `key_type`,
`lookups`/`associative`/`one`/`array`).

- `parent_container_resolution_test.base_shared_dependencies_are_used_by_system_and_channels`:
  shared bindings of a common parent are one instance across sibling children
  and a keyed composition. **kept**: unchanged.
- `parent_container_resolution_test.channels_use_only_their_own_local_binding_instances`:
  each child serves its own local binding; neither the parent nor a sibling's
  parent sees it. **kept**: unchanged.
- `parent_container_resolution_test.channels_use_base_parent_dependency`:
  children inject the parent's shared instance. **kept**: unchanged.
- `parent_container_resolution_test.children_resolve_shared_dependency_from_base`:
  children resolve the parent's shared instance directly. **kept**: unchanged.
- `parent_container_resolution_test.delegating_factory_returns_the_instance_shared_by_children`:
  a factory that delegates to another container's resolution of the same type is
  not a recursion. **kept**: unchanged.
- `parent_container_resolution_test.child_tree_node_binding_delegates_to_base_tree_node`:
  a child's binding shadows the parent's, and its factory can resolve the
  parent's binding of the same type. **kept**: unchanged.
- `parent_container_resolution_test.system_bundles_channels_resolved_from_children`:
  keyed bindings that delegate to children, gathered by keyed `dependency`
  arguments. **kept**: unchanged.
- `parent_container_resolution_test.system_registered_lambda_dynamically_resolves_configured_channel`:
  a callable resolves a runtime key chosen by another binding. **kept**:
  unchanged.

### test/container/recursion.cpp

- `recursion_test.container_default_constructible_cycle`: a shared cycle through
  the largest-arity constructors of default-constructible types is a recursion.
  **ported**: `test/next/containers/recursion.cpp`
  `next_recursion.a_cycle_of_default_constructible_types_is_a_recursion` (body
  unchanged)
- `recursion_test.container_unique_default_constructible_cycle`: the same cycle
  with unique bindings is a recursion. **ported**:
  `test/next/containers/recursion.cpp`
  `next_recursion.a_unique_cycle_of_default_constructible_types`. The
  constructors take `T&&` instead of `T&`, because under
  `consumed_objects_are_never_borrowed` a unique binding no longer serves `T&`
  (unchanged, the old test throws `type_not_convertible_exception`)
- `recursion_test.container_bindings_default_constructible_cycle`: the same
  cycle in `container<bindings<...>>`. **ported**:
  `test/next/containers/recursion.cpp`
  `next_recursion.a_bound_cycle_of_default_constructible_types` (body unchanged)
- `recursion_test.static_container_detected_constructor_cycle`: the same cycle
  in `static_container` throws at runtime. **ported**:
  `test/next/containers/recursion.cpp`
  `next_recursion.a_static_cycle_of_default_constructible_types` (body
  unchanged)
- `recursion_test.static_container_detected_constructor_chain`: an acyclic
  static chain of detected constructors is not a recursion. **ported**:
  `test/next/containers/recursion.cpp`
  `next_recursion.a_static_chain_of_detected_constructors_is_no_recursion` (body
  unchanged)

### test/factory/constructor_detection.cpp

This file includes old internal headers
(`dingo/factory/constructor_detection.h`,
`matrix/fixtures/constructor_detection.h` →
`dingo/factory/detail/constructor_detection_msvc.hpp`). In next, constructor
detection lives in `include/dingo/constructor/constructor.h` and
`constructor_detection.h`, with the backends portable and msvc, selected by the
compiler variant `constructor_detection_msvc`, and the modes arity and
signature.

- (file-scope `static_assert`s, not a TEST): scalar arity 0, cv-qualified scalar
  same, empty aggregate arity 0, empty derived aggregate arity 1, annotated
  aggregate uses its declared constructor, per-type `max_arity`. **replaced**:
  `test/next/constructor/constructor.cpp`
  `next_constructor.value_initializes_a_scalar`,
  `next_constructor.detects_the_constructor_of_the_largest_arity`,
  `next_constructor.takes_the_declared_constructor_arguments`; tests
  `constructor/constructor.h`. The per-type
  `constructor_detection_traits::max_arity` is an old internal, not public API,
  and next has no equivalent.
- `constructor_detection_test.public_constructor_argument_metadata`:
  `constructor<T(int,float)>::arguments`, detected argument types (int, float),
  the `DINGO_CONSTRUCTOR` argument list, a defaulted constructor's empty list.
  **replaced**: `test/next/constructor/constructor.cpp`
  `next_constructor.takes_the_declared_constructor_arguments` (declared
  `(double, const char*)` and `(int)` lists) and
  `next_constructor.detects_the_constructor_of_the_largest_arity` (arity 2 / 0),
  and `next_constructor.the_signature_mode_recovers_the_argument_types` (int,
  float); tests `constructor/constructor.h` and
  `constructor/constructor_detection.h`.
- `constructor_detection_test.unresolved_signature_arguments_fall_back_to_void`:
  old `detail::constructor_signature_arguments` yields `void` for an argument
  category it cannot resolve. **replaced**:
  `test/next/constructor/constructor.cpp`
  `next_constructor.an_unrecorded_argument_category_is_not_recovered`; tests
  `constructor/constructor_signature.h`.
- `constructor_detection_test.traits_limit_only_the_public_search`: a per-type
  arity limit bounds the public search but not an explicit search with a higher
  limit. **replaced**: `test/next/constructor/constructor.cpp`
  `next_constructor.detects_the_constructor_of_the_largest_arity`
  (`find_arity<T, 2>` finds 2 where the unbounded search finds 3); tests
  `constructor/constructor.h`. The only limit in next is
  `DINGO_CONSTRUCTOR_DETECTION_ARGS` (`config_constructor_detection_args`)
- `constructor_detection_test.public_detection_defaults_to_shape`: the public
  `constructor_detection<T>` is the `constructor_arity` detection mode.
  **replaced**: `test/next/constructor/constructor.cpp`
  `next_constructor.the_signature_mode_recovers_the_argument_types` (the type
  tree mode reports no argument types) and
  `next_constructor.the_engine_detects_by_the_selected_backend`; tests
  `constructor/constructor_detection.h`.
- `constructor_detection_test.auto_construction_uses_public_selection`: an
  auto-constructible dependency is made by its `DINGO_CONSTRUCTOR`-selected
  constructor. **ported**: `test/next/containers/construct.cpp`
  `next_construct.auto_constructs_a_dependency_by_its_declared_constructor`.
  This is stronger than the old test: an `int` is registered, so detection alone
  would pick the other constructor.
- `constructor_detection_test.array_traits_construct_move_only_elements_in_place`:
  `std::array<std::unique_ptr<int>,2>` is built from move-only arguments in
  order (old `constructor_traits<std::array>`) **replaced**:
  `test/next/constructor/constructor.cpp`
  `next_constructor.constructs_an_array_of_move_only_handles_in_order`; tests
  `invoke_constructor` (list-initialization) in `constructor/constructor.h`.
- `constructor_detection_test.array_traits_construct_nested_move_only_elements_in_place`:
  the same for a nested array of unique_ptr. **replaced**:
  `test/next/constructor/constructor.cpp`
  `next_constructor.constructs_a_nested_array_of_move_only_handles`.
- `constructor_detection_test.array_traits_construct_nested_move_only_values_in_place`:
  a nested array of move-only values is constructed. **replaced**:
  `test/next/constructor/constructor.cpp`
  `next_constructor.resolves_every_move_only_element_of_a_nested_array`.
- `constructor_detection_test.array_shape_constructs_typed_move_only_elements_in_place`:
  detection of an array resolves each move-only element by its type (2
  resolutions) **replaced**: `test/next/constructor/constructor.cpp`
  `next_constructor.resolves_every_move_only_element_of_an_array`.
- `constructor_detection_test.nested_array_shape_constructs_typed_move_only_elements_in_place`:
  the same for a nested array (4 resolutions) **replaced**:
  `test/next/constructor/constructor.cpp`
  `next_constructor.resolves_every_move_only_element_of_a_nested_array`.
- `constructor_detection_test.selected_alternative_uses_the_conversion_operation`:
  `construct<optional<variant>, constructor<A>>`, doubly nested optional and
  `variant*` build the named alternative. **ported**:
  `test/next/containers/construct.cpp`
  `next_construct.constructs_a_named_alternative_inside_wrappers` (body
  unchanged)
- `constructor_detection_test.ordinary_wrapper_construction_remains_in_place`:
  `optional<immovable>` is constructed in place (old
  `detail::construction_dispatch`) **ported**:
  `test/next/containers/construct.cpp`
  `next_construct.constructs_the_element_of_an_optional_in_place` (through
  `container.construct`)
- `constructor_detection_test.selected_conversion_route_is_executed`: a
  `variant<int,long>` gets its `long` alternative built from an int source (old
  `detail::target_alternative_type_conversion`) **ported**:
  `test/next/containers/construct.cpp`
  `next_construct.a_named_leaf_converts_to_the_alternative_it_names`
  (`constructor<long(int)>`; the leaf converts into the stored variant through
  `constructor_factory` in `conversion/construct.h`)

## Group B: lookup

The cases of `test/lookup/`. The new tests are in `test/next/backend/index.cpp`
(`next_index`) and `test/next/containers/lookups.cpp` (`next_lookups`). They are
not mutation-checked. The covered rows were read and assert the same property.

### test/lookup/lookup_index.cpp

- (file-scope `static_assert`s): which keys count as static
  (`is_static_lookup_key_v`) and what `make_lookup_key` returns for each
  selector. **replaced**: `test/next/backend/index.cpp` file-scope
  `static_assert`s on `key_has_value`, `key_of_t`, `request_key_of`; tests
  `lookup/key.h`.
- `index_test.lookup_backend_inserts_key_value_rows`: a `one`/ordered key-value
  backend finds the row of a key, refuses a second row, and forgets the row once
  it is erased. **replaced**:
  `next_index.finds_the_candidate_of_a_key_value_and_no_other` and
  `next_index.a_rebuilt_index_holds_only_the_candidates_of_its_last_build`;
  tests `lookup/index.h`. In next the backend rejects duplicates, not the index:
  see `next_keys.rejects_a_second_binding_of_a_key` and
  `next_lookups.an_unordered_domain_of_one_selects_by_key_value`.
- `index_test.lookup_backend_iterates_many_key_value_rows`: a `many`/unordered
  backend visits every row of a key, and erasing one leaves the other.
  **replaced**:
  `next_index.visits_the_candidates_of_a_key_in_order_until_told_to_stop` and
  `next_index.a_rebuilt_index_holds_only_the_candidates_of_its_last_build` (both
  run linear, sorted, hashed and dense); tests `lookup/index.h`.
- `index_test.lookup_backend_inserts_no_key_rows`: a no-key `one` backend holds
  a row, refuses a second, finds it, and erases it. **replaced**:
  `next_index.a_memo_of_a_key_without_a_value_visits_every_candidate`
  (`no_key`); tests `backend/memo.h`. The rejection is
  `next_lookups.a_second_binding_of_a_key_of_a_domain_of_one_is_rejected`.
- `index_test.lookup_backend_inserts_key_type_rows`: a tag-key `one` backend
  holds a row, finds it, and erases it. **replaced**:
  `next_index.a_memo_of_a_key_without_a_value_visits_every_candidate`
  (`tag_key`); tests `backend/memo.h`.
- `index_test.lookup_backend_iterates_many_static_key_rows`: a no-key `many`
  backend visits its rows in insertion order, and after the first is erased
  `find` gives the next. **replaced**:
  `next_index.a_memo_of_a_key_without_a_value_visits_every_candidate` (checks
  order, stopping after the first, and clear followed by re-index); tests
  `backend/memo.h`.
- `index_test.lookup_index_get_returns_lookup_backend`: the lookup index gives
  each lookup entry its own backend. **replaced**:
  `next_index.a_domain_is_indexed_by_the_index_of_its_declaration` and
  `next_index.a_default_declaration_indexes_every_other_domain`; tests
  `declaration_list` in `lookup/declarations.h` and `backend_indexing` in
  `dingo/lookup/lookup.h`.

### test/lookup/index_registration.cpp

- `index_test.normal_unkeyed_singular_registration_uses_allocator_owned_storage`:
  an unkeyed binding, including a large one, is allocated from the container
  allocator and released. **ported**: `test/next/containers/lookups.cpp`
  `next_lookups.a_binding_is_allocated_from_the_container_allocator` (case
  `keyed false`)
- `index_test.shared_resolution_without_local_bindings_avoids_allocation`:
  resolving a shared binding allocates nothing more from the container
  allocator. **ported**:
  `next_lookups.resolving_a_shared_binding_allocates_nothing_more`.
- `index_test.typed_key_singular_registration_uses_allocator_owned_storage`: the
  same as the unkeyed allocation test, for a tag-keyed binding. **ported**:
  `next_lookups.a_binding_is_allocated_from_the_container_allocator` (case
  `keyed true`)
- `index_test.normal_unkeyed_duplicate_storage_rejects_no_key_one_row`: a second
  unkeyed binding is rejected, the first stays, and a collection holds one
  element and is reserved for 1. **ported**:
  `next_lookups.a_second_binding_of_a_key_of_a_domain_of_one_is_rejected`; the
  reserve part is `next_lookups.a_collection_is_reserved_for_its_bindings`.
- `index_test.normal_unkeyed_collection_uses_no_key_one_rows`: after an unkeyed
  duplicate is rejected, a collection holds the first binding (reserved for 1)
  **ported**:
  `next_lookups.a_second_binding_of_a_key_of_a_domain_of_one_is_rejected`; the
  reserve part is `next_lookups.a_collection_is_reserved_for_its_bindings`.
- `index_test.failed_normal_unkeyed_registration_preserves_binding`: with
  `single<>` declared, a duplicate is rejected, the same instance is kept and
  constructed once, and a collection request throws `type_not_found`.
  **changed**: `collections_of_a_domain_of_one`. The new behavior (the
  collection holds the one binding; the instance is kept and constructed once)
  is tested by
  `next_lookups.a_collection_of_a_declared_domain_of_one_holds_its_binding`.
- `index_test.failed_normal_unkeyed_registration_preserves_collection_rows`:
  after an unkeyed duplicate is rejected, the same instance is kept, the
  collection holds it, and it was constructed once. **ported**:
  `next_lookups.a_second_binding_of_a_key_of_a_domain_of_one_is_rejected`
  (checks same address, a collection of one, and `counted::constructions`)
- `index_test.key_valueed_collection_requires_associative_many_lookup`: a
  collection of a key value in an `associative<..., one>` domain throws
  `type_not_found`, while a singular request succeeds. **changed**:
  `collections_of_a_domain_of_one`; new behavior tested by
  `next_lookups.a_collection_of_a_declared_domain_of_one_holds_its_binding`
  (part `by_value`), and in the selection table by
  `selection:runtime/default/impl_0:runtime7/all:runtime7#collection` (declared
  in the change)
- `index_test.implicit_typed_key_duplicate_storage_rejects_typed_key_one_row`: a
  second tag-keyed binding is rejected, the first stays, and a collection holds
  one (reserved for 1) **ported**:
  `next_lookups.a_second_binding_of_a_key_of_a_domain_of_one_is_rejected` (part
  `tagged`); the reserve part is
  `next_lookups.a_collection_is_reserved_for_its_bindings`.
- `index_test.failed_implicit_typed_key_registration_preserves_collection_rows`:
  after a tag-keyed duplicate is rejected, the same instance is kept, the
  collection holds it, and it was constructed once. **ported**:
  `next_lookups.a_second_binding_of_a_key_of_a_domain_of_one_is_rejected` (part
  `tagged`)
- `index_test.key_value_one_lookup_uses_equal_distinct_key_object`: a user key
  type is found by an equal but distinct object, giving the same instance.
  **ported**: `next_lookups.a_key_value_is_found_by_an_equal_distinct_object`.
- `index_test.key_value_many_lookup_uses_value_semantics`:
  `associative<user_key, I, many>` gathers the bindings of a key in order, a
  singular request is ambiguous, and another key resolves. **ported**:
  `next_lookups.a_domain_of_many_holds_every_binding_of_a_key_in_order`
  (`comparable_key`, ordered)
- `index_test.associative_alias_behaves_like_key_value_one`:
  `associative<size_t, I>` is the internal key-value `one` definition: it
  rejects a duplicate, and a collection throws `type_not_found`. **changed**:
  `collections_of_a_domain_of_one`; tested by
  `next_lookups.a_collection_of_a_declared_domain_of_one_holds_its_binding`
  (part `by_value`: resolves the key, rejects the duplicate, collection `{1}`).
  The `static_assert` on the old internal alias is dropped.
- `index_test.associative_many_alias_behaves_like_key_value_many`:
  `associative<size_t, I, many>` gathers the bindings of a key in order, a
  singular request is ambiguous, and another key resolves. **ported**:
  `next_lookups.a_domain_of_many_holds_every_binding_of_a_key_in_order`
  (`std::size_t`, ordered). The internal `static_assert` is dropped.

### test/lookup/associative_backends.cpp

- `associative_backend_test.unordered_one_uses_key_value_lookup`: an unordered
  `one` domain resolves a key, rejects a duplicate and keeps the instance, and
  accepts another key. **ported**:
  `next_lookups.an_unordered_domain_of_one_selects_by_key_value`.
- `associative_backend_test.unordered_many_resolves_duplicate_key_entries`: an
  unordered `many` domain gathers both bindings of a key, and a singular request
  is ambiguous. **ported**:
  `next_lookups.a_domain_of_many_holds_every_binding_of_a_key_in_order`
  (`std::size_t`, unordered; next also guarantees the order)
- `associative_backend_test.array_one_uses_dense_key_lookup`: `array<4>` `one`:
  a key inside resolves, a registration outside throws `std::out_of_range`, and
  a request outside is not found. **changed**:
  `out_of_range_keys_raise_the_index_exception`; tested by
  `next_lookups.a_dense_array_domain_of_one_keeps_its_bindings_on_a_reject` and
  `next_keys.a_dense_array_never_stores_a_key_outside_its_bound`.
- `associative_backend_test.array_one_out_of_range_registration_rolls_back_key_value_rows`:
  a rejected out-of-range registration leaves nothing behind, and a key inside
  can still be registered. **changed**:
  `out_of_range_keys_raise_the_index_exception`; tested by
  `next_lookups.a_dense_array_domain_of_one_keeps_its_bindings_on_a_reject`
  (`array<1>`, same sequence)
- `associative_backend_test.array_many_uses_dense_key_rows`: `array<4>` `many`:
  duplicates of a key gather (2, then 3), a singular request is ambiguous,
  another key resolves, and a registration outside throws. **changed**:
  `out_of_range_keys_raise_the_index_exception`; tested by
  `next_lookups.a_dense_array_domain_of_many_keeps_its_bindings_on_a_reject`.
- `associative_backend_test.array_many_out_of_range_registration_rolls_back_key_value_rows`:
  a rejected out-of-range `many` registration leaves no element (the collection
  of that key is empty), and a key inside works. **changed**:
  `out_of_range_keys_raise_the_index_exception`; tested by
  `next_lookups.a_dense_array_domain_of_many_keeps_its_bindings_on_a_reject`
  (checks an empty collection of key 4 before and after)
- `associative_backend_test.custom_backend_uses_stl_like_try_emplace`: a user
  `storage<>` backend is constructed and used for insertion (`try_emplace` once,
  `emplace` never) and resolution. **changed**: `index_extension_point`: a
  backend names its index, `index<Value, Candidate>` over `dingo::index_item`
  values, and a map is no longer a backend, so no insertion member is called.
  New behavior: `next_lookups.an_index_of_the_user_indexes_its_domain` (the
  index of the user is built and visited, and resolves each key)
- `associative_backend_test.parent_fallback_searches_each_container_once`:
  through grandparent, parent and child, a resolution searches each container's
  backend once (3 finds) **ported**:
  `next_lookups.a_parent_fallback_searches_each_container_once` (a counting
  index per container: a repeated request of the child searches 3 times, as the
  baseline did)
- `associative_backend_test.custom_backend_storage_uses_arbitrary_mapped`: the
  test's own `custom_lookup_backend::storage` works with any mapped type under
  one/many semantics. **changed**: `index_extension_point`: a backend has no
  storage map of a mapped type; it names an index of `dingo::index_item` values.
  New behavior:
  `next_memo.an_index_of_the_user_indexes_in_the_order_of_registration` and
  `next_lookups.an_index_of_the_user_indexes_its_domain`.
- `associative_backend_test.custom_backend_prefers_try_emplace_over_operator_subscript`:
  a user backend's `operator[]` is never used for insertion. **changed**:
  `index_extension_point`: the index of a backend is filled by its `build`, not
  by members of a map. New behavior:
  `next_lookups.an_index_of_the_user_indexes_its_domain`.

### test/lookup/index_injection.cpp

- `index_test.external_lookup_constructs_string_key_for_indexed_injection`
  (C++20): a constructor parameter with a custom string key selector (a
  `detail::` hook) injects from an `associative<std::string>` domain.
  **ported**: `next_lookups.injects_a_fixed_string_key_into_a_string_domain`
  (C++20 only). It uses the public `key_type<std::string, fixed_string{"json"}>`
  instead of the internal hook.
- `index_test.constructor_injects_fixed_indexed_enum_key`:
  `dependency<I&, key_type<Enum, V>>` injects from `associative<Enum, I>`.
  **ported**: `next_lookups.injects_a_fixed_enumeration_key`.
- `index_test.selected_type_selector_registers_and_injects_keyed_binding`: a
  binding keyed by a tag (spelled with the internal
  `detail::selected<type_selector>`) is injected into a parameter that names the
  tag. **covered**: `next_keys.selects_by_tag_and_by_value`
  (`consumer<key_type<tag_a>>` gets the tag_a binding; `key_type<tag_b>` is not
  found)
- `index_test.selected_value_selector_injects_indexed_binding`: a parameter with
  a fixed value key (spelled with the internal `detail::value_selector`) is
  injected from a key-value binding. **covered**:
  `next_keys.selects_by_tag_and_by_value` (`consumer<key_type<int, 7>>` finds
  `key_value{7}`); with a declared associative domain,
  `next_lookups.injects_a_fixed_enumeration_key`.
- `index_test.annotated_interfaces_keep_independent_indexes`: two annotated
  interfaces, each declared associative, keep separate key indexes. **covered**:
  `next_annotated.selects_annotated_interfaces_by_key` (stricter: both register
  the same key 1 and each resolves its own)
- `index_test.DISABLED_constructor_injects_string_literal_key_into_string_index`
  (only under `DINGO_ENABLE_FUTURE_KEY_STRING_TESTS`): the future
  `key_string<std::string, "json">` injects from a string domain. **ported**:
  `next_lookups.injects_a_fixed_string_key_into_a_string_domain` (C++20). The
  property can now be expressed with `key_type` and a structural string; the
  name `key_string` does not exist.

### test/lookup/index_unkeyed.cpp

- `index_test.base_one_ordered_resolves_implicit_static_keys`:
  `base<one, ordered>` resolves no-key and tag-key bindings singly and as
  collections of one. **ported**:
  `next_lookups.a_default_declaration_of_one_holds_a_binding_of_each_key`
  (ordered; also checks that a duplicate is rejected)
- `index_test.base_one_unordered_resolves_implicit_static_keys`: the same with
  `base<one, unordered>`. **ported**:
  `next_lookups.a_default_declaration_of_one_holds_a_binding_of_each_key`
  (unordered)
- `index_test.base_many_ordered_enumerates_implicit_static_keys`:
  `base<many, ordered>` gathers two no-key and two tag-key bindings, and
  singular requests are ambiguous. **ported**:
  `next_lookups.a_default_declaration_of_many_holds_any_number_of_each_key`
  (ordered; also checks key values and order)
- `index_test.base_many_unordered_enumerates_implicit_static_keys`: the same
  with `base<many, unordered>`. **ported**:
  `next_lookups.a_default_declaration_of_many_holds_any_number_of_each_key`
  (unordered)
- `index_test.base_many_enumerates_implicit_unkeyed_collection_entries`:
  `base<many, ordered>` gathers two no-key bindings, and a singular request is
  ambiguous. **ported**:
  `next_lookups.a_default_declaration_of_many_holds_any_number_of_each_key`.
- `index_test.base_uses_configured_backend_for_default_index`:
  `base<one, user backend>` selects that backend (internal `static_assert`) and
  resolves. **ported**:
  `next_lookups.a_default_declaration_names_the_backend_of_other_domains` (an
  undeclared key-value domain is indexed by the index of the user's backend).
  The type-level part is
  `next_index.a_default_declaration_indexes_every_other_domain`.
- `index_test.collection_alias_behaves_like_no_key_many_lookup`: `collection<I>`
  gathers two no-key bindings in order, and a singular request is ambiguous.
  **covered**: `next_collections.a_domain_declared_many_holds_any_number`
  (traits declare `collection<iface>`; checks `{0, 1}` in order and an ambiguous
  singular request)

### test/lookup/index_runtime.cpp

- `index_test.exception_message_index_out_of_range_negative`: the message of the
  out-of-range exception for key -1 is "type index out of range: key type int".
  **ported**: `next_lookups.a_key_outside_a_dense_array_names_its_key_type`
  (registers `key_value{-1}` in `array<2>` and checks `what()` exactly; no
  `detail::` factory is used)
- `index_test.explicit_lookup_resolves_with_custom_container_allocator`: a
  string-keyed associative domain works with a user container allocator.
  **ported**:
  `next_lookups.a_container_allocator_allocates_the_key_values_it_holds`
  (`string_traits` part)
- `index_test.key_value_lookup_backend_uses_rebound_allocator`: key values are
  allocated from the container allocator (largest allocation is at least the
  size of a 4 KiB key) and released. **ported**:
  `next_lookups.a_container_allocator_allocates_the_key_values_it_holds`
  (`large_traits` part). In next the key is stored in the binding, which the
  container allocator allocates; the memo's own copy uses `std::allocator`.
- `index_test.indexed_container_releases_rebound_allocator_storage`: a `many`
  domain with a user allocator releases everything it allocated. **ported**:
  `next_lookups.a_container_allocator_allocates_the_key_values_it_holds`
  (`many_traits` part)
- `index_test.associative_many_allows_multiple_values_per_key_value`:
  `associative<size_t, I, many>`: both bindings of key 7, an ambiguous singular
  request, key 8 resolves, key 9 not found. **ported**:
  `next_lookups.a_domain_of_many_holds_every_binding_of_a_key_in_order`
  (`std::size_t`, ordered)

### test/lookup/index_typed.cpp

- `index_test.typed_key_many_populates_custom_collections`:
  `typed<tag, I, many>` fills a user collection (`collection_traits`) with both
  bindings, reserved for 2. **ported**:
  `next_lookups.a_typed_domain_of_many_fills_a_collection_of_the_user`; the
  reserve part is `next_lookups.a_collection_is_reserved_for_its_bindings`.
- `index_test.base_many_enumerates_implicit_typed_key_collection_entries`:
  `base<many>` gathers two tag-key bindings, and a singular request is
  ambiguous. **ported**:
  `next_lookups.a_default_declaration_of_many_holds_any_number_of_each_key`.

## Group C: memory, runtime, and type

The old arena (`dingo/memory/arena_allocator.h`), object store
(`dingo/memory/object_store.h`), container runtime, and runtime transaction
(`dingo/runtime/*`) have no one-to-one counterpart. In the next implementation
each binding is one allocation from the container allocator
(`memory/allocation.h`); `memory/retained.h` keeps the objects of a container,
which a failed transaction destroys back to its mark; and
`basic_transaction_log` of `memory/lifetime_slot.h` is the undo log of the
requests of a container, in which every request undoes its own effects when it
fails. The next implementation has no commit or finish actions, no action state,
no scratch or inline arena, and no block-size policy, which is why several rows
are obsolete.

### test/memory/arena_allocator.cpp

- `arena_allocator_test.buffer_backed_arena_allocates_overflow_blocks`: a
  buffer-backed arena serves from its buffer and takes overflow blocks from its
  allocator. **obsolete**: next has no arena and no caller-supplied buffer: each
  binding is its own allocation from the container allocator, tested by
  `test/next/containers/allocator.cpp`
  `next_allocator.a_container_allocates_its_bindings_from_its_allocator`.
- `arena_allocator_test.buffer_backed_reset_releases_overflow_and_rewinds`:
  reset gives the overflow blocks back to the allocator and rewinds to the
  buffer. **replaced**: `test/next/memory/allocation.cpp`
  `next_allocation.frees_the_bytes_it_allocated` (same pointers and byte counts
  handed back); tests `memory/allocation.h`. Rewinding has no counterpart.
- `arena_allocator_test.checkpoint_rewind_reuses_storage_after_mark`: rewinding
  to a mark drops the allocations after it. **replaced**:
  `test/next/memory/retained.cpp`
  `next_retained.a_failed_transaction_destroys_the_objects_kept_during_it`,
  `next_retained.objects_kept_after_an_undo_follow_the_remaining_ones`; tests
  `memory/retained.h` (undo to the mark). Address reuse is arena-specific and
  obsolete.
- `arena_allocator_test.checkpoint_rewind_releases_later_overflow_blocks`:
  rewinding frees the blocks allocated after the mark. **replaced**:
  `test/next/containers/allocator.cpp`
  `next_allocator.a_failed_request_frees_the_binding_it_registered` (an
  allocation made during a failed transaction goes back to the allocator); tests
  `backend/runtime_backend.h` undo_add + `allocation.h`.
- `arena_allocator_test.inline_arena_uses_its_embedded_buffer`: an inline arena
  serves from storage inside itself. **obsolete**: next has no inline arena. Its
  nearest relative, the inline journal of `basic_transaction_log<Inline>`, is
  checked for correctness past its inline capacity by
  `next_transaction_log.undoes_more_effects_than_it_records_inline`. Staying
  allocation-free is a codegen property that no test checks.
- `arena_allocator_test.heap_arena_reset_releases_all_blocks`: reset gives every
  owned block back to the allocator. **replaced**:
  `test/next/containers/allocator.cpp`
  `next_allocator.a_container_allocates_its_bindings_from_its_allocator` (when
  the container is destroyed, deallocations equal allocations and freed bytes
  equal allocated bytes)
- `arena_allocator_test.small_owned_blocks_are_not_rounded_to_a_page`:
  block-size policy: small blocks are not rounded to a page. **obsolete**: next
  has no blocks. It allocates exactly size + alignment + header per object
  (`allocate_aligned`); byte balance is checked in
  `next_allocation.frees_the_bytes_it_allocated`.
- `arena_allocator_test.small_owned_blocks_do_not_grow`: block-size policy:
  block size stays fixed. **obsolete**: as above: no blocks in next.
- `arena_allocator_test.oversized_block_does_not_change_following_size`:
  block-size policy after an oversized block. **obsolete**: as above: no blocks
  in next.
- `arena_allocator_test.allocation_rejects_block_size_overflow`: a runtime-sized
  request that would overflow is refused without allocating. **obsolete**: next
  never allocates a runtime-sized block: `allocate_aligned` is called only with
  `sizeof(Type)` from `allocate_object`, so its sum of size, alignment and
  header cannot overflow, and it has no check.
- `arena_allocator_test.large_owned_blocks_are_rounded_to_a_page`: block-size
  policy: large blocks are rounded to a page. **obsolete**: as above: no blocks
  in next.

### test/memory/object_store.cpp

- `object_store_test.object_store_destroys_constructed_objects_in_reverse_order`:
  stored objects are destroyed in reverse order of construction. **replaced**:
  `test/next/memory/retained.cpp`
  `next_retained.destroys_its_objects_in_the_reverse_order_they_were_kept`;
  tests `memory/retained.h`.
- `object_store_test.object_store_first_destructor_record_stays_in_inline_arena`:
  the first destructor record fits the inline arena. **obsolete**: next has no
  destructor records and no inline arena: every kept object is a heap holder
  node that carries its own destroy function.
- `object_store_test.object_store_destroy_checkpoint_does_not_rewind_arena`:
  destroying to a checkpoint destroys later objects while earlier memory stays
  valid. **replaced**:
  `next_retained.an_object_is_destroyed_before_the_objects_kept_before_it` (an
  observer reads an earlier kept int from its destructor during the undo); tests
  `retained.h` `undo_to_mark`.
- `object_store_test.object_store_destroy_preserves_reverse_order_across_linked_nodes`:
  39 objects across record nodes are destroyed in exact reverse order.
  **replaced**:
  `next_retained.a_failed_transaction_destroys_the_objects_kept_during_it` (39
  objects, exact reverse order)
- `object_store_test.object_store_trivial_construction_does_not_register_destructor_records`:
  a trivially destructible object registers no destructor record. **obsolete**:
  an optimization of the old record list. Next keeps every object in a holder
  and has no record to leave out.

### test/runtime/container_runtime.cpp

- `container_runtime_test.container_runtime_destroys_constructed_objects_in_reverse_order`:
  the runtime destroys its objects in reverse order. **replaced**:
  `test/next/memory/retained.cpp`
  `next_retained.destroys_its_objects_in_the_reverse_order_they_were_kept`;
  tests `memory/retained.h`.
- `container_runtime_test.container_runtime_rollback_destroys_objects_after_checkpoint`:
  rollback destroys only the objects after the mark, in reverse; earlier ones
  live until the end. **replaced**:
  `next_retained.a_failed_transaction_destroys_the_objects_kept_during_it`
  (object 0 survives the undo and is destroyed with its owner),
  `next_retained.a_transaction_that_returns_keeps_its_objects`.
- `container_runtime_test.container_runtime_rollback_destroys_before_rewinding_arena`:
  rollback destroys an object while the earlier object it reads still exists.
  **replaced**:
  `next_retained.an_object_is_destroyed_before_the_objects_kept_before_it`.
- `container_runtime_test.container_runtime_rollback_preserves_reverse_order_across_linked_nodes`:
  rollback of 39 objects keeps exact reverse order; the object before the mark
  survives. **replaced**:
  `next_retained.a_failed_transaction_destroys_the_objects_kept_during_it`.
- `container_runtime_test.container_runtime_rollback_checkpoint_leaves_valid_linked_tail`:
  after a rollback, newly kept objects chain to the survivors and are destroyed
  first. **replaced**:
  `next_retained.objects_kept_after_an_undo_follow_the_remaining_ones` (32 + 1
  objects, same assertions)

### test/runtime/transaction.cpp

- `runtime_transaction_test.runtime_transaction_rollback_allows_following_committed_allocation`:
  after a rollback, the next transaction commits and keeps its objects.
  **replaced**: `test/next/memory/retained.cpp`
  `next_retained.a_transaction_after_a_failed_one_keeps_its_objects`; tests
  `retained.h` + `slot.h` `basic_transaction_log`.
- `runtime_transaction_test.runtime_transaction_without_persistent_allocations_is_harmless`:
  a transaction with no objects destroys nothing, whether it commits or rolls
  back. **replaced**:
  `next_retained.a_transaction_after_a_failed_one_keeps_its_objects` (first
  block: a failed transaction that kept nothing destroys nothing),
  `next_transaction_log.a_request_that_returns_keeps_its_effects`.
- `runtime_transaction_test.runtime_transaction_destroys_action_states_after_commit_callbacks`:
  commit actions run in reverse, then their states are destroyed. **obsolete**:
  next has no commit actions and no action state. An entry is a stateless
  (target, undo, context) triple (`slot.h` `transaction_entry`), and a returning
  request "checks nothing". The old library used no `on_commit` (grep
  include/dingo)
- `runtime_transaction_test.runtime_transaction_first_action_stays_in_inline_arena`:
  the first action record fits the scratch inline arena. **obsolete**: no
  scratch arena in next. The inline journal is internal; see
  `next_transaction_log.undoes_more_effects_than_it_records_inline`.
- `runtime_transaction_test.runtime_transaction_nontrivial_action_stays_in_inline_arena`:
  a stateful action fits the inline arena and its state is destroyed.
  **obsolete**: no scratch arena and no action state in next.
- `runtime_transaction_test.runtime_transaction_processes_action_installed_by_completion`:
  a commit action can install another commit action, which runs. **obsolete**:
  no commit actions in next.
- `runtime_transaction_test.runtime_transaction_defers_reentrant_action_state_destruction`:
  action states are destroyed only after reentrant actions finish. **obsolete**:
  no action state in next.
- `runtime_transaction_test.runtime_transaction_destroys_action_states_after_rollback_callbacks`:
  rollback actions run in reverse order (then their states are destroyed)
  **replaced**: `test/next/memory/transaction_log.cpp`
  `next_transaction_log.a_failed_request_undoes_its_effects_in_reverse`; tests
  `memory/lifetime_slot.h` `basic_transaction_log`. State destruction is
  obsolete.
- `runtime_transaction_test.runtime_transaction_action_state_finishes_once`: a
  finish action runs once, on commit or on rollback. **obsolete**: no finish
  actions in next. Undone-once is covered for undo entries by
  `next_transaction_log.records_an_effect_once_per_transaction`.
- `runtime_transaction_test.runtime_transaction_commit_preserves_reverse_action_order`:
  39 commit actions run in reverse order. **obsolete**: no commit actions in
  next. The same reverse-order property for undo is in
  `next_transaction_log.undoes_more_effects_than_it_records_inline`.
- `runtime_transaction_test.nested_runtime_transaction_commit_is_provisional_until_root_commit`:
  a nested commit holds only until the root finishes. **replaced**:
  `next_transaction_log.every_request_undoes_its_own_effects` (block 1: a nested
  request returns, the outer one fails, and both are undone inner first); tests
  `memory/lifetime_slot.h` `basic_transaction_log`. Public side:
  `next_nested_requests.a_failure_undoes_the_effects_of_the_requests_that_returned`.
- `runtime_transaction_test.transactions_on_different_runtimes_finish_independently`:
  transactions of two runtimes commit and roll back independently. **replaced**:
  `next_transaction_log.the_transactions_of_two_containers_are_their_own`;
  public side also in
  `next_transactions.a_registration_belongs_to_the_transaction_of_its_container`
  (read: a child registration during a failed parent request is kept)
- `runtime_transaction_test.nested_runtime_transaction_commit_rolls_back_with_root`:
  a root rollback also undoes a committed nested transaction (actions and
  objects) **replaced**:
  `next_transaction_log.every_request_undoes_its_own_effects` (block 1),
  `next_retained.enters_a_transaction_once_with_its_first_mark` (objects kept by
  a nested request that returned are destroyed when the outer one fails)
- `runtime_transaction_test.nested_runtime_transaction_rollback_preserves_root_state`:
  an inner rollback destroys the inner objects at once; the root still commits
  its own. **ported**: `test/next/containers/nested_requests.cpp`
  `next_nested_requests.a_failure_caught_inside_a_request_undoes_its_effects`
  (the failed nested request destroys its object before the outer request
  returns, which keeps its own); log level:
  `next_transaction_log.every_request_undoes_its_own_effects` (block 2)
- `runtime_transaction_test.runtime_transaction_rollback_preserves_reverse_action_order`:
  39 rollback actions run in reverse order. **replaced**:
  `next_transaction_log.undoes_more_effects_than_it_records_inline` (39 effects
  in exact reverse, past the 8 inline)
- `runtime_transaction_test.runtime_transaction_accepts_move_only_capturing_lambda`:
  an action may be a move-only callable. **obsolete**: undo entries in next are
  function pointers with void\* target and context, so there is no callable to
  store.
- `runtime_transaction_test.runtime_transaction_rolls_back_without_runtime_context`:
  rollback destroys the object constructed in it and resets the slot that
  published it. **ported**: `test/next/containers/nested_requests.cpp`
  `next_nested_requests.a_failed_request_forgets_the_objects_it_constructed`
  (destroyed once; the slot constructs again afterwards and keeps the new
  object). Related:
  `next_transactions.a_failed_request_undoes_its_effects_on_the_container`.
- `runtime_transaction_test.runtime_transaction_explicit_reset_action_covers_null_slot_install`:
  rollback resets a slot that was empty before the transaction installed an
  object. **ported**:
  `next_nested_requests.a_failed_request_forgets_the_objects_it_constructed`
  (the slot was empty, the failed request filled it, and it is empty again
  afterwards)
- `runtime_transaction_test.runtime_transaction_commit_keeps_persistent_state`:
  commit keeps the objects until the runtime ends. **replaced**:
  `next_retained.a_transaction_that_returns_keeps_its_objects`.
- `runtime_transaction_test.runtime_transaction_finishes_scratch_before_persistent_rollback`:
  scratch finish actions run before persistent objects are rolled back.
  **obsolete**: no scratch arena or finish actions in next. Everything is one
  log, undone in reverse order of recording.
- `runtime_transaction_test.actions_run_only_for_the_matching_outcome`: commit
  actions run only on commit, rollback actions only on rollback, finish on both.
  **replaced**: `next_transaction_log.a_request_that_returns_keeps_its_effects`
  \+ `next_transaction_log.a_failed_request_undoes_its_effects_in_reverse` (undo
  only on failure). Commit and finish actions are obsolete.
- `runtime_transaction_test.rollback_processes_action_installed_by_rollback_callback`:
  a rollback action may install another rollback action, which also runs.
  **replaced**: `test/next/memory/transaction_log.cpp`
  `next_transaction_log.an_effect_recorded_while_undoing_is_undone` (an undo
  records an effect while the log, grown out of line to its capacity, is undone;
  the recorded effect is undone too); tests `memory/lifetime_slot.h`
  `basic_transaction_log`.
- `runtime_transaction_test.inner_rollback_discards_inner_actions_and_preserves_outer_actions`:
  an inner rollback undoes the inner actions at once and the outer transaction
  still commits. **ported**:
  `next_nested_requests.a_failure_caught_inside_a_request_undoes_its_effects`;
  log level: `next_transaction_log.every_request_undoes_its_own_effects` (block
  2: the inner effect is undone when the inner request fails, the outer one is
  kept)
- `runtime_transaction_test.repeated_finish_calls_are_harmless`: repeated
  commit/rollback calls finish once. **obsolete**: next has no explicit commit
  or rollback: `basic_transaction_log::run` ends the transaction exactly once,
  by scope.
- `runtime_transaction_test.construct_persistent_with_registers_destructor_for_rollback`:
  an object made in place by a constructor function is destroyed on rollback.
  **replaced**:
  `next_retained.a_failed_transaction_destroys_the_objects_kept_during_it`
  (`emplace` takes a maker; objects kept during the transaction are destroyed)

### test/type/type_identity.cpp (+ type_identity_fixture.cpp, type_identity_shared.h)

- `type_identity_test.descriptors_of_one_type_compare_equal_across_units`: the
  identity of `unique_ptr<service>` is equal across TUs that spell it
  differently. **replaced**: `test/next/base/type_identity.cpp` +
  `type_identity_fixture.cpp` + `type_identity_shared.h`
  `next_type_identity.the_identity_of_a_type_is_one_across_units` (also
  static_asserts on equality during constant evaluation); tests
  `type/type_tree.h` `type_id_v`, `base/mechanics.h`
  `type_tag`/`named_type_ids`. Passes with the default variant and with
  `-DDINGO_VARIANT_NAMED_TYPE_ID=1`.
- `type_identity_test.descriptors_of_different_types_compare_unequal`:
  identities of different types differ. **replaced**:
  `next_type_identity.the_identities_of_different_types_differ` (adds `const` as
  a difference)
- `type_identity_test.resolve_across_units_with_either_spelling`: a binding
  registered in one TU resolves in another under the other spelling. **ported**:
  `next_type_identity.resolves_across_units_by_either_spelling` (public
  `dingo/container.h` + `dingo/storage/unique.h`; the fixture also has the core
  type_id helpers)

### test/type/type_list.cpp

- `type_list_test.meta_utilities`: cat, size, head, to_type_list, contains,
  unique, merge. **replaced**: `test/next/base/type_list.cpp` static_asserts and
  `next_type_list.finds_the_first_element_of_each_type`; tests
  `base/mechanics.h` `type_list::size`, `type_list_at_t` (head = index 0),
  `type_list_index_of` (contains = index < size; a nested list is one element).
  cat, unique, merge and to_type_list have no counterpart in next and are
  obsolete.
- `type_list_test.for_each_visits_types_in_order`: for_each visits elements in
  order and never visits for an empty list. **obsolete**: core type_list has no
  for_each: next expands packs directly (fold expressions). Ordered expansion is
  used in `next_type_list.finds_the_first_element_of_each_type`.
- `type_list_test.for_type_matches_first_type_only`: runtime type-index dispatch
  hits only the first matching element; a missing type or an empty list hits
  nothing. **replaced**: `next_type_list.finds_the_first_element_of_each_type` +
  static_asserts on `type_list_index_of` (first match of a duplicate, size when
  missing, 0 for an empty list); tests `base/mechanics.h`. Next has no runtime
  type-index dispatch.

### test/type/type_name.cpp

- `type_name_test.value_type_name`: the raw name of a class is the compiler
  spelling. **replaced**: `test/next/diagnostics/type_name.cpp`
  `next_type_name.spells_a_class_as_the_compiler_does` (+ static_assert on
  `spelled_type_name`); tests `diagnostics/type_name.h`.
- `type_name_test.array_type_name_keeps_closing_bracket`: an array name keeps
  `[3]`. **replaced**: `next_type_name.keeps_the_extent_of_an_array`.
- `type_name_test.const_reference_type_name`: `const X&` spelling. **replaced**:
  `next_type_name.puts_the_qualifiers_of_a_class_in_front`.
- `type_name_test.pointer_type_name`: `X*` spelling. **replaced**:
  `next_type_name.composes_pointers_around_their_pointee`.
- `type_name_test.double_pointer_type_name`: `X**` spelling. **replaced**:
  `next_type_name.composes_pointers_around_their_pointee`.
- `type_name_test.pointer_reference_descriptor`: the descriptor of
  `const X* const&` breaks into reference, cv and pointee. **replaced**:
  `next_type_name.composes_pointers_around_their_pointee` (`const X* const&`).
  Next has no descriptor struct; the decomposition shows up as the composed
  name.
- `type_name_test.nested_pointer_descriptor`: the descriptor of
  `const X* const* volatile` breaks into two pointee levels. **replaced**:
  `next_type_name.composes_pointers_around_their_pointee`
  (`const X* const* volatile`)
- `type_name_test.shared_ptr_type_name_contains_wrapper_and_pointee`: the name
  of a shared_ptr contains the wrapper and its element. **replaced**:
  `next_type_name.names_a_wrapper_with_its_element`.
- `type_name_test.optional_type_name_contains_wrapper_and_pointee`: the name of
  an optional contains the wrapper and its element. **replaced**:
  `next_type_name.names_a_wrapper_with_its_element`.
- `type_name_test.function_signature_type_name`: parts of `void(int, const X&)`
  appear in order. **replaced**:
  `next_type_name.names_the_parts_of_a_function_type`.
- `type_name_test.invoke_style_function_signature_type_name`: parts of
  `void(X&)` appear in order. **replaced**:
  `next_type_name.names_the_parts_of_a_function_type`.
- `type_name_test.function_pointer_type_name`: parts of `int(*)(double, X*)`
  appear in order. **replaced**:
  `next_type_name.names_the_parts_of_a_function_type`.
- `type_name_test.static_function_pointer_type_name`: parts of a static member
  function pointer appear in order. **replaced**:
  `next_type_name.names_the_parts_of_a_function_type`.
- `type_name_test.functor_operator_type_name`: parts of a const call operator
  member pointer appear in order. **replaced**:
  `next_type_name.names_the_parts_of_a_function_type`.
- (file-scope static_asserts) `detail::type_name_find`, `raw_type_name`
  non-empty: substring search, and a non-empty raw name. **replaced**:
  static_asserts in `test/next/diagnostics/type_name.cpp` on `detail::find_text`
  and `spelled_type_name`.

### test/type/type_traits.cpp

- `type_traits_test.annotated_copies_non_default_constructible_copy_only_values`:
  `annotated<T, Tag>` copies a T that is copyable but neither movable nor
  default-constructible. **replaced**: `test/next/conversion/request_types.cpp`
  `next_request_types.an_annotated_value_copies_a_copy_only_value` (annotated
  and keyed, from an lvalue and an rvalue); tests `type/annotated.h` and
  `lookup/key.h` `keyed`.
- (file-scope static_asserts) `detail::recursion_guard` not copyable or movable:
  the old RAII recursion guard cannot be copied. **obsolete**: next guards
  recursion with a local guard struct in `backend/slot.h`, not a type. Recursion
  behavior is covered by `next_construction.reports_a_recursion` /
  `next_unique.reports_a_recursion`.
- (file-scope static_asserts)
  `is_interface_storage_rebindable_v<test_shared/test_unique<Class>, IClass>`:
  user wrappers rebind to an interface of their element. **replaced**:
  static_asserts in `test/next/conversion/request_types.cpp` on
  `core::replace_leaf_t<user_shared<impl>, iface>`,
  `dingo::rebind_type_t<user_unique<impl>, iface>`, `leaf_type_t`; tests
  `type/wrapper_traits.h`, `api/type/rebind_type.h`.
- (file-scope static_asserts) `dingo::is_copy_constructible_v` of wrappers and
  std containers: shared and optional copy; unique, vector<unique>, map\<int,
  unique_ptr> do not. **replaced**: static_asserts on
  `type_tree_v<...>.nodes[0].copyable` for shared_ptr, unique_ptr, optional,
  optional\<unique_ptr>, user shared/unique wrappers, and optional of each. The
  vector and map cases are not carried over: next treats a std container as a
  leaf and asks `std::is_copy_constructible_v`, which is true for them, while a
  value request of such a vector fails to compile in both implementations, so no
  public behavior differs.
- (file-scope static_asserts) `request_type<...>` lookup/interface/value/exact
  types, rvalue flags: how a request type breaks down. **replaced**:
  static_asserts on `core::request_traits` (kind, value_type, made_type),
  `core::leaf_of_t`, `core::resolved_t`, `core::annotated_request`,
  `annotated_traits`, and the annotated request descriptor leaf
  (`type_id_v<annotated<service, tag>>`); tests `conversion/request.h`,
  `backend/selection.h`, `type/annotated.h`. The `removes_rvalue_references`
  mode has no counterpart (obsolete)
- (file-scope static_asserts) annotated constructibility (shared_ptr yes,
  unique_ptr from const& no): annotated values copy copyable values and refuse
  to copy move-only ones. **replaced**: static_asserts in `request_types.cpp`
  (also `keyed<unique_ptr>`), plus
  `next_request_types.an_annotated_value_converts_to_the_value_it_holds`.
- (file-scope static_asserts) `dependency<S&, key_type<tag>>` request_type: a
  keyed parameter breaks down into its interface and key. **replaced**:
  static_asserts: `dingo::dependency<service &, key_type<tag>>` is
  `core::keyed<service &, tag_key<tag>>`, `resolved_t` is `service &`, and
  `dependency<service &>` is `service &`.

## Group D: registration traits

No case is kept: every one includes
`test/registration/type_registration_common.h`, which includes old internal
headers. The engine cases are `static_assert`s, so breaking a property breaks
the build; they are not mutation-checked otherwise. Two differences found here
are not observed through the public API of the baseline and are recorded only:

- The unique scope of the next implementation serves an immovable leaf by value,
  as a prvalue, where the test-only `fresh_value_storage<immovable>` of the old
  binding model published none.
- The old code normalized `volatile A(*)[3]` to `A[3]`; the next implementation
  names the leaf `A`. Both give the identity a lookup uses.

### test/registration/registration_traits.cpp

- `type_registration_test.runtime_local_bindings_declare_the_registration_container`:
  a registration with `bindings<...>` resolves its dependencies from its local
  static bindings (and the proxy and container types it gets) **ported**:
  `test/next/containers/stored_type_trees.cpp`
  `next_stored_type_trees.local_static_bindings_resolve_the_dependencies_of_their_registration`.
  The type-level part is replaced by `test/next/conversion/registration.cpp`
  `next_engine.local_bindings_are_binding_types_of_the_registration`, which
  tests `dingo/registration.h` (`binding::locals`)
- `type_registration_test.conversion_availability_uses_source_access`: a
  conversion is available only when the source access (borrow/consume) allows
  it, for declared conversions, converting constructors, handles, optionals,
  variants, addresses, arrays, and volatile bindings. **replaced**:
  `test/next/conversion/routes.cpp`:
  `next_engine.a_conversion_needs_the_access_it_declares` (tests
  `type/type_conversion_traits.h` `converts`),
  `next_engine.a_consumed_object_is_moved_wrapped_or_adopted` (`int*` makes a
  `shared_ptr` only when consumed),
  `next_engine.a_qualified_object_is_served_with_its_qualifiers` (volatile
  binding and volatile variant),
  `next_engine.an_array_of_arrays_serves_its_rows_and_itself`; these test
  `conversion/route.h` and `request.h`. Also
  `test/next/containers/stored_type_trees.cpp`
  `next_stored_type_trees.volatile_storage_keeps_the_volatile_qualifier`. Not
  carried over: the user types described through the old `dingo::type_traits`
  (replaced by `wrapper_extension_point`), `void*` from a variant lvalue and the
  address conversion (next has no `void*` request type tree), and
  `array_base(*)[2]` from `array_derived*` (an array is never upcast; the
  nearest refusal is the `interface_in_array` status)
- `type_registration_test.inferred_borrowed_conversion_uses_const_source`: a
  borrowed conversion by converting constructor uses the const source and leaves
  it intact. **replaced**: `test/next/conversion/routes.cpp`
  `next_engine.a_borrowed_conversion_leaves_its_source`
  (`type/type_conversion_traits.h` `convert_to`);
  `test/next/containers/stored_type_trees.cpp`
  `next_stored_type_trees.a_borrowed_user_handle_converts_from_a_const_source`
  (the `keep_step` of `conversion/convert.h` converts `std::as_const`)
- `type_registration_test.same_type_custom_conversion_is_preserved`: a declared
  conversion whose target is its source type is used, not the identity.
  **replaced**: `test/next/conversion/routes.cpp`
  `next_engine.a_declared_conversion_is_used_for_its_target`; tests
  `type/type_conversion_traits.h`.
- `type_registration_test.borrowed_optional_conversion_uses_const_contained_value`:
  converting an optional of an owner to an optional of a copy borrows the
  contained value as const. **replaced**: the const-source property:
  `next_engine.a_borrowed_conversion_leaves_its_source` and
  `next_stored_type_trees.a_borrowed_user_handle_converts_from_a_const_source`.
  The conversion of the element of an optional is changed:
  `optional_elements_are_not_converted`, tested by
  `next_stored_type_trees.a_pointer_to_an_optional_serves_no_optional_of_its_interface`.
- `type_registration_test.inferred_mutable_lvalue_conversion_consumes_source`: a
  consuming conversion takes the source's resource. **replaced**:
  `test/next/conversion/routes.cpp`
  `next_engine.a_borrowed_conversion_leaves_its_source` (consumed half:
  `convert_to` of an rvalue moves the source out);
  `test/next/containers/stored_type_trees.cpp`
  `next_stored_type_trees.a_consumed_user_handle_converts_by_its_traits`. Next
  passes a consumed source as an rvalue, not as a mutable lvalue.
- `type_registration_test.borrowed_custom_conversion_executes_non_owning_result`:
  a declared borrowing conversion that returns an alternative type builds the
  variant target. **replaced**: `test/next/conversion/routes.cpp`
  `next_engine.a_declared_conversion_is_used_for_its_target` (a conversion that
  returns an object the target is constructed from); tests
  `type/type_conversion_traits.h`.
- `type_registration_test.borrowed_storage_publishes_fresh_move_only_conversion_results`:
  external `move_source*` with interfaces serves a move-only interface by value
  through the declared conversion. **ported**:
  `test/next/containers/stored_type_trees.cpp`
  `next_stored_type_trees.a_borrowed_leaf_converts_to_a_new_interface_value`.
- `type_registration_test.borrowed_storage_publishes_copyable_custom_value_conversion`:
  a declared conversion to a copyable interface is selected over a copy.
  **ported**: `test/next/containers/stored_type_trees.cpp`
  `next_stored_type_trees.a_borrowed_leaf_converts_to_a_new_interface_value`
  (`copy_target` gets 47 from the conversion, not 23 from a copy)
- `type_registration_test.consumed_binding_executes_custom_type_conversion`: a
  unique user handle converts to the handle of its interface by its conversion
  traits. **ported**: `test/next/containers/stored_type_trees.cpp`
  `next_stored_type_trees.a_consumed_user_handle_converts_by_its_traits`.
- `type_registration_test.binding_model_rewrites_single_interface_storage_leaf`:
  with one interface the stored `shared_ptr<A>` is rebound to `shared_ptr<I>`.
  **changed**: `interfaces_do_not_rebind_storage`;
  `test/next/conversion/routes.cpp`
  `next_engine.interfaces_do_not_rebind_the_stored_type_tree`.
- `type_registration_test.binding_model_preserves_storage_for_multi_interface_registration`:
  with several interfaces the storage is kept and the binding serves each
  interface. **replaced**: `test/next/conversion/routes.cpp`
  `next_engine.interfaces_do_not_rebind_the_stored_type_tree` (part `two`: one
  descriptor serves `I` and `J`, not `A`); tests `conversion/route.h` and
  `dingo/registration.h`.
- `type_registration_test.recursive_leaf_and_rebind_traits`: leaf and rebind
  traits, request lookup types, qualification matching of requests, and the
  values, references, and pointers each stored type tree publishes.
  **replaced**: `test/next/conversion/routes.cpp`:
  `next_engine.a_pointer_serves_the_object_it_points_to_and_its_copy`,
  `next_engine.a_move_only_object_is_never_copied`,
  `next_engine.a_consumed_object_is_moved_wrapped_or_adopted`,
  `next_engine.a_borrowing_scope_does_not_move_its_object`,
  `next_engine.a_wrapper_of_the_requested_type_is_not_wrapped_again`,
  `next_engine.a_request_adds_qualifiers_only_below_const_levels`
  (`core::qualifies`, the replacement for `is_resolution_request_v` and
  `matches_qualification_conversion`),
  `next_engine.a_request_names_the_leaf_through_its_wrappers` (`request_traits`,
  `leaf_of_t`), and `next_engine.a_conversion_needs_the_access_it_declares`
  (unique_ptr deleter). The incompatible-deleter interface binding is changed
  under `interface_bindings_serve_an_interface`, tested by
  `next_engine.a_handle_that_converts_to_no_interface_is_rejected`. The
  `rebind_leaf_t`/`rebind_type_t` asserts are changed under
  `interfaces_do_not_rebind_storage`. The `const A*` interface part is
  `next_stored_type_trees.a_handle_of_a_const_object_serves_only_const_interfaces`;
  the `optional<I>` part is changed under `optional_elements_are_not_converted`
  (`next_stored_type_trees.a_pointer_to_an_optional_serves_no_optional_of_its_interface`).
  The parts using `fresh_value_storage`, a test-only storage of the old binding
  model, are obsolete.
- `type_registration_test.nested_raw_pointer_storage_resolves_exact_request_shapes`:
  pointer storage (`optional<T>*`, `const array*`, `shared_ptr*`, move-only,
  null, shared, unique) serves the exact request type trees, and an
  `optional<impl>*` serves `optional<interface>`. **ported**:
  `test/next/containers/stored_type_trees.cpp`:
  `next_stored_type_trees.a_pointer_to_a_wrapper_serves_the_wrapper_and_a_copy`,
  `.a_pointer_to_a_move_only_object_serves_no_copy`,
  `.a_null_external_pointer_is_served_as_null`,
  `.a_shared_pointer_to_a_const_wrapper_is_one_object`,
  `.a_unique_pointer_to_a_wrapper_is_adopted_by_handles`,
  `.a_pointer_to_an_optional_serves_its_interfaces`. The `optional<interface>`
  part is changed: `optional_elements_are_not_converted`, tested by
  `next_stored_type_trees.a_pointer_to_an_optional_serves_no_optional_of_its_interface`.
- `type_registration_test.const_reference_storage_preserves_const_access`:
  `const T&`, `const optional&`, a const user optional, and nested
  optional/variant storage serve only const access, and an empty optional serves
  a null pointer. **ported**: `test/next/containers/stored_type_trees.cpp`
  `next_stored_type_trees.const_storage_serves_only_const_access` and
  `.const_nested_wrappers_serve_only_const_access`; with `interfaces<int>`,
  `next_stored_type_trees.a_handle_of_a_const_object_serves_only_const_interfaces`;
  `test/next/conversion/routes.cpp`
  `next_engine.a_qualified_object_is_served_with_its_qualifiers`. The empty
  optional: `next_stored_type_trees.the_element_of_an_empty_optional_is_null`
  (its element is a null address; a reference or a value of it is refused with
  element_not_held, behavior change `empty_elements_are_refused`)
- `type_registration_test.const_external_smart_pointer_storage_preserves_const_access`:
  a const `shared_ptr`/`unique_ptr` to a const object serves only const borrows,
  also through an interface. **ported**:
  `test/next/containers/stored_type_trees.cpp`
  `next_stored_type_trees.a_handle_of_a_const_object_serves_only_const_access`;
  through an interface,
  `next_stored_type_trees.a_handle_of_a_const_object_serves_only_const_interfaces`.
- `type_registration_test.const_alternative_storage_only_publishes_const_borrows`:
  a const variant serves only const borrows of its alternative. **ported**:
  `test/next/containers/stored_type_trees.cpp`
  `next_stored_type_trees.a_const_alternative_serves_only_const_access`; with
  `interfaces<int>`,
  `next_stored_type_trees.a_handle_of_a_const_object_serves_only_const_interfaces`;
  `test/next/conversion/routes.cpp`
  `next_engine.a_qualified_object_is_served_with_its_qualifiers`.
- `type_registration_test.multidimensional_array_storage_preserves_exact_and_row_shapes`:
  `int[2][3]` serves `int(&)[2][3]`, row and whole pointers with added cv
  qualifiers, and no value. **ported**:
  `test/next/containers/stored_type_trees.cpp`
  `next_stored_type_trees.an_array_of_arrays_serves_its_rows_and_itself`;
  `test/next/conversion/routes.cpp`
  `next_engine.an_array_of_arrays_serves_its_rows_and_itself`.
- `type_registration_test.normalized_type_trait`: the normalized (lookup) type
  of a request through cv, references, pointers, handles, arrays, and
  annotations, and which requests may escape. **replaced**:
  `test/next/conversion/routes.cpp`
  `next_engine.a_request_names_the_leaf_through_its_wrappers`; tests
  `conversion/request.h` (`request_traits::descriptor`, `kind`, `made_type`) and
  `type/wrapper_traits.h` (`leaf_of_t`). `request_may_escape_v` is replaced by
  `request_kind`: an lvalue reference refers to a kept object. The retention of
  rvalue requests is in `next_unique.retains_the_object_of_an_rvalue_reference`.

### test/registration/type_registration.cpp

- `type_registration_test.get_type`: finds the part of a kind in a list of
  registration parts. **replaced**: `test/next/conversion/registration.cpp`
  `next_engine.a_registration_takes_the_first_part_of_each_kind`; tests
  `dingo/registration.h` (`find_part`, via `registration::*_part`)
- `type_registration_test.registration_basic`: a registration exposes its scope,
  storage, interfaces, and factory parts. **replaced**:
  `test/next/conversion/registration.cpp`
  `next_engine.a_registration_takes_the_first_part_of_each_kind` (`reordered`);
  tests `dingo/registration.h`.
- `type_registration_test.registration_prefers_first_explicit_match`: the first
  part of each kind wins. **replaced**: `test/next/conversion/registration.cpp`
  `next_engine.a_registration_takes_the_first_part_of_each_kind`. The
  `conversions<>` part is obsolete: next has no conversions policy.
- `type_registration_test.registration_deduction`: storage is deduced from the
  factory, interfaces from storage or factory, and the factory from storage.
  **replaced**: `test/next/conversion/registration.cpp`
  `next_engine.a_registration_without_interfaces_is_named_by_its_stored_leaf`
  (an empty interface list names the stored leaf, `detected_factory` is the
  default); tests `dingo/registration.h`. Deducing storage from `factory<T>` is
  obsolete: it gave `storage<factory type>`, which names no object for a real
  factory, and next requires a storage part
  (`test/lit/generated/diagnostics/registration_without_storage.cpp`)
- `type_registration_test.registration_default_interface_and_conversions`: the
  default interface is the stored type, and the default conversions are
  `conversions<shared, int, runtime_type>`. **replaced**:
  `test/next/conversion/registration.cpp`
  `next_engine.a_registration_without_interfaces_is_named_by_its_stored_leaf`.
  The conversions part is obsolete: next derives the served requests from the
  stored type tree and the scope descriptor (`conversion/route.h`), as the
  `test/next/conversion/routes.cpp` cases check.
- `type_registration_test.registration_dependencies_default_from_factory_traits`:
  a registration's dependencies default to the declared constructor, the
  annotated constructor, or the function arguments. **replaced**:
  `test/next/conversion/registration.cpp`
  `next_engine.a_factory_declares_the_arguments_of_its_leaf`; tests
  `conversion/construct.h` `declared_arguments` (what the static graph of
  `backend/static_backend.h` reads)
- `type_registration_test.registration_dependencies_prefer_explicit_metadata`:
  `dependencies<>` overrides the dependencies of a declared factory.
  **changed**: `dependencies_are_declared_once`: a registration declares its
  dependencies by a `dependencies<>` or by its factory, not both, so the old
  registration does not compile. New behavior:
  `test/lit/generated/diagnostics/registration_dependencies_twice.cpp`
  (diagnostic registration_dependencies_twice)
- `type_registration_test.registration_explicit_dependencies_shape_default_constructor_factory`:
  `dependencies<double, bool>` without a factory selects
  `constructor<T(double, bool)>`. **replaced**:
  `test/next/conversion/registration.cpp`
  `next_engine.dependencies_declare_the_constructor_of_a_registration`; tests
  `dingo/registration.h` `declared_factory` and `conversion/construct.h`.
- `type_registration_test.factory_traits_report_dependencies`: factory_traits
  report the dependencies, explicitness, and compile-time bindability of
  constructor, detected, function, and callable factories. **replaced**:
  `test/next/conversion/registration.cpp`
  `next_engine.a_factory_declares_the_arguments_of_its_leaf`; tests
  `conversion/construct.h` `declared_arguments`. In next a detected, unannotated
  constructor declares `void`, because its arguments are known only at
  construction, where the old code reported `type_list<>`.
  `has_explicit_dependencies` has no next equivalent. Compile-time bindability
  is the rejection of a callable factory in static bindings:
  `test/lit/static-bindings-callable-factory-is-rejected.cpp`
  (factory_not_declared)
- `type_registration_test.registration_specialization`: an interface rebinds
  smart-handle storage only when C++ converts the handle directly; an empty
  conversion cache is empty; plain bindings are no larger than handle bindings.
  **changed**: `interfaces_do_not_rebind_storage` (and
  `wrapper_extension_point`, which removes `is_interface_storage_rebindable_v`
  from the public API); `test/next/conversion/routes.cpp`
  `next_engine.interfaces_do_not_rebind_the_stored_type_tree` (the nested
  `shared_ptr<unique_ptr<A>>` keeps its type) and
  `next_engine.a_binding_keeps_objects_only_for_its_cached_routes`
  (`no_kept_objects` is empty and trivially destructible, which replaces the
  conversion cache). The binding-size comparison is not reproduced;
  `next_object_sizes.a_slot_keeps_its_object_and_what_its_scope_needs` bounds
  the slot of each scope instead, which is a related property, not the same.

## Group E: routes, resolution, and runtime registration

The cases of `test/registration/object_routes.cpp`, `object_sizes.cpp`,
`runtime_registration.cpp`, and `test/resolution/`. A detected constructor
parameter `T&&` binds a temporary, destroyed after the constructor returns; only
a declared `dependencies<T&&>` refers to an object the container keeps, so the
ported retention tests declare their dependencies, as the old ones did.

### test/registration/object_routes.cpp

- (file-scope `static_assert(object_routes_match_v<...>)`, 50 bindings): the
  merged object routes of a binding list the same reference, pointer and value
  targets as the separate storage-list routes, once each, and every shared route
  replaces a default borrow or identity copy of the same source. **obsolete**:
  Tested an internal optimization of the old planner
  (`detail::binding_object_resolutions` vs `storage_resolutions_t`). Next has
  one derivation (`conversion/route.h` `derive`/`select_route`) and nothing to
  merge. Which requests each binding serves is checked against the reference by
  `engine.computes_what_the_reference_computes` (every scope x leaf/interfaces x
  stored type tree of up to two wrappers x request: found, served, rules) and by
  the route table (`conform.py --exact` over `routes:*/...`)
- `object_routes_test.bindings_accept_the_same_erased_requests`: type-erased
  request matching gives the same answer for merged and separate routes, over
  220 request variants per binding. **obsolete**: Same reason as above: next has
  no separate route lists to compare. The type-erased side is the runtime
  backend's memo of the derived reachable types (`backend/memo.h`). Its
  found/served decisions equal the reference's in
  `engine.computes_what_the_reference_computes`.
- `object_routes_test.a_pointer_route_serves_the_reference_of_its_object`: a
  shared leaf serves `&`, `const &`, `*`, `const *`, value and const value, but
  not `&&` or another leaf. **ported**: `test/next/containers/object_routes.cpp`
  `next_object_routes.a_shared_leaf_serves_its_references_and_copies`,
  `next_object_routes.a_shared_leaf_serves_no_move_and_no_other_leaf`.
- `object_routes_test.a_const_object_does_not_serve_a_mutable_request`: an
  external `const leaf &` serves const references and pointers only. **ported**:
  `test/next/containers/object_routes.cpp`
  `next_object_routes.a_const_object_serves_no_mutable_reference`,
  `next_object_routes.a_const_object_serves_const_references_and_copies`.
- `object_routes_test.wrapper_leaf_routes_keep_a_route_per_category`: a shared
  `shared_ptr<leaf>` serves the handle by its address and the leaf by reference
  and pointer (3 internal routes) **ported**:
  `test/next/containers/object_routes.cpp`
  `next_object_routes.a_shared_handle_serves_itself_and_its_leaf`. The served
  requests are ported. The internal route count has no counterpart.
- `object_routes_test.one_route_serves_reference_pointer_and_copy`: `&`, `*`,
  `const &`, `const *` reach one stored object, a value copies it once, `&&` is
  not convertible. **ported**:
  `next_object_routes.a_shared_leaf_serves_its_references_and_copies`,
  `next_object_routes.a_shared_leaf_serves_no_move_and_no_other_leaf`.
- `object_routes_test.a_move_only_object_serves_references_and_no_copy`: a
  shared move-only object serves `&`/`*`, and a value request is not
  convertible. **ported**:
  `next_object_routes.a_move_only_object_serves_references_and_no_copy`.
- `object_routes_test.a_const_object_serves_const_references_and_copies`: an
  external const object serves const `&`/`*` and copies, and refuses mutable
  `&`/`*`. **ported**:
  `next_object_routes.a_const_object_serves_const_references_and_copies`,
  `next_object_routes.a_const_object_serves_no_mutable_reference`.
- `object_routes_test.external_unique_handle_serves_a_copy_of_its_object`: an
  external `unique_ptr` serves its pointee by `&`/`*` and as one copy.
  **ported**:
  `next_object_routes.an_external_unique_handle_serves_a_copy_of_its_object`.
- `object_routes_test.external_unique_handle_serves_no_copy_of_move_only`: an
  external `unique_ptr` of a move-only leaf serves `&`, and a value request is
  not convertible. **ported**:
  `next_object_routes.an_external_unique_handle_serves_no_copy_of_a_move_only_object`.
- `object_routes_test.shared_pointer_serves_a_copy_of_its_pointee`: a shared
  `T*` made by a callable serves its pointee by `&`/`*` and as one copy.
  **ported**:
  `next_object_routes.a_shared_pointer_serves_a_copy_of_its_pointee`. The
  binding owns its pointee and deletes it with the container, as the baseline
  did: `next_object_routes.a_shared_pointer_owns_the_object_its_factory_returns`
  and `next_object_routes.a_shared_pointer_owns_the_object_it_constructs`.
- `object_routes_test.shared_pointer_serves_no_copy_of_move_only_pointee`: a
  shared `T*` of a move-only pointee serves `&`/`*`, and a value request is not
  convertible. **ported**:
  `next_object_routes.a_shared_pointer_serves_no_copy_of_a_move_only_pointee`.
- `object_routes_test.a_custom_copy_conversion_keeps_its_value_route`: a value
  request copies a leaf through its `type_conversion_traits<T, T>`. **changed**:
  `leaves_are_copied_by_their_copy_constructor`: a conversion a leaf declares to
  its own type is not a route. New behavior:
  `next_object_routes.a_leaf_is_copied_by_its_copy_constructor`.
- `object_routes_test.interface_objects_serve_their_references`: interfaces of a
  shared leaf are served by `&`/`*`, and the undeclared leaf is not found.
  **ported**: `next_object_routes.interfaces_are_served_in_place_of_the_leaf`.
- `object_routes_test.wrapper_objects_serve_their_references_and_copies`: a
  shared `shared_ptr<leaf>` serves leaf `&`/`*`, handle `&`/`*`/copy of the same
  handle, and one copy of the leaf. **ported**:
  `next_object_routes.a_shared_handle_serves_itself_and_its_leaf`.
- `object_routes_test.a_pointer_storage_serves_its_pointee`: an external `T*`
  serves the pointee by `&`/`*` and as one copy. **ported**:
  `next_object_routes.a_pointer_storage_serves_its_pointee`.
- `object_routes_test.external_shared_handle_serves_a_copy_of_its_leaf`: an
  external `shared_ptr` serves its leaf by `&`/`*`, itself by copy, and one copy
  of the leaf. **ported**:
  `next_object_routes.an_external_shared_handle_serves_a_copy_of_its_leaf`.
- `object_routes_test.external_nested_shared_handle_serves_a_copy_of_leaf`: an
  external `shared_ptr<optional<T>>` serves one copy of its leaf. **ported**:
  `next_object_routes.an_external_nested_handle_serves_a_copy_of_its_leaf` (also
  asserts the `&` reaches the held leaf)
- `object_routes_test.external_nested_shared_handle_serves_no_copy_of_move_only_leaf`:
  the same handle of a move-only leaf refuses a value request. **ported**:
  `next_object_routes.an_external_nested_handle_serves_no_copy_of_a_move_only_leaf`.

### test/registration/object_sizes.cpp

- `object_sizes_test.runtime_container_and_lookup_sizes`: upper bounds on
  `sizeof` of `container<>` (\<=120) and ~35 old internals (registry, binding
  states, binding owners, lookup backends/index, conversion cache, type index);
  emptiness and non-polymorphism of storage. **replaced**:
  `test/next/backend/object_sizes.cpp`
  `next_object_sizes.a_runtime_container_is_a_few_words` (`container<>` \<= 120
  on 64-bit, `transaction_log` is one pointer),
  `next_object_sizes.a_slot_keeps_its_object_and_what_its_scope_needs` (slot of
  each scope: lazy object+state, external one pointer, unique one bool,
  two-phase bound, +1 pointer for kept handles, not polymorphic),
  `next_object_sizes.kept_objects_are_a_list_of_one_pointer`. Tests
  `backend/slot.h`, `memory/lifetime_slot.h`, `memory/retained.h`. Growth of
  container size is contract `object_size_runtime_container` (constant) and
  `object_size_static_*`/`object_size_mixed_container` in
  `spec/content/contracts.py`, measured by `test/contracts/probe.py`
  (`object_size/runtime` = 96 in `baseline-next.json` for gcc13/clang18. I
  measured 96 here with both). The old lookup-backend, registry and
  binding-owner types have no next counterpart.

### test/resolution/resolution_operation.cpp

- `resolution_operation_test.alternative_projectors_reject_unrelated_targets_with_runtime_exception`:
  projecting a variant to a type that is not one of its alternatives throws at
  runtime instead of failing to compile. **ported**:
  `test/next/conversion/execute.cpp`
  `next_execute.an_alternative_refuses_unrelated_targets_when_resolved`
  (`type_not_found_exception` for `&` and `*`, since next's `finds` rejects the
  request before any projection). The case of an alternative that is not held is
  `next_variants.refuse_an_alternative_not_held`.
- `resolution_operation_test.resolved_immovable_value_is_rejected_without_copy_instantiation`:
  a value request of an immovable object compiles and throws
  `type_not_convertible_exception`. **ported**:
  `next_execute.an_immovable_object_is_referred_to_and_never_copied`.
- `resolution_operation_test.rvalue_source_destroys_materialized_value`: a value
  materialized for an rvalue source is destroyed once. **covered**:
  `next_unique.retains_the_object_of_an_rvalue_reference` (read: two `&&`
  requests keep two live objects, `live == 0` after the container is gone).
  Mechanism `memory/retained.h`. Partly confirmed, see Unresolved.
- `resolution_operation_test.nested_conversions_publish_retained_cache_types`: a
  retaining conversion nested in borrow, alternative, wrapper,
  target-alternative and construction conversions still publishes its cache
  type. **replaced**:
  `next_execute.a_binding_keeps_objects_only_for_a_retaining_route`. Tests
  `conversion/convert.h` `keeps_objects`/`binding_type::keeps`/`context_type`: a
  retained upcast handle at depth 1 and inside `optional`/`variant` keeps
  objects, while plain leaves, leaf handles and consumed handles keep none.
- `resolution_operation_test.traits_conversion_explicitly_constructs_the_target_from_its_result`:
  a `type_conversion_traits` whose `convert` returns another type constructs the
  target explicitly from it. **replaced**:
  `next_execute.a_declared_conversion_constructs_its_target_explicitly`. Tests
  `type/type_conversion_traits.h` (`declares_conversion`, `converts`,
  `convert_to`)
- `resolution_operation_test.shared_fixed_array_storage_reset_destroys_elements_in_reverse_order`:
  a shared `T[3]` destroys its elements in reverse order. **ported**:
  `test/next/backend/slot.cpp`
  `next_slot.a_shared_array_destroys_its_elements_in_reverse_order`.

### test/resolution/cache.cpp

- `cache_test.request_traits_and_keys_distinguish_cached_types`: the request
  cache covers reference/pointer requests and keys `T*`, `const T*`, `T&`,
  `const T&` apart. **replaced**: `test/next/backend/slot.cpp`
  `next_slot.a_settled_stable_object_is_borrowed_once_per_request_type`. A
  counting user slot shows that a repeated `T&` does not reach the slot, while
  `const T&` and `T*` each reach it once more. Tests `backend/runtime_backend.h`
  (`candidate::resolved`) and `backend/memo.h` (`request_id` per request type).
  In next every borrowing completion keeps its address, not only `&`/`*`.
- `cache_test.state_stores_entries_only_when_enabled`: an enabled cache state
  stores an entry, and a disabled one is empty. **replaced**:
  `next_slot.a_binding_without_cached_rules_keeps_nothing_beside_its_object`
  (`no_kept_objects` empty, `binding_context` empty, slot size = object size,
  and a binding with a cached rule adds `kept_objects`),
  `next_slot.keeps_a_derived_object_until_its_object_is_reset`. Tests
  `backend/slot.h`, `memory/retained.h`.
- `cache_test.stable_storage_trait_requires_explicit_stability`: only storage
  that declares itself stable is cached. **replaced**:
  `next_slot.an_unstable_scope_is_reached_by_every_request` (scope descriptors:
  `shared`/`external` stable, `unique`/`shared_cyclical` not. A user scope whose
  descriptor sets `stable = false` is reached by every request). Tests
  `backend/runtime_backend.h` `settled` and the `scope_descriptor`.
- `cache_test.conversion_cache_reuses_value_until_reset`: the conversion cache
  returns its first object until reset. **replaced**:
  `next_slot.keeps_a_derived_object_until_its_object_is_reset`
  (`binding_slot<lazy>` context: `get_or_emplace` makes the handle once, and it
  is forgotten when a failed transaction resets the slot). Tests
  `backend/slot.h` + `memory/retained.h` `kept_objects`. Not mutation-checked.
- `cache_test.tracked_conversion_clears_slot_on_rollback`: a tracked cache entry
  is cleared by the context's rollback. **replaced**:
  `next_slot.keeps_a_derived_object_until_its_object_is_reset` (a failed
  transaction runs `reset_slot` -> `clear_context`, then a committed one keeps
  it and a later failure keeps it)
- `cache_test.empty_conversion_cache_has_no_state`: a cache of no types is empty
  and trivially destructible. **replaced**:
  `next_slot.a_binding_without_cached_rules_keeps_nothing_beside_its_object`
  (`no_kept_objects` empty and trivially destructible, `binding_context` of a
  binding without cached rules empty)
- `cache_test.runtime_conversion_cache_entry_rolls_back_after_failed_resolution`:
  a converted `shared_ptr<iface>&` made during a failed resolution is undone
  (`use_count` 1), remade on retry (`use_count` 2), and stable afterwards.
  **ported**: `next_slot.a_failed_request_forgets_the_converted_handle`
  (`dependencies<>` dropped: next detects the constructor)
- `cache_test.committed_runtime_conversion_cache_survives_later_failed_resolution`:
  a converted handle committed before a failed resolution keeps its address.
  **ported**: `next_slot.a_committed_converted_handle_survives_a_later_failure`.
- `cache_test.rollback_clears_cache_replaced_by_another_request_shape`: a failed
  `int*` request leaves the committed `int&` address intact. **ported**:
  `next_slot.a_failed_request_keeps_the_address_of_another_request_type`.

### test/registration/runtime_registration.cpp

The file and its helper `type_registration_common.h` include old internal
headers (`dingo/runtime/*`, `dingo/registration/*`, `dingo/binding/*`, ...), so
no case is kept as is. `RR` below stands for
`test/next/containers/runtime_registration.cpp`.

- `resolution_access_test.borrowed_raw_pointers_respect_wrapper_ownership`
  (TYPED, 6 old container types): an external `int*` is never adopted by
  `shared_ptr` (alone or in an optional), but converts to a non-owning
  `type_traits` handle (`observer_pointer`) **changed
  (`wrapper_extension_point`)**: refusal half ported: `RR`
  `next_runtime_registration_borrowing.an_external_pointer_is_never_adopted_by_an_owning_handle`
  (TYPED on `container<>`, `container<static_container_traits>`,
  `container<static rtti traits>`); the observer half has no counterpart:
  `type_traits::is_owning_handle` is gone and a `wrapper_traits` wrapper
  converts only within its family, so a raw pointer never becomes a user handle.
- `resolution_access_test.borrowed_wrapper_conversions_cannot_acquire_ownership_implicitly`
  (TYPED, 6 old container types): a pointer inside an external optional is not
  adopted into an optional of an owning handle. **ported**: `RR`
  `next_runtime_registration_borrowing.a_pointer_in_an_external_optional_is_never_adopted`
  (std::optional instead of the test_optional user wrapper; the wrapper
  `interfaces<>` dropped)
- `type_registration_test.runtime_scoped_source_allocation_is_destroyed_when_resolution_throws`:
  a by-value unique dependency of a throwing shared object is destroyed.
  **ported**: `RR`
  `next_runtime_registration.a_value_dependency_lives_as_long_as_the_request`
  (throwing part)
- `type_registration_test.runtime_shared_storage_retains_unique_dependency_source`:
  a unique object a shared object borrows by `&` lives until the shared object
  is destroyed. **changed (`consumed_objects_are_never_borrowed`)**: refusal:
  `RR`
  `next_runtime_registration.an_lvalue_dependency_on_a_consumed_object_is_refused`;
  the retention job with a declared `T&&` dependency: `RR`
  `next_runtime_registration.a_shared_object_does_not_outlive_its_rvalue_dependency`.
- `type_registration_test.runtime_shared_storage_retains_unique_pointer_dependency_source`:
  a pointer made by a custom factory through `context.construct` (persistent
  arena) lives as long as the shared object. **obsolete**: the factory protocol
  `construct(construction_scope, Context&, Container&)` and the runtime arena do
  not exist in next (factories are constructor, function, callable); a `T*` from
  a unique binding hands its ownership to the requester
  (`next_unique.injects_a_new_object_into_each_dependent`); the retention job is
  `RR`
  `next_runtime_registration.a_shared_object_does_not_outlive_its_rvalue_dependency`.
- `type_registration_test.runtime_retained_source_dependency_rolls_back_after_failed_shared`:
  the borrowed unique dependency of a failed shared object is destroyed, and
  made again by a retry. **changed (`consumed_objects_are_never_borrowed`)**:
  `RR`
  `next_runtime_registration.a_failed_request_destroys_the_rvalue_dependency_it_made`
  (with `dependencies<T&&>`)
- `type_registration_test.committed_runtime_retained_source_survives_later_failed_transaction`:
  a later failed request keeps the dependency retained by an earlier one.
  **changed (`consumed_objects_are_never_borrowed`)**: `RR`
  `next_runtime_registration.a_failed_request_keeps_the_rvalue_dependency_of_an_earlier_one`.
- `type_registration_test.runtime_retained_source_storage_resets_before_retained_sources`:
  the shared object is destroyed before the dependency it keeps. **changed
  (`consumed_objects_are_never_borrowed`)**: `RR`
  `next_runtime_registration.a_shared_object_is_destroyed_before_its_rvalue_dependency`.
- `type_registration_test.runtime_policy_unique_shared_unique_keeps_shared_committed`:
  unique -> shared -> unique: the shared object and its dependency are committed
  by the unique request and kept. **changed
  (`consumed_objects_are_never_borrowed`)**: `RR`
  `next_runtime_registration.a_unique_request_commits_the_shared_objects_it_makes`
  (inner dependency `T&&`)
- `type_registration_test.runtime_policy_shared_value_unique_dependency_is_ephemeral`:
  a by-value unique dependency of a shared object is destroyed after its
  construction. **ported**: `RR`
  `next_runtime_registration.a_value_dependency_lives_as_long_as_the_request`.
- `type_registration_test.runtime_local_resolution_container_rolls_back_after_failed_resolution`:
  a local static binding constructed during a failed request is destroyed, and
  constructed again by a retry. **ported**: `RR`
  `next_runtime_registration.a_failed_request_destroys_the_local_objects_it_made`.
- `type_registration_test.first_failed_shared_resolve_resets_partial_storage`: a
  shared object whose construction threw is constructed again by the next
  request. **covered**: `next_construction.retries_after_a_failed_construction`
  (counts the attempts, 2 after a failure and a success, and the address is
  kept)
- `type_registration_test.later_failed_dependent_resolve_preserves_committed_shared_storage`:
  a failed request keeps a shared object committed before it, same address, not
  destroyed. **ported**: `RR`
  `next_runtime_registration.a_failed_request_keeps_the_shared_objects_committed_before_it`.
- `type_registration_test.failed_first_shared_cyclical_resolve_rolls_back_publication`:
  a shared_cyclical handle (with interfaces) whose construction threw is
  published again by a retry. **ported**: `RR`
  `next_runtime_registration.a_failed_shared_cyclical_publication_is_published_again`.
- `type_registration_test.later_failed_resolve_preserves_committed_shared_cyclical_storage`:
  a failed dependent keeps the committed shared_cyclical handle and its address.
  **ported**: `RR`
  `next_runtime_registration.a_failed_request_keeps_a_committed_shared_cyclical_handle`.
- `type_registration_test.runtime_context_construct_uses_explicit_lifetimes`:
  ephemeral objects of a context die with it in reverse order, persistent ones
  with the runtime. **replaced**: `test/next/memory/retained.cpp`
  `next_retained.destroys_its_objects_in_the_reverse_order_they_were_kept`
  (tests `memory/retained.h`); public lifetimes: `RR`
  `next_runtime_registration.a_value_dependency_lives_as_long_as_the_request`,
  `next_runtime_registration.a_shared_object_does_not_outlive_its_rvalue_dependency`.
- `type_registration_test.runtime_context_detection_construction_uses_explicit_scope`:
  a detected construction in the ephemeral scope dies with the request, in the
  persistent one with the runtime. **replaced**: `RR`
  `next_runtime_registration.a_value_dependency_lives_as_long_as_the_request`
  (detected constructor) and
  `next_runtime_registration.a_shared_object_does_not_outlive_its_rvalue_dependency`;
  tests `memory/retained.h` through `runtime_backend::make_with`.
- `type_registration_test.runtime_context_detection_construction_copies_copy_only_values`:
  a copy-only (move deleted) type is constructed by value and as `T&&`.
  **ported**: `RR` `next_runtime_registration.constructs_a_copy_only_object` and
  `next_runtime_registration.refers_to_a_kept_copy_only_object_by_rvalue_reference`
  (runtime and static containers). The old test reached `T&&` through the
  internal `runtime_context::construct`; through the public API the request is
  the behavior change `rvalue_references_to_copy_only_objects`.
- `type_registration_test.runtime_context_explicit_scopes_are_independent`: the
  ephemeral and persistent lifetimes do not depend on the order of construction.
  **replaced**: `test/next/memory/retained.cpp`
  `next_retained.destroys_its_objects_in_the_reverse_order_they_were_kept` and
  `next_retained.a_failed_transaction_destroys_the_objects_kept_during_it`
  (tests `memory/retained.h`)
- `type_registration_test.runtime_context_scratch_destructor_runs_once_on_commit`:
  a temporary of a request that returns is destroyed exactly once. **replaced**:
  `RR`
  `next_runtime_registration.a_value_dependency_lives_as_long_as_the_request`
  (live count, counting copies and moves, returns to 0)
- `type_registration_test.runtime_context_scratch_destructor_runs_once_on_rollback`:
  a temporary of a failed request is destroyed exactly once. **replaced**: `RR`
  `next_runtime_registration.a_value_dependency_lives_as_long_as_the_request`
  (throwing part, live count 0);
  `next_retained.a_failed_transaction_destroys_the_objects_kept_during_it`.
- `type_registration_test.runtime_context_ephemeral_store_preserves_pending_transaction_actions`:
  rollback actions recorded during a request still run when it fails.
  **replaced**: `test/next/memory/transaction_log.cpp`
  `next_transaction_log.a_failed_request_undoes_its_effects_in_reverse` (tests
  `memory/lifetime_slot.h` `basic_transaction_log`)
- `type_registration_test.successful_runtime_registration_keeps_state_until_container_destruction`:
  the factory object of a registration lives until the container is destroyed.
  **ported**: `RR`
  `next_runtime_registration.keeps_the_factory_of_a_registration_until_the_container_is_destroyed`
  (a `dingo::callable` capturing the counter, as next has no user factory class
  protocol)
- `type_registration_test.nested_runtime_registration_rolls_back_with_throwing_root_operation`:
  a registration made by a failed `construct` is undone, and can be registered
  again. **ported**: `RR`
  `next_runtime_registration.a_failed_construct_undoes_the_registrations_it_made`.
- `type_registration_test.nested_runtime_resolution_commits_with_root_resolution`:
  a nested resolve inside a factory commits with the outer request. **ported**:
  `RR`
  `next_runtime_registration.a_nested_request_commits_with_the_outermost_one`.
- `type_registration_test.nested_runtime_resolution_rolls_back_with_throwing_root_resolution`:
  a nested resolve inside a failed factory is undone and made again by a retry.
  **ported**: `RR`
  `next_runtime_registration.a_nested_request_is_undone_with_the_outermost_one`.
- `type_registration_test.failed_duplicate_runtime_registration_rolls_back_constructed_state`:
  a rejected duplicate registration destroys its factory, the first one keeps
  its own. **ported**: `RR`
  `next_runtime_registration.destroys_the_factory_of_a_rejected_registration`.
- `type_registration_test.runtime_binding_state_constructs_child_container_on_demand`:
  the child container of a runtime binding is constructed on first use, once.
  **replaced**: `test/next/backend/local.cpp`
  `next_local_bindings.creates_the_runtime_bindings_on_demand_once` (tests
  `backend/local.h` `local_bindings::runtime_or_create`)
- `type_registration_test.runtime_unique_registration_constructs_nested_container_on_demand`:
  a unique registration resolves a `&` dependency from a local unique binding
  made by a function. **changed (`consumed_objects_are_never_borrowed`)**: `RR`
  `next_runtime_registration.a_unique_registration_resolves_from_its_local_bindings`
  (by-value dependency); refusal of `&`: `RR`
  `next_runtime_registration.an_lvalue_dependency_on_a_consumed_object_is_refused`.
- `type_registration_test.registration_containers_isolate_bindings_in_shared_runtime_domain`:
  with `base<many, ordered>`, local bindings of two registrations stay isolated,
  singular and collection. **ported**: `RR`
  `next_runtime_registration.isolates_local_bindings_in_a_container_of_many`.
- `type_registration_test.registration_static_bindings_and_proxy_registrations_compose`:
  a local static, a local runtime, and a container binding compose (7, 13, 11)
  **covered**: `next_local.composes_static_runtime_and_container_bindings` (same
  types, values, and assertions)
- `type_registration_test.proxy_binding_dependencies_resolve_through_registration_container`:
  nested proxy registrations survive a failed request and serve the retry (31)
  **ported**: `RR`
  `next_runtime_registration.keeps_the_local_bindings_of_proxies_when_a_request_fails`
  (the address check of `child.get()` dropped: the next registry lives in the
  proxy)
- `type_registration_test.registration_static_runtime_and_root_collections_compose`:
  a collection holds the container, local runtime, and local static bindings
  (11, 33, 22) under default traits. **changed
  (`local_bindings_shadow_collections`)**: the local bindings shadow the
  container's for a collection, so it holds the local runtime and the local
  static bindings, {33, 22}:
  `next_local.shadows_the_collections_of_the_container` (with `collection<>`
  declared many). Under default traits a local runtime binding of a key a local
  static one holds is rejected (`duplicates_rejected_in_both_domains`): `RR`
  `next_runtime_registration.rejects_a_local_runtime_binding_of_a_key_a_local_static_one_holds`.
- `type_registration_test.shared_runtime_domain_rolls_back_nested_registration`:
  a registration made by a failed factory is undone, so the retry registers it
  again. **ported**: `RR`
  `next_runtime_registration.a_registration_of_a_failed_factory_is_undone_so_a_retry_registers_it`.
- `type_registration_test.registration_container_scopes_preserve_array_lookup_backend`:
  local bindings of two registrations follow the container's `array<1>` lookup.
  **ported**: `RR`
  `next_runtime_registration.local_bindings_follow_the_dense_array_of_the_container`
  (adds the out-of-range rejection)
- `type_registration_test.registration_container_collections_use_local_entries_or_parent_fallback`:
  a keyed collection holds only the local bindings of its key (22), and falls
  back to the container for another key (33) **ported**: `RR`
  `next_runtime_registration.keyed_collections_hold_the_local_bindings_of_their_key`
  (the collection of key 0 holds the local binding, {22}; the collection of a
  typed key the local bindings do not hold is the container's, {33})

## Group F: static graph

The cases of the old static graph and static bindings. The twelve
`static_execution_traits_test` cases sized the old `static_context`: its
temporary, destructible, and retained-frame slots. The next implementation has
no such context. Temporaries live on the stack, the objects of rvalue requests
in `retained_objects`, and the only precomputed bound is that of the transaction
log (`static_transactions`). Each case is replaced by a behavioral test of the
same paths and by the test of the bound; the frame depth, the slot counts, and
the topological order have no counterpart. In a scratch copy of `include/dingo`,
flipping four of the `next_static_graph` expectations one at a time breaks the
build each time.

### test/registration/binding_graph.cpp

Every case includes old internals (`dingo/static/graph.h`, `static/context.h`,
`static/activation_set.h`) and uses `detail::` types of the old static graph, so
none can be kept. The file does not compile against next.

- `static_graph_test.exposes_dependency_nodes_and_topological_order`: declared
  dependencies become graph edges config \<- service_interface \<- controller,
  the graph is acyclic, and it has a topological order and frame depth.
  **replaced**: `test/next/backend/static_graph.cpp`
  `next_static_graph.declared_dependencies_are_edges_to_the_bindings_they_name`;
  tests `backend/static_backend.h` (`detail::dependency_graph::make`/`find`).
  The topological order and the retained frame depth have no counterpart: next
  precomputes no order or frame and resolves slots on demand.
- `static_graph_test.detects_cycles_without_invalidating_compile_time_bindings_source`:
  a shared a\<->b cycle is found while each binding on its own stays valid.
  **replaced**:
  `next_static_graph.a_cycle_of_declared_dependencies_is_found_with_its_path`
  (each binding `registers<>()`, `find()` gives path 0->1->0); tests
  `backend/static_backend.h`. Rejecting the container: lit
  `test/lit/static-container-rejects-a-declared-cycle.cpp`.
- `static_graph_test.allows_cycles_when_every_cycle_binding_is_shared_cyclical`:
  a cycle made only of shared_cyclical bindings is legal, and a shared owner of
  a node in it resolves. **replaced**:
  `next_static_graph.a_cycle_of_shared_cyclical_bindings_is_legal` (edges
  present, `find().length == 0`); tests `backend/static_backend.h`. Resolution
  of such a declared cycle:
  `next_shared_cyclical.resolves_a_declared_cycle_statically` (read)
- `static_graph_test.rejects_cycles_when_any_cycle_binding_is_not_shared_cyclical`:
  a cycle through a binding that is not shared_cyclical is illegal.
  **replaced**:
  `next_static_graph.a_cycle_through_a_binding_that_is_not_shared_cyclical_is_found`
  (path 1->0->1, starting at the shared binding); tests
  `backend/static_backend.h`. Rejecting the container with that exact binding
  type tree: lit `test/lit/static-container-rejects-a-declared-cycle.cpp`.
- `static_graph_test.partial_analysis_detects_static_cycles_even_with_runtime_dependencies`:
  a dependency with no static binding (one that runtime registrations could
  answer) does not hide a static cycle. **replaced**:
  `next_static_graph.a_dependency_no_static_binding_answers_adds_no_edge_and_hides_no_cycle`;
  tests `backend/static_backend.h`, whose `static_backend` is the static part of
  both `static_container` and the mixed `container`.
- `static_execution_traits_test.unique_only_paths_use_one_temporary_slot_per_unique_binding`:
  a static context is sized with one temporary slot per unique binding of a
  chain. **replaced**: `test/next/containers/static_bindings.cpp`
  `next_static_bindings.constructs_unique_dependencies_and_destroys_their_temporaries`
  (leaf->middle->root, nothing left alive) and
  `next_static_graph.a_static_container_logs_only_the_slots_that_construct_an_object`
  (a unique-only container has `no_transaction_log`); tests
  `backend/static_backend.h`. The old chain passed `leaf &` from a unique
  binding, which next refuses (behavior change
  `consumed_objects_are_never_borrowed`), so the new chain passes by value.
- `static_execution_traits_test.sibling_unique_dependencies_do_not_accumulate_nested_peaks`:
  sibling unique dependencies of one constructor share their temporary peak.
  **replaced**:
  `next_static_bindings.constructs_unique_dependencies_and_destroys_their_temporaries`
  (`pair_root` takes two unique siblings); tests `backend/static_backend.h`.
  next keeps temporaries on the stack, so there is no peak to size.
- `static_execution_traits_test.detected_auto_constructor_dependencies_control_context_bounds`:
  the arguments of a detected constructor decide whether a binding can be
  analysed at compile time. **replaced**:
  `next_static_graph.a_detected_constructor_declares_no_dependencies_and_a_declared_one_does`;
  tests `conversion/construct.h` `declared_arguments` and
  `dependency_graph::any` (spec `graph_rules.cycle`: detected-constructor
  dependencies are known only at construction)
- `static_execution_traits_test.resolution_operations_publish_static_temporary_requirements`:
  each resolution operation names the temporaries it needs (an optional for a
  destructible unique value) **replaced**:
  `next_static_bindings.keeps_the_object_of_an_rvalue_request_until_the_container_is_destroyed`
  (`payload &&` and `std::optional<payload>` from a unique binding; live
  counts); tests `static_backend::resolve_from` and `memory/retained.h`. next
  has no published temporary-type list: it keeps what an rvalue request refers
  to in `retained_objects`.
- `static_execution_traits_test.static_context_frames_are_not_polymorphic`: the
  static context frame has no vtable. **replaced**:
  `next_static_graph.a_static_container_logs_only_the_slots_that_construct_an_object`
  (`!std::is_polymorphic_v` of the static backend); tests
  `backend/static_backend.h`. next has no context frame.
- `static_execution_traits_test.default_stable_leaf_bindings_skip_conversion_cache_discovery`:
  direct and wrapped shared bindings need no conversion cache, while custom
  conversions do. **replaced**:
  `next_static_graph.a_binding_keeps_objects_only_when_a_route_derives_one`
  (`binding_type::keeps`, `context_type` of `conversion/convert.h`). The
  converted `shared_ptr<service>` -> interface binding now keeps a handle
  (behavior change `interface_handles_are_retained`, asserted in the same test).
  The old `detail::conversions` custom-conversion assertion has no counterpart
  (no such class in next)
- `static_execution_traits_test.default_stable_leaf_requests_skip_resolution_discovery`:
  which requests of wrapped, converted, and array shared bindings get a direct
  leaf resolution, and that an array binding serves `T*` but not `T&`.
  **replaced**:
  `next_static_graph.a_binding_serves_the_requests_its_route_reaches`
  (`static_backend::serves_named`); tests `backend/static_backend.h`. The "skip
  discovery" shortcut is old-only. `T&&` is refused (behavior change
  `borrowing_never_moves`); the old test only said it was not a *direct*
  resolution.
- `static_execution_traits_test.retained_frame_depth_counts_retaining_bindings_across_non_retaining_edges`:
  retained frame depth counts only retaining (shared) bindings, across unique
  edges. **replaced**:
  `next_static_bindings.a_shared_object_keeps_nothing_of_its_consumed_dependencies`
  and
  `next_static_graph.a_static_container_logs_only_the_slots_that_construct_an_object`
  (log bound = constructing slots + 1); tests `backend/static_backend.h`
  `static_transactions`. Frame depth itself has no counterpart. The old chain
  borrowed from unique bindings, see `consumed_objects_are_never_borrowed`.
- `static_execution_traits_test.unique_wrapper_storage_paths_account_for_wrapper_materialization_slots`:
  a unique `shared_ptr` binding reserves one destructible slot. **replaced**:
  `next_static_bindings.constructs_unique_dependencies_and_destroys_their_temporaries`
  (unique `shared_ptr<other_leaf>`, the resolved handle and its destruction
  counted) and
  `next_static_graph.a_static_container_logs_only_the_slots_that_construct_an_object`.
- `static_execution_traits_test.unique_value_storage_paths_account_for_optional_conversion_shape`:
  a destructible unique value reserves an optional-sized slot. **replaced**:
  `next_static_bindings.keeps_the_object_of_an_rvalue_request_until_the_container_is_destroyed`
  (`std::optional<leaf>` from `unique<leaf>`)
- `static_execution_traits_test.shared_cyclical_paths_account_for_rollback_temporary_slots`:
  a shared_cyclical binding reserves a rollback slot. **replaced**:
  `next_static_bindings.a_failed_shared_cyclical_construction_is_rolled_back_statically`
  (a failing construction is undone, nothing left alive, and a retry succeeds)
  and
  `next_static_graph.a_static_container_logs_only_the_slots_that_construct_an_object`
  (the shared_cyclical slot counts toward `basic_transaction_log<3, false>`)
- `static_execution_traits_test.destructible_slots_follow_non_trivial_dependency_requests_along_static_paths`:
  `shared_ptr` and `optional` dependencies of a unique service each reserve a
  destructible slot. **replaced**:
  `next_static_bindings.constructs_unique_dependencies_and_destroys_their_temporaries`
  (value and `shared_ptr` dependencies, live count back to 0) and
  `next_static_bindings.keeps_the_object_of_an_rvalue_request_until_the_container_is_destroyed`
  (optional)
- `static_graph_test.annotated_bindings_reuse_compile_time_bindings_lookup`: an
  annotated dependency is an edge to the binding of the annotated interface.
  **replaced**:
  `next_static_graph.an_annotated_dependency_is_an_edge_to_the_annotated_interface`;
  tests `backend/static_backend.h` (`argument_request<annotated>`)

### test/registration/static_bindings_source.cpp

The file includes the old internal `dingo/static/registry.h`, and most cases use
the old registry (`source::type::model`, `dependencies`, `dependency_bindings`,
`selection`). With that include removed, cases 1, 2, 9, 13 and 14 compile and
pass against next unchanged (checked).

- `static_bindings_source_test.borrowable_storage_constructs_and_borrows_its_value`:
  a shared `optional<int>` with interface int serves int, const int, and a
  reference and pointer into the optional. **ported**:
  `test/next/containers/static_bindings.cpp`
  `next_static_bindings.borrows_the_value_of_a_borrowable_storage` (body
  unchanged)
- `static_bindings_source_test.nested_borrowable_storage_uses_the_published_conversion`:
  the int under `optional<optional<variant<int,float>>>` is served by value,
  reference, and pointer. **ported**:
  `next_static_bindings.borrows_the_leaf_of_a_nested_borrowable_storage` (body
  unchanged)
- `static_bindings_source_test.exposes_models_and_dependency_bindings`: the
  registry publishes each binding's model and its declared dependencies resolved
  to bindings. **replaced**: `test/next/backend/static_graph.cpp`
  `next_static_graph.declared_dependencies_are_edges_to_the_bindings_they_name`
  (service -> config edge); `dependency<config &> == config &` in
  `next_static_graph.a_keyed_dependency_is_an_edge_to_the_binding_of_its_key`;
  tests `backend/static_backend.h`. The old `registration_types`/`model<>`
  accessors have no counterpart.
- `static_bindings_source_test.zero_argument_detected_constructor_is_known`: a
  detected zero-argument constructor has an empty dependency list. **replaced**:
  `next_static_graph.a_detected_constructor_declares_no_dependencies_and_a_declared_one_does`
  (`declared_arguments` of a detected constructor is void, including the
  zero-argument one: known only at construction, spec `graph_rules.cycle`);
  tests `conversion/construct.h`.
- `static_bindings_source_test.constructor_typedef_bindings_reuse_detected_dependencies`:
  a `DINGO_CONSTRUCTOR` binding publishes its declared arguments as
  dependencies. **replaced**:
  `next_static_graph.a_detected_constructor_declares_no_dependencies_and_a_declared_one_does`
  (`type_list<config &>` and the edge); resolution in
  `next_static_bindings.resolves_the_dependencies_of_a_constructor`; tests
  `conversion/construct.h`.
- `static_bindings_source_test.plain_constructor_dependencies_follow_default_detection_metadata`:
  a plain constructor's dependencies are whatever detection reports.
  **replaced**:
  `next_static_graph.a_detected_constructor_declares_no_dependencies_and_a_declared_one_does`
  (void, no edges); tests `conversion/construct.h`, `backend/static_backend.h`.
- `static_bindings_source_test.plain_constructor_resolves_default_detected_dependencies`:
  a detected constructor gets the shared config of the static container.
  **ported**: `next_static_bindings.resolves_the_dependencies_of_a_constructor`
  (detected and declared); metadata part as in the row above.
- `static_bindings_source_test.annotated_dependencies_normalize_to_annotated_interfaces`:
  an annotated dependency resolves to the annotated interface's binding.
  **replaced**:
  `next_static_graph.an_annotated_dependency_is_an_edge_to_the_annotated_interface`;
  tests `backend/static_backend.h`.
- `static_bindings_source_test.container_invokes_annotated_static_bindings_by_tag`:
  `invoke` of a mixed container passes annotated `shared_ptr` interfaces chosen
  by tag. **ported**:
  `next_static_bindings.invokes_a_callable_with_annotated_static_bindings` (body
  unchanged, types renamed)
- `static_bindings_source_test.supports_multibindings_and_keyed_multibindings`:
  two static bindings of one tag key are held, and requests by key count 0/2/1
  bindings. **changed**: `duplicates_rejected_in_both_domains` (default traits
  now reject the two `first_key` bindings when the container is formed). New
  behavior:
  `next_static_graph.a_second_binding_of_a_key_is_a_duplicate_unless_many`
  (rejected under default, held under `typed<first_key, iface, many>`), and
  `next_static_graph.a_request_names_every_binding_of_its_key` (with many:
  counts 0/2/1, ambiguous vs. served)
- `static_bindings_source_test.indexed_no_key_selection_preserves_binding_cardinality`:
  the same binding listed twice is ambiguous, a unique one is found, a missing
  one is not found. **changed**: `duplicates_rejected_in_both_domains` (a
  repeated binding is now rejected unless many). New behavior:
  `next_static_graph.a_second_binding_of_a_key_is_a_duplicate_unless_many`
  (`first, first`) and
  `next_static_graph.a_request_names_every_binding_of_its_key` (under many:
  `fixed_count` 2/1/0, `find_binding` ambiguous/found/not_found)
- `static_bindings_source_test.keyed_dependencies_resolve_against_matching_keyed_bindings`:
  a `dependency<config &, key_type<K>>` resolves to the binding with key K.
  **replaced**:
  `next_static_graph.a_keyed_dependency_is_an_edge_to_the_binding_of_its_key`
  (`backend/static_backend.h`) and
  `next_static_bindings.resolves_a_keyed_dependency_from_the_binding_of_its_key`
  (identity of the injected config)
- `static_bindings_source_test.static_container_resolves_shared_cyclical_bindings`:
  two declared shared_cyclical bindings resolve, each referring to the other,
  with stable identity. **covered**:
  `next_shared_cyclical.resolves_a_declared_cycle_statically` (read:
  `static_container`, declared `dependencies<>`, both shared_cyclical, mutual
  references and repeated identity checked)
- `static_bindings_source_test.container_resolves_static_shared_cyclical_interface_handles`:
  a mixed container resolves a shared_cyclical cycle through an interface of a
  `shared_ptr` binding. **ported**:
  `next_static_bindings.a_mixed_container_resolves_shared_cyclical_interface_handles`
  (body unchanged, types renamed)
- `static_bindings_source_test.static_container_resolves_collection_of_multiple_interface_bindings`:
  a static container injects a vector of two unkeyed bindings of an interface.
  **changed**: `duplicates_rejected_in_both_domains` (without a many declaration
  the container is now rejected). New behavior:
  `next_static_bindings.injects_a_collection_of_static_bindings_of_a_domain_declared_many`
  (static part, `collection<collection_interface>` declared) and
  `next_static_graph.a_second_binding_of_a_key_is_a_duplicate_unless_many`.
- `static_bindings_source_test.container_resolves_static_collection_of_multiple_interface_bindings`:
  the same through the mixed container. **changed**:
  `duplicates_rejected_in_both_domains`;
  `next_static_bindings.injects_a_collection_of_static_bindings_of_a_domain_declared_many`
  (mixed part)

### test/registration/static_parent_container.cpp

Includes only public headers. Against next, only case 1 fails to compile; the
other six compile and pass unchanged (checked with g++ and clang++). Because the
file cannot be kept whole, all seven move to
`test/next/containers/static_parents.cpp`, with the six bodies unchanged.

- `static_parent_container_test.child_dependency_ambiguity_does_not_fall_back_to_parent`:
  two static bindings of config in a mixed child make the child's `service`
  dependency ambiguous (it throws) instead of taking the parent's config.
  **changed**: `duplicates_rejected_in_both_domains`, which lists
  `matrix:dingo_matrix_test_static_parent_container/*`. The
  `lookup already registered` static_assert is that change: the child is now
  rejected when it is formed, and with many declared a request among two static
  bindings is rejected at compile time. New behavior:
  `test/next/containers/static_parents.cpp`
  `next_static_parents.child_dependency_ambiguity_does_not_fall_back_to_parent`
  (many declared, one static and one runtime binding in the child: throws
  `type_ambiguous_exception` and does not fall back to the parent),
  `next_static_graph.a_second_binding_of_a_key_is_a_duplicate_unless_many`, lit
  `test/lit/static-container-rejects-duplicate-keys.cpp`.
- `static_parent_container_test.static_child_context_is_sized_for_parent_temporaries`:
  an empty static child resolves a unique destructible type of its static
  parent. **ported**:
  `next_static_parents.static_child_context_is_sized_for_parent_temporaries`
  (unchanged body)
- `static_parent_container_test.static_child_context_is_sized_for_grandparent_temporaries`:
  the same across two generations. **ported**:
  `next_static_parents.static_child_context_is_sized_for_grandparent_temporaries`
  (unchanged)
- `static_parent_container_test.static_empty_child_resolves_parent_retained_conversion`:
  an empty static child returns the parent's shared interface reference, with
  the same identity. **ported**:
  `next_static_parents.static_empty_child_resolves_parent_retained_conversion`
  (unchanged)
- `static_parent_container_test.static_non_empty_child_resolves_parent_retained_conversion`:
  the same with a child that has its own binding, which also resolves.
  **ported**:
  `next_static_parents.static_non_empty_child_resolves_parent_retained_conversion`
  (unchanged)
- `static_parent_container_test.static_non_empty_child_retains_parent_dependency_temporaries`:
  a parent shared object built from a unique temporary is served through the
  child. **ported**:
  `next_static_parents.static_non_empty_child_retains_parent_dependency_temporaries`
  (unchanged)
- `static_parent_container_test.static_non_empty_child_resolves_grandparent_retained_conversion`:
  the grandparent's shared interface reference is served through a non-empty
  parent and child. **ported**:
  `next_static_parents.static_non_empty_child_resolves_grandparent_retained_conversion`
  (unchanged)

## Group G: lit tests of the current implementation

The lit tests under `test/lit/` that compile against the current implementation
(`%dingo_cxx`). A port is named after the old file with a `next-` prefix and
compiles with `%dingo_next_cxx`; every one passes `lit` with `CXX=g++` (GCC 13)
and `CXX=clang++` (Clang 18). A rejection is checked by the fixed part of the
message of its diagnostic, which the literal and the formatted message share.
Some ports keep a name that ends in `-is-rejected` although the program is now
accepted; the row says so and names the behavior change, and the leading comment
of the file says so too.

Two rows are attributed to `keys_in_every_container`, "Every key kind works in
every container": `index-empty-definition-lookup-is-rejected.cpp` (a runtime key
value without a declared lookup) and
`static-fixed-runtime-key-selector-is-required.cpp` (a static fixed key without
a declared `associative`). The baseline text of the change lists neither case;
`spec/content/lookup_declarations.py` says that a domain without a declaration
follows the default declaration, which is the new behavior of both.

### test/lit (lit tests of the current implementation, `%dingo_cxx`)

- `collection-missing-traits-specialization.cpp`: a collection request of a type
  without collection_traits is rejected. **ported**:
  `test/lit/collection-missing-traits-specialization.cpp`
  (collection_traits_missing). `gen:collection_traits_missing` is the same
  program.
- `construction-ambiguous-is-rejected.cpp`: `construct<T>()` of a type with two
  single-argument constructors is rejected. **ported**:
  `test/lit/construction-ambiguous-is-rejected.cpp` (constructor_not_detected).
  `gen:constructor_not_detected` covers a private constructor, not an ambiguous
  one.
- `construction-bounded-array-rejects-too-many-initializers.cpp`:
  `constructor_traits<int[2]>::construct(1,2,3)` is rejected. **obsolete**: Next
  has no way to build an array from element initializers. A stored array is
  value-initialized in place (`construct_stored` in conversion/construct.h), and
  no factory can return an array type. `constructor_traits` is internal to the
  old code.
- `construction-dynamic-array-requires-size-or-initializers.cpp`: building an
  unbounded array needs an explicit size or initializers. **ported**:
  `test/lit/construction-dynamic-array-requires-size-or-initializers.cpp`:
  `unique_ptr<int[]>` without a factory gives no_default_factory. The old test
  called internal `detail::make_dynamic_array`.
- `construction-generic-constructor-requires-explicit-factory.cpp`: a template
  constructor is detected as generic and needs an explicit factory. **covered**:
  `gen:constructor_generic`: same rejection of a template constructor. Next
  rejects the old program with constructor_generic too.
- `container-local-bindings-cycle-is-rejected.cpp`: a cycle in the local
  `bindings<>` of a runtime `register_type` is rejected at compile time.
  **ported**: `test/lit/container-local-bindings-cycle-is-rejected.cpp`
  (type_recursion). `gen:type_recursion` covers only a static container.
- `container-static-bindings-cycle-is-rejected.cpp`: a cycle among the static
  bindings of a mixed container is rejected when the type is formed. **ported**:
  `test/lit/container-static-bindings-cycle-is-rejected.cpp` (type_recursion)
- `container-static-fixed-runtime-key-selector-is-rejected.cpp`: a mixed
  container rejects a `key_type<K, V>` static binding. **changed**:
  `keys_in_every_container` ("a key in a mixed container"). Next accepts. New
  behavior: `next_mixed.selects_among_static_and_runtime_bindings` (static
  `key_type<int, 7>` without lookups, resolved by 7)
- `harness-container-header-compiles.cpp`: `<dingo/container.h>` alone compiles
  a container and a resolve. **ported**:
  `test/lit/harness-container-header-compiles.cpp` (compile only, as before)
- `index-conflicting-typed-key-selector-is-rejected.cpp`: `typed<K, I, one>` and
  `typed<K, I, many>` for one domain are rejected. **ported**:
  `test/lit/index-conflicting-typed-key-selector-is-rejected.cpp`
  (lookup_declared_twice). `gen:lookup_declared_twice` declares `single<I>` and
  `collection<I>`, not two typed declarations.
- `index-duplicate-interface-key-is-rejected.cpp`: two `associative<K, I>`
  declarations of one domain are rejected. **ported**:
  `test/lit/index-duplicate-interface-key-is-rejected.cpp`
  (lookup_declared_twice: two associative declarations that agree)
- `index-empty-definition-lookup-is-rejected.cpp`: a request by a runtime key
  value with no declared lookup is rejected at compile time. **changed**:
  `keys_in_every_container`. Next accepts the program and throws
  type_not_found_exception at runtime. Tested by
  `next_keys.selects_by_tag_and_by_value` (`container<>` without lookups,
  `resolve<iface &>(9)` throws). Attribution not fully confirmed, see the group
  note.
- `index-missing-definition-is-rejected.cpp`: a key value for an interface whose
  domain no lookup declares is rejected. **changed**:
  `keys_apply_to_every_interface`. Next accepts the program. New behavior:
  `test/lit/index-missing-definition-is-rejected.cpp` (runs: found by its key,
  not found without it)
- `index-no-key-selector-is-rejected.cpp`: `single<I>` and `collection<I>` for
  one domain are rejected. **covered**: `gen:lookup_declared_twice`: the same
  `single<I>` and `collection<I>` declarations, rejected with
  lookup_declared_twice.
- `indexed-external-string-selector.cpp`: an external request injects a
  `std::string` key into constructor injection (runs) **covered**:
  `next_lookups.injects_a_fixed_string_key_into_a_string_domain`
  (test/next/containers/lookups.cpp): same injection through the public
  `key_type<std::string, fixed_string{"json"}>`. The old program used internal
  `dingo::detail` hooks, which next lacks, so next rejects it.
- `key-invalid-fixed-value-is-rejected.cpp`: `key_type<T, V>` with V not usable
  as `T{V}` is rejected. **ported**:
  `test/lit/key-invalid-fixed-value-is-rejected.cpp` (key_value_not_of_key_type,
  the old key used by a registration). `gen:key_value_not_of_key_type` rejects
  forming the key type alone, as the old program did.
- `key-structural-fixed-value-compiles.cpp`: a fixed key of a structural class
  type yields its value. **ported**:
  `test/lit/key-structural-fixed-value-compiles.cpp` (runs; runtime and static,
  resolve and injection). The old test read internal `detail::key_value_traits`.
- `registration-array-requires-matching-element-type.cpp`: `storage<int[2]>`
  with `interfaces<float>` is rejected. **covered**: `gen:interface_in_array`:
  an array storage with a declared interface. Next rejects the old program with
  interface_in_array.
- `registration-array-requires-matching-type tree.cpp`: `storage<int[2]>` with
  `interfaces<int[3]>` is rejected. **covered**: `gen:interface_in_array`. Next
  rejects the old program with interface_in_array.
- `registration-function-interface-is-rejected.cpp`: `interfaces<int()>` is
  rejected. **ported**:
  `test/lit/registration-function-interface-is-rejected.cpp`
  (no_interface_served). `gen:no_interface_served` covers a consumed optional.
- `registration-incomplete-interface-type-is-rejected.cpp`: an incomplete
  interface is rejected. **ported**:
  `test/lit/registration-incomplete-interface-type-is-rejected.cpp`
  (no_interface_served)
- `registration-incomplete-registered-type-is-rejected.cpp`:
  `storage<incomplete *>` with an interface is rejected. **ported**:
  `test/lit/registration-incomplete-registered-type-is-rejected.cpp`
  (interface_not_derived)
- `registration-interface-requires-pointer-convertibility.cpp`: a unique binding
  with an unrelated interface is rejected. **ported**:
  `test/lit/registration-interface-requires-pointer-convertibility.cpp`
  (no_interface_served for unique). `gen:interface_not_derived` covers a shared
  binding.
- `registration-missing-interface-is-rejected.cpp`: internal `type_registration`
  with user policy types fails to deduce an interface. **obsolete**: Next has no
  `type_registration` and no `rebind_t` storage or factory policy classes.
  Without `interfaces<>` the interface is the stored leaf, so deducing it cannot
  fail.
- `registration-missing-scope-is-rejected.cpp`: a registration without `scope<>`
  is rejected. **covered**: `gen:registration_without_scope`
  (`register_type<storage<config>>()`)
- `registration-missing-storage-is-rejected.cpp`: a registration without
  `storage<>` is rejected. **covered**: `gen:registration_without_storage`
  (`register_type<scope<shared>>()`)
- `registration-reference-interface-is-rejected.cpp`: `interfaces<service &>` is
  rejected. **ported**:
  `test/lit/registration-reference-interface-is-rejected.cpp`
  (no_interface_served)
- `registration-void-interface-is-rejected.cpp`: `interfaces<void>` is rejected.
  **ported**: `test/lit/registration-void-interface-is-rejected.cpp`
  (no_interface_served)
- `runtime-registration-fixed-key-is-rejected.cpp`: a runtime registration with
  `key_type<K, V>` is rejected. **changed**: `keys_in_every_container` ("a fixed
  key in a runtime registration"). Next accepts. New behavior:
  `next_keys.selects_by_tag_and_by_value` (registers `key_type<int, 8>`,
  resolved by `key_value{8}`)
- `runtime-registration-multiple-key-ambiguous-is-rejected.cpp`: one key value
  for a binding of two keyed interfaces compiles. **ported**:
  `test/lit/runtime-registration-multiple-key-ambiguous-is-rejected.cpp`
  (compile only, as before. Despite its name, the old test asserts the program
  compiles)
- `runtime-registration-multiple-key-duplicate-is-rejected.cpp`: two key values
  of one type for one interface are rejected. **covered**:
  `gen:registration_key_values`: two `key_value` of one type, same reason. Next
  rejects the old program with registration_key_values.
- `runtime-registration-multiple-key-extra-is-rejected.cpp`: a key value of a
  type with no declared lookup is rejected. **changed**:
  `keys_apply_to_every_interface`. Next accepts the program. New behavior:
  `test/lit/runtime-registration-multiple-key-extra-is-rejected.cpp` (runs:
  found by 7 and by "extra", same object)
- `runtime-registration-multiple-key-missing-is-rejected.cpp`: a registration
  must give a key value for every declared key lookup of its interface.
  **covered**: `gen:registration_key_value_missing`: lookups by `int` and `long`
  declared and only `key_value{7}` given. Next rejects the old program, with
  `int` and `std::string`, with registration_key_value_missing too.
- `static-bindings-callable-factory-is-rejected.cpp`: a callable factory is
  rejected in static bindings. **ported**:
  `test/lit/static-bindings-callable-factory-is-rejected.cpp`
  (factory_not_declared). The old test formed the internal `static/registry.h`;
  the port forms a `static_container`. `gen:factory_not_declared` names a
  function object type, not a `dingo::callable`.
- `static-bindings-duplicate-interface-is-rejected.cpp`: (compiles) internal
  registry: two static bindings of one tag key make the selection ambiguous.
  **changed**: `duplicates_rejected_in_both_domains`. In next,
  `static_container` rejects the duplicate when the type is formed. Tested by
  `test/lit/static-container-rejects-duplicate-keys.cpp` and
  `gen:lookup_already_registered`. The old test used internal headers.
- `static-bindings-missing-dependency-is-rejected.cpp`: a declared dependency no
  binding serves is rejected when the static container is formed. **ported**:
  `test/lit/static-bindings-missing-dependency-is-rejected.cpp`
  (type_not_found_required_by; the program plus a request).
  `gen:type_not_found_required_by` rejects the same binding, shared instead of
  unique, when the container is formed, and next rejects the old program
  unchanged as well.
- `static-container-ambiguous-keyed-multibinding-is-rejected.cpp`: two static
  bindings of one tag key: resolving by the key is rejected. **covered**:
  `gen:lookup_already_registered` and
  `test/lit/static-container-rejects-duplicate-keys.cpp`: next rejects the
  duplicate when the type is formed (`duplicates_rejected_in_both_domains`)
- `static-container-ambiguous-multibinding-is-rejected.cpp`: two unkeyed static
  bindings of one interface: resolve is rejected. **covered**:
  `gen:lookup_already_registered`: the same two unkeyed bindings, rejected when
  formed.
- `static-container-cycle-is-rejected.cpp`: a declared dependency cycle in a
  static container is rejected. **covered**: `gen:type_recursion` and
  `test/lit/static-container-rejects-a-declared-cycle.cpp`.
- `static-fixed-runtime-key-duplicate-is-rejected.cpp`: two static bindings of
  one fixed key in a domain of one are rejected. **covered**:
  `test/lit/static-container-rejects-duplicate-keys.cpp` (`key_type<int, 7>`
  twice, lookup_already_registered)
- `static-fixed-runtime-key-many-child-singular-ambiguity-is-rejected.cpp`: two
  bindings of a key in a static child make a request ambiguous, though the
  parent binds the key once. **ported**:
  `test/lit/static-fixed-runtime-key-many-child-singular-ambiguity-is-rejected.cpp`
  (type_ambiguous)
- `static-fixed-runtime-key-many-duplicate-storage-is-rejected.cpp`: a static
  domain declared many rejects two bindings of one implementation and key.
  **changed**: `duplicates_rejected_in_both_domains` ("a domain declared many
  accepts any number"). Next accepts the program. New behavior:
  `test/lit/static-fixed-runtime-key-many-duplicate-storage-is-rejected.cpp`
  (runs: the collection of the key holds 2 objects)
- `static-fixed-runtime-key-many-singular-ambiguous-is-rejected.cpp`: a singular
  request by a fixed key bound twice in a domain declared many is rejected.
  **ported**:
  `test/lit/static-fixed-runtime-key-many-singular-ambiguous-is-rejected.cpp`
  (type_ambiguous). `gen:type_ambiguous` covers only the unkeyed case.
- `static-fixed-runtime-key-many-singular-missing-is-rejected.cpp`: a static
  request by an unbound fixed key, with another key bound, is rejected.
  **ported**:
  `test/lit/static-fixed-runtime-key-many-singular-missing-is-rejected.cpp`
  (type_not_found_keyed). `gen:type_not_found_keyed` covers an empty container
  with a tag key.
- `static-fixed-runtime-key-selector-is-required.cpp`: a static `key_type<K, V>`
  binding requires a declared `associative` lookup. **changed**:
  `keys_in_every_container`. Next accepts the program. New behavior:
  `next_keys.selects_static_bindings_by_key` (`static_container<bindings>` with
  default traits, fixed keys 7 and 8). Attribution not fully confirmed, see the
  group note.
- `static-rvalue-interface-wrapper-request-is-rejected.cpp`:
  `shared_ptr<iface> &&` from a shared `shared_ptr<impl>` is rejected in static
  and mixed containers. **ported**:
  `test/lit/static-rvalue-interface-wrapper-request-is-rejected.cpp`
  (type_not_convertible). Two RUN lines, the second for the mixed container
  alone.
- `storage-header-is-required.cpp`: `scope<dingo::unique>` without
  `<dingo/storage/unique.h>` is rejected. **changed**:
  `scope_tags_need_no_header`: next accepts the old program. New behavior:
  `test/next/containers/scope_headers.cpp`
  `next_scope_headers.the_container_declares_the_scopes_of_the_library`.
  `test/lit/storage-header-is-required.cpp` keeps the old name for another
  program: a scope tag without `dingo::scope_traits` is rejected with
  scope_undeclared, the program of `gen:scope_undeclared`.
- `storage-missing-materialization-traits-specialization.cpp`: internal
  `storage_materialization_traits` of an unknown tag is rejected. **obsolete**:
  The old storage traits do not exist in next, and `wrapper_extension_point`
  says they are not public API. Wrappers are described by
  `dingo::wrapper_traits`.
- `storage-requirements-are-enforced.cpp`: shared_cyclical rejects an interface
  that is a virtual base. **covered**: `gen:cyclical_virtual_interface` (the
  same program plus a resolve) and
  `test/lit/shared-cyclical-rejects-virtual-interfaces.cpp`.
- `type-conversion-missing-required-access-is-rejected.cpp`: a
  `type_conversion_traits` specialization without `required_access` is rejected.
  **ported**: `test/lit/type-conversion-missing-required-access-is-rejected.cpp`
  (conversion_access_undeclared), through the public `type_conversion_traits`
  and a converting interface. The old test used internal `detail` and the old
  `type_traits`. `gen:conversion_access_undeclared` is the same program.
- `type-duplicate-variant-alternatives-are-rejected.cpp`: a variant with
  duplicate alternatives is rejected. **covered**: `gen:alternatives_repeated`:
  `storage<std::variant<int, int>>` registered through the public API is
  rejected with alternatives_repeated. The old test used internal
  `detail::alternative_type_alternatives`; the generated program is its public
  equivalent.

## Group H: other tests, tools, and targets

These build or probe the current implementation outside the unit tests and the
lit tests above. "Kept with next as implementation" means the entry builds
against the next implementation, which the `dingo` target of a development build
selects, and "compares against the baseline" means the entry reads the current
implementation from the `include/` of the commit in
`test/differential/baseline-commit` instead of the working tree.

### Generated matrix (`test/matrix`)

The families of `test/matrix/README.md`, generated by `test/matrix/generate.py`
into one executable per feature or suite, `dingo_matrix_test_<name>`. They link
the `dingo` target, so they build against the next implementation, and the
differential tests build them against the baseline too. Where a behavior change
declares a difference from the baseline, the matrix asserts the behavior of the
next implementation, and that of the baseline under `DINGO_MATRIX_BASELINE`,
which a differential build of the baseline defines, as M9.2 describes:
`lifetime_counts` (`stored_objects_are_constructed_in_place`,
`consuming_moves_minimally`), `local_bindings`
(`local_bindings_shadow_collections`), `lookup_route_regressions`
(`collections_of_a_domain_of_one`), `runtime_exception_regressions`
(`empty_collections_are_valid`), `runtime_lookup_routing_regressions`
(`keys_apply_to_every_interface`), and `runtime_unique_reference_regressions`
(`consumed_objects_are_never_borrowed`).

- registration family (`resolve_concrete`, `resolve_interface`,
  `resolve_wrapper`, `resolve_unique_wrapper`, `custom_wrappers`,
  `nested_wrappers`, `resolve_collection`, `resolve_keyed`,
  `resolve_keyed_collection`, `resolve_indexed`, `construct`,
  `construct_collection`, `construct_invoke`, `local_bindings`,
  `nested_container`, `annotated`, `variant`, `array`, `factory_override`,
  `lifetime_counts`, `mixed_external_dependency`, `mixed_singular_ambiguity`,
  `custom_allocator`, `custom_rtti`, `explicit_dependencies`): registers,
  resolves, constructs, and counts the lifetimes of each feature across the
  container, scope, stored type, exposed type, and request axes. **kept** with
  next as implementation.
- invoke family (`invoke`): dispatches callables (lambdas, `std::function`,
  function pointers, member functions, functors, move-only callables) over the
  dependency provisionings of the registration family. **kept** with next as
  implementation.
- scenario family (`conversion_delivery_regressions`,
  `conversion_qualification_regressions`, `indexed_registration_regressions`,
  `lookup_route_regressions`, `nested_container`,
  `runtime_construct_dependency_regressions`, `runtime_container_regressions`,
  `runtime_exception_regressions`, `runtime_lookup_routing_regressions`,
  `runtime_unique_reference_regressions`, `shared_cyclical_regressions`,
  `static_indexed_regressions`, `static_parent_container`): runs complete
  regression and parent-container scenarios on the containers each applies to.
  **kept** with next as implementation.
- `constructor_detection` family: crosses the detection backends (portable,
  msvc) and modes (arity, signature) with constructor type_trees, and argument
  storages with conversion categories. **kept**, against the detection of the
  next implementation (`constructor/constructor_detection.h`, the backends
  `portable_constructor_detection` and `msvc_constructor_detection`, the modes
  `constructor_arity` and `constructor_signature`): the same axes, the selected
  type trees spelled by `dingo::dependency<T, key_type<S>>`, and each storage
  and conversion category checked by the request `core::constructor_argument`
  makes through `core::invoke_constructor`. It names the core detection, which
  the baseline lacks, so the differential tests exclude it.
- `dependency_composition_resolve`, `dependency_composition_invoke` (four
  executables each): resolves and invokes recursive dependency type compositions
  across registration modes and scopes. **kept** with next as implementation.
- `shared_cyclical` family: resolves two-node cycles across container,
  registration mode, node storage, and the dependency edge of each direction.
  **kept** with next as implementation.
- `dingo_matrix_generator_test`: tests the generator in Python
  (`test/matrix/generator_test.py`). **kept**; it does not depend on the
  implementation.
- `test/support/custom_wrappers.h`, `test/support/resolution_test_types.h`: user
  wrappers and types of the matrix. **kept** with next as implementation; the
  other headers of `test/support/` served only `dingo_unit_test` and are removed
  with it.

### Other targets

- FetchContent test (`test/fetchcontent`): builds a program that includes
  `<dingo/container.h>` against dingo v0.2.0 fetched by FetchContent, in CI.
  **kept**; it builds a released tag, not the working tree, and changes only
  when its `GIT_TAG` moves to a release with the next implementation.
- `test/lit/contract-baseline.t` of the current implementation: checks the
  contract measurements of the current implementation against
  `test/contracts/baseline.json`. **replaced** by the check of the next
  implementation, which is now `test/lit/contract-baseline.t`, against the
  baseline of the next implementation, now `test/contracts/baseline.json`.
- `test/lit/public-api.t` of the current implementation: compiles every public
  include path and name of `spec/content/public_api.py` against the current
  implementation, which must lack the entries an `api:` behavior change
  declares. **replaced** by the check of the next implementation, which is now
  `test/lit/public-api.t`; `public_api_check.py --implementation baseline`
  checks the baseline, which lacks the changed entries.
- `test/lit/spec-scopes.t` (`test/spec/scopes_check.py`): compares the scope
  descriptors of the spec with the `storage_scope_policy` of the current
  implementation. **changed**: `scopes_check.py` compares against the baseline,
  with a `--cxx-command` that names its include directory, and is no lit test;
  the scopes of the next implementation are generated from
  `spec/content/scopes.py` (`storage/scopes.gen.h`).
- `test/lit/codegen-static-probes.cpp`: checks the size and the instructions of
  representative static and runtime resolutions. **kept** with next as
  implementation. The limits of Clang 18 and of the mixed probes of GCC 13 are
  the sizes of the next implementation, which these versions do not fold as
  later ones do, and Clang 18 skips the checks that a static probe is a tiny
  branch-free function and smaller than its runtime path; a decision of the
  user, recorded in the Decision Log of 2026-10-21.
- `test/lit/harness-smoke.t`: checks that lit and FileCheck run. **kept**; it
  does not depend on the implementation.
- `dingo_benchmark` (`benchmark/container/basic.cpp`, `container/dingo.cpp`,
  `index/index.cpp`, optional `comparison/fruit.cpp`): benchmarks containers and
  indexes. **kept** with next as implementation; the benchmarks of the old
  arena, container runtime, and runtime transactions in `container/dingo.cpp`
  are **obsolete**, as the next implementation has none of them, and `basic.cpp`
  no longer uses the internal `dingo/memory/aligned_storage.h`.
- `dingo_resolve_benchmark` (`benchmark/resolve/resolve.cpp`): benchmarks
  resolution through the public API alone, with the requested binding among `N`
  others. **kept** with next as implementation.
- `dingo_next_resolve_benchmark`: the same source against `dingo::next`.
  **replaced** by `dingo_resolve_benchmark`, the same program now that next is
  the implementation.
- examples (`examples/`, 26 `dingo_example_*` targets): build and run the
  documented examples, through public headers, against the `dingo` target.
  **kept** with next as implementation. `examples/storage/array.cpp` no longer
  resolves an array leaf as an owning array handle
  (`arrays_are_not_adopted_into_owning_handles`), and
  `examples/registration/collection.cpp` builds once the next implementation
  instantiates no copy of a collection of move-only elements and no default
  append its traits cannot make.

### Tools that probe the current implementation

- `test/routes/extract.py`: records, for every domain, leaf kind, scope, stored
  type tree, and request, what the library does. **changed**: the next
  implementation by default; `--implementation baseline` with the
  `--include-dir` of the baseline extracts the table of the baseline.
- `test/routes/conform.py`: compares a route table with the reference
  implementation. **kept**: `--exact` for the table of the next implementation,
  and every difference of the table of the baseline declared by a behavior
  change.
- `test/routes/messages.py`: compares the exception messages of the table of the
  next implementation with those of the current one, every difference declared
  by a `messages:` case. **changed**: compares against the table of the
  baseline.
- `test/routes/summarize.py`, `test/routes/witness.py`: summarize the
  differences between the domains of a table; run the witnesses of the
  conversion rules against the next implementation. **kept** with next as
  implementation.
- `test/selection/extract.py`, `test/selection/conform.py`: record how the
  library selects bindings by key, and compare it with
  `spec/reference/selection.py`. **changed**: the next implementation by
  default, with the traits of the mixed container; `--implementation baseline`
  with the `--include-dir` of the baseline extracts the table of the baseline.
- `test/contracts/baseline.json` (`probe.py`, `evaluate.py`): the contract
  measurements of the current implementation. **replaced** by the baseline of
  the next implementation, now `test/contracts/baseline.json`; `probe.py`
  measures the next implementation only, and the header cost contracts of the
  spec are the maximum of its baseline.
- `test/differential` (`run.py`, `compare.py`, `recorder.cpp`,
  `baseline-commit`): builds the matrix against the working tree and against the
  pinned baseline commit, records every assertion, and requires every difference
  to be declared by a `matrix:` behavior change. **kept**: the working-tree side
  is the next implementation, and `--implementation` is gone.
