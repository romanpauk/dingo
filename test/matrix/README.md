# Generated Matrix Test Model

The matrix is described as independent axes, then filtered into valid cases.
The generator should not keep hand-written recipes as the primary model. Recipes
are an output of axis selection.

## Axes

### Feature

- `resolve_concrete`: resolve the registered concrete type.
- `resolve_interface`: resolve through `interfaces<...>`.
- `resolve_wrapper`: resolve an owning or sharing wrapper directly.
- `resolve_collection`: resolve all unkeyed bindings for an interface.
- `resolve_keyed`: resolve one keyed binding.
- `resolve_keyed_collection`: resolve all bindings for a key.
- `resolve_indexed`: resolve one indexed runtime binding.
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
- `custom_allocator`: use a caller-selected allocator for container
  bookkeeping.
- `custom_rtti`: use a caller-selected RTTI provider for runtime lookup.

Negative compile-time checks stay in `test/lit`. The generated matrix covers
valid behavior combinations.

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
- `T[N]`
- `T[M][N]`
- `std::unique_ptr<T[]>`
- `std::shared_ptr<T[]>`
- `std::variant<A, B>`

### Exposed Type

- concrete only
- one interface
- multiple interfaces
- keyed concrete
- keyed interface
- annotated concrete
- annotated interface
- collection of interfaces
- indexed interface

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
- `std::vector<std::shared_ptr<I>>`
- `keyed<T, Key>`
- `keyed<T&, Key>`
- `keyed<std::vector<std::shared_ptr<I>>, Key>`
- `T (*)[N]`
- `T (&)[M][N]`
- `std::unique_ptr<T[]>`
- `std::shared_ptr<T[]>`
- `std::variant<A, B>`
- held variant alternative as value, reference, and pointer

### Container

- `runtime_container<>`
- `container<>`
- `static_container<bindings<...>>`
- `container<bindings<...>>` using only static bindings
- `container<bindings<...>>` using static and runtime registrations
- `runtime_container<indexed_traits<map>>`
- `container<indexed_traits<map>>`
- `runtime_container<indexed_traits<unordered_map>>`
- `container<indexed_traits<unordered_map>>`
- `runtime_container<indexed_traits<array>>`
- `container<indexed_traits<array>>`
- allocator-parameterized `container`
- custom-RTTI `container`

## Filters

The generator should form candidate rows from:

`feature x registration_mode x scope x stored_type x exposed_type x resolved_type x container`

Then it should keep only rows that satisfy all rules below.

- The container must support the registration mode.
- Static-only containers cannot use runtime registration.
- Runtime-only containers cannot use static bindings.
- Mixed containers require at least one static binding and may add runtime
  bindings.
- Indexed features require indexed container traits.
- Indexed registration is runtime-only unless static indexed bindings are added
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
- a registration mode produces no rows
- a scope produces no rows
- a stored type category produces no rows
- a resolved type category produces no rows
- a container produces no rows
- a filter rule is declared but never exercised

Not every axis combination should exist. Completeness means every axis member and
filter rule is represented by at least one valid generated row, and broad
features such as concrete/interface resolution cover the common product of
scope, stored type, resolved type, registration mode, and container.
