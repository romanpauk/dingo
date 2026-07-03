# Generated Matrix Test Model

The matrix is described as independent axes, then filtered into valid cases.
The generator should not keep hand-written recipes as the primary model. Recipes
are an output of axis selection.

## Axes

### Feature

- `resolve_concrete`: resolve the registered concrete type.
- `resolve_interface`: resolve through `interfaces<...>`.
- `resolve_wrapper`: resolve an owning or sharing wrapper directly.
- `custom_wrappers`: resolve through user-defined wrapper traits.
- `nested_wrappers`: resolve nested smart-pointer, variant, and array wrapper
  combinations.
- `resolve_collection`: resolve all unkeyed bindings for an interface.
- `resolve_keyed`: resolve one typed-key lookup binding.
- `resolve_keyed_collection`: resolve all bindings for a key.
- `resolve_indexed`: resolve one runtime-key lookup binding.
- `construct`: construct an unregistered type from registered dependencies.
- `invoke`: invoke a callable from registered dependencies.
- `construct_collection`: construct a collection with default or custom insertion.
- `local_bindings`: resolve dependencies from bindings attached to a registration.
- `nested_container`: resolve through child and parent containers.
- `annotated`: disambiguate registrations with `annotated<T, Tag>`.
- `variant`: construct or resolve a variant and its held alternative.
- `array`: resolve raw and smart-array storage forms.
- `factory_override`: construct through explicit callable, function, or
  constructor factories.
- `lifetime_counts`: verify selected storage forms construct, move, copy, and
  destroy the stored value with expected counts.
- `mixed_external_dependency`: combine compile-time bindings with a runtime
  external dependency.
- `custom_allocator`: use a caller-selected allocator for container
  bookkeeping.
- `custom_rtti`: use a caller-selected RTTI provider for runtime lookup.

Negative compile-time checks stay in `test/lit`. The generated matrix covers
valid behavior combinations.

### Feature Case

Most features use a single default case. Dispatch-heavy features use this axis
to describe the behavior under test without hiding it in resolved-type checks.

- `invoke`: inferred lambdas, explicit signatures, `std::function`, function
  pointers, static member functions, member function pointers, mutable and
  generic lambdas, ref-qualified and `noexcept` functors, move-only functors,
  multi-argument callables, and `std::move_only_function` when available.
- `factory_override`: no-argument function factories, function factories with
  dependencies, explicit constructor factories, detected constructors,
  `DINGO_CONSTRUCTOR` typedef constructors, callable registration factories,
  and explicit callable construction.

Factory case entries own the registration factory expression for the behavior
under test. The stored, exposed, and resolved type axes only select the C++
fixture type needed by that case.

### Registration Mode

- `runtime`: registrations are added with `register_type<...>()`.
- `static`: registrations are declared with `bindings<bind<...>>`.
- `mixed`: `container<bindings<...>>` combines static bindings with runtime
  registrations.

### Scope

- `external`: the stored value is supplied by the caller.
- `unique`: each resolution creates a new value.
- `shared`: resolution reuses the same stored value.
- `shared_cyclical`: shared resolution with two-phase cycle support.

### Stored Type

- `T`
- `T*`
- `T&`
- `std::unique_ptr<T>`
- `std::shared_ptr<T>`
- `std::optional<T>`
- user-defined shared, unique, and optional-like wrappers
- `T[N]`
- `T[M][N]`
- `std::unique_ptr<T[]>`
- `std::shared_ptr<T[]>`
- `std::variant<A, B>`
- nested smart-pointer, variant, and array wrapper combinations
- local-binding target types
- factory-override target types
- cycle-shaped shared types
- interface implementation types

### Exposed Type

- concrete only
- mixed static graph with a runtime external dependency
- one interface
- multiple interfaces
- copyable interface
- user-defined wrapper concrete and interface bindings
- keyed concrete
- keyed interface
- annotated concrete
- annotated interface
- collection of interfaces
- keyed collection of interfaces
- runtime-key lookup interface
- local binding target
- local binding override target
- local and host collection target
- factory override target
- cycle-shaped concrete target
- unique interface target

### Resolved Type

- `T`
- `T&`
- `T*`
- `const T&`
- `const T*`
- `I`
- `I&`
- `I*`
- `std::unique_ptr<T>`
- `std::unique_ptr<I>`
- `std::shared_ptr<T>`
- `std::shared_ptr<T>&`
- `std::shared_ptr<I>`
- `std::shared_ptr<I>&`
- user-defined wrapper values, references, and interface conversions
- `std::vector<std::shared_ptr<I>>`
- custom inserted collection forms such as `std::map<int, std::shared_ptr<I>>`
- typed-key `T`, `T&`, and collection requests through `resolve(...,
  key<Key>{})`
- `dependency<T, key<Key>>` constructor and invocation dependencies
- `dependency<std::vector<std::shared_ptr<I>>, key<Key>>` constructor and
  invocation dependencies
- runtime-key requests
- `T (*)[N]`
- `T (&)[M][N]`
- `std::unique_ptr<T[]>`
- `std::shared_ptr<T[]>`
- `std::variant<A, B>`
- held variant alternative as value, reference, and pointer
- nested smart-pointer, variant, and array wrapper requests
- annotated concrete and interface requests
- local binding, factory override, mixed external dependency, and cycle-shaped
  concrete requests

### Container

- `runtime_container<>`
- `container<>`
- `static_container<bindings<...>>`
- `container<bindings<...>>` using only static bindings
- `container<bindings<...>>` using static and runtime registrations
- `runtime_container<indexed_traits<std::size_t>>`
- `container<indexed_traits<std::size_t>>`
- `runtime_container<indexed_traits<int>>`
- `container<indexed_traits<int>>`
- `runtime_container<indexed_traits<std::string>>`
- `container<indexed_traits<std::string>>`
- `runtime_container<indexed_dsl_traits>`
- `container<indexed_dsl_traits>`
- allocator-parameterized `container`
- custom-RTTI `container`

## Filters

The generator should form candidate rows from:

`feature x feature_case x registration_mode x scope x stored_type x exposed_type x resolved_type x container`

Then it should keep only rows that satisfy all rules below.

- The container must support the registration mode.
- Static-only containers cannot use runtime registration.
- Runtime-only containers cannot use static bindings.
- Mixed containers require at least one static binding and may add runtime
  bindings.
- Runtime-key lookup features require lookup container traits.
- Runtime-key registration is runtime-only unless static fixed-key bindings are added
  to the library.
- `external` scope requires caller-supplied storage.
- `unique` scope can resolve owning values and wrappers, but not stable shared
  references across resolutions.
- `shared` and `shared_cyclical` can resolve stable references and pointers.
- `shared_cyclical` rows require at least one cycle-shaped dependency graph.
- Interface resolution requires an exposed interface.
- Multiple-interface rows must verify at least two exposed interfaces from the
  same stored object.
- Collection rows require more than one matching binding.
- Keyed collection rows require more than one matching binding for the same key.
- Keyed singular rows require exactly one binding for the requested key.
- Annotated rows require matching `annotated<..., Tag>` request and registration.
- Array rows must preserve the exact array shape.
- Variant held-alternative rows require the held alternative to be unique in the
  variant type.
- Factory rows require a constructor shape that would otherwise need the explicit
  factory.
- Local-binding rows must verify local lookup, host fallback, and local override.
- Local-binding collection rows must verify local and host collection merge.
- Nested-container rows must verify child lookup, parent fallback, and child
  override.

## Coverage Expectations

The generator should fail if:

- a feature produces no rows
- an explicit feature case produces no rows
- a registration mode produces no rows
- a scope produces no rows
- a stored type category produces no rows
- a resolved type category produces no rows
- a container produces no rows
- an applicable feature / registration mode / container combination produces no
  rows
- a filter rule is declared but never exercised

Not every axis combination should exist. Completeness means every axis member and
filter rule is represented by at least one valid generated row, and each
applicable feature / registration mode / container combination has at least one
valid generated row.

## Generated Sources

Generated test executables are split by feature. Implementation and runner
sources are split by:

`feature x feature_case`

Features with only the default case keep a single source and runner named after
the feature. Dispatch-heavy features get one source and runner per case, for
example `invoke_member_function_pointer.cpp` and
`matrix_runner_invoke_member_function_pointer.cpp`. The split keeps
translation units tied to matrix meaning instead of arbitrary shard numbers,
while limiting the amount of template-heavy container and generated test code
compiled by one compiler process.
