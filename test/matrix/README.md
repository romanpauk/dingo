# Generated Matrix Test Model

Each matrix family declares independent axes, then filters or materializes their
Cartesian product into valid cases. The generator should not keep hand-written
recipes as the primary model. Recipes are an output of axis selection.

## Source Layout

- `axes/` is the Python package containing one declarative catalog per matrix
  dimension. `data.py` assembles and validates the registration catalogs;
  family-local generators validate their own catalogs before generation.
- `axes/dependency_forms.py` defines independent dependency carrier and
  decoration axes, derives their supported product, and declares exhaustive
  provisioning recipes used across matrix families.
- `schema.py`, `plugins.py`, `family.py`, and `generate.py` contain the typed
  model, registration/policy helpers, shared family rendering lifecycle, and
  generator CLI respectively.
- `common/` contains the assertion mechanism and other matrix-wide plumbing.
- `containers/` contains container types and traits selected by the container
  axis.
- `fixtures/` contains passive values and dependency shapes used by generated
  rows.
- `policies/` contains generic operations parameterized by a generated row.
- `scenarios/` contains complete behavioral and regression scenarios. Larger
  families such as parent-container and runtime regressions have one header per
  generated shard and share only their fixture definitions.

The registration family uses the application-wide registration and resolution
axes below. Invocation has callable, dependency-provisioning,
registration-mode, and container axes. Complete behavioral scenarios use their
own scenario and container axes. Constructor detection is a separate family
with backend, detection mode, and constructor-shape axes, plus an
argument-storage and conversion-category sub-matrix. Family-local axes do not
add irrelevant dimensions to other families.

Dependency request forms are the supported
`shape x carrier x decoration` product. Shapes own the C++ type transformation,
carriers select value or reference categories, and decorations wrap that result
as plain, selected, or annotated requests. Each decoration explicitly lists the
shape/carrier cells and test families for which it is supported. Constructor
detection and templated invocation cases are derived from this product, while
resolved types and provisioning recipes declare the same form identities. The
shared contract rejects missing, extra, renamed, or duplicate product cells and
requires every form in each applicable family.
Family-local compatibility still applies after that selection; for example,
annotated invocation excludes hybrid containers whose compile-time binding path
cannot materialize the wrapper safely.

Recursive dependency compositions are a separate shared type axis. Constructor
detection projects each type into a one-argument constructor shape. Resolution
and invocation model the full product of the same type with their container,
scope, and request-strategy axes. The generated coverage report keeps every
supported and unsupported cell explicit. C++ generation projects supported
cells into a smaller witness set: the `full` profile executes every supported
composition for resolve and invoke, and the `portable` profile executes every
structural category. Both profiles additionally cover every feasible outer
operator, container, scope, request-strategy, copyability, and movability
combination for both operations. Unsupported cells are reported and contract
tested. Exact stored wrappers use the request category selected by the scope
rule, while a top-level raw pointer registers its leaf and requests the pointer
value. This keeps carrier semantics out of this axis while still exercising
both exact-wrapper resolution and the supported pointer conversion.

Select the execution profile with `-DDINGO_MATRIX_PROFILE=full` or
`-DDINGO_MATRIX_PROFILE=portable`. Local development defaults to `full`; CI
runs it on one primary compiler and uses `portable` for portability jobs.

Shared-cyclical graphs use a dedicated compositional family. It crosses
container implementation, registration mode, the value or `shared_ptr`
representation of each node, and the dependency edge used in each direction.
The edge catalog covers references, pointers, and all three `shared_ptr`
request forms exposed by cyclical storage. Unsupported container/mode and
storage/edge cells remain generated skips with an explicit reason.

Dependency provisionings are exhaustive registration recipes: scope, stored
type, exposed type, exact form/resolved-type/feature cases, and registration
modes. Invocation consumes every compatible recipe through templated request
policies. The registration family must produce a row for each
`form x provisioning x resolved type x mode x container` combination, so an
unrelated feature or another container cannot mask missing shared, unique,
external, annotated, or keyed resolution coverage.

All registration families lower their selected axes into the same generic
registration recipe. Each registration entry independently declares whether it
participates in static, runtime, and mixed generation. The shared renderer then
selects static bindings and runtime setup from the requested container mode.
Recursive dependency compositions use this path with arbitrary rendered C++
types; mixed rows add a static anchor through the recipe instead of constructing
container setup separately.

Every family owns its row model and semantic sharding, but exposes the same
generation contract. It produces source-shard descriptions, and `family.py`
renders each description into a stable implementation/runner pair before
grouping the files by executable. This keeps validation, file writing, and
executable assembly consistent without forcing unrelated families into one row
schema or C++ template.

Axis members declare their required C++ includes through `headers`. Scenario
headers belong to the feature or feature case that executes them; they are not
global support code.

## Axes

### Matrix Family

- `registration`: container registration, resolution, construction, and
  lifetime coverage.
- `invoke`: callable dispatch across shared dependency provisionings and
  containers.
- `scenario`: complete regression and parent-container behaviors across the
  container implementations to which each scenario applies.
- `constructor_detection`: constructor backend and metadata recovery coverage
  across constructor shapes, plus constructor-probe argument conversion
  selection.
- `dependency_composition_resolve` and `dependency_composition_invoke`: the
  shared recursive dependency type axis across representative registration
  modes and storage scopes.
- `shared_cyclical`: real two-node cycles across independent container,
  registration-mode, node-storage, and directed dependency-edge axes.

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
- `construct_collection`: construct a collection with default or custom
  insertion.
- `local_bindings`: resolve dependencies from bindings attached to a
  registration.
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
- `custom_allocator`: use a caller-selected allocator for container bookkeeping.
- `custom_rtti`: use a caller-selected RTTI provider for runtime lookup.
- `explicit_dependencies`: verify explicit constructor dependency metadata in
  representative runtime, static, and mixed containers.

Generic runtime regression scenarios are intentionally limited to
`runtime_container<>` and `container<>`. Indexed, allocator, and custom-RTTI
variants run only scenarios whose behavior depends on those configurations.
This keeps regression coverage representative without multiplying unrelated
template instantiations.

Negative compile-time checks stay in `test/lit`. The generated matrix covers
valid behavior combinations.

### Feature Case

Most features use a single default case. Dispatch-heavy registration features
use this axis to describe the behavior under test without hiding it in
resolved-type checks.

- `factory_override`: no-argument function factories, function factories with
  dependencies, explicit constructor factories, detected constructors,
  `DINGO_CONSTRUCTOR` typedef constructors, callable registration factories, and
  explicit callable construction.

Factory case entries own the registration factory expression for the behavior
under test. The stored, exposed, and resolved type axes only select the C++
fixture type needed by that case.

### Invoke Callable

The invoke family covers inferred lambdas, explicit signatures,
`std::function`, function pointers, static and non-static member functions,
mutable and generic lambdas, ref-qualified and `noexcept` functors, move-only
functors, multi-argument callables, keyed value and collection dependencies,
and `std::move_only_function` when available.

### Invoke Dependency Shape

Invocation selects compatible registration shapes independently from the
callable: shared and unique values; external values, references, pointers,
stored pointers, stored references, and shared pointers; typed-key values; and
typed-key collections. Plain dependency carriers use one templated request
policy. Each provisioning reuses the registration family's typed
registration-plan builder and declares its supported registration modes.

### Scenario

The scenario family contains complete behaviors that own their registration
recipes instead of borrowing stored, exposed, and resolved type members:

- runtime container, construction, exception, reference, and cycle regressions
- indexed registration and static indexed regressions
- unkeyed, typed-key, and runtime-key lookup routing
- cross-parent behavior and static-parent combinations

### Scenario Container

- `standalone`: the scenario creates every required container itself.
- `runtime_container` and `container_runtime`: the two dynamic container
  implementations used by general runtime regressions.
- runtime and general indexed containers for `std::size_t`, `int`,
  `std::string`, and DSL-defined indexes.

Each scenario declares the subset it supports. Parent scenarios are standalone
because their C++ scenario functions construct and compare multiple parent and
child container families in one behavioral case.

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

`shared_cyclical` is consumed by its dedicated graph family rather than the
flat registration matrix because a valid row requires two registrations and
two directed dependency edges.

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
- optional and variant compositions containing shared, unique, move-only, and
  copy-only values
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
- typed-key `T`, `T&`, and collection requests through
  `resolve(..., key_type<Key>{})`
- `dependency<T, key_type<Key>>` constructor and invocation dependencies
- `dependency<std::vector<std::shared_ptr<I>>, key_type<Key>>` constructor and
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

### Constructor Detection Backend

- `portable`: the generic detector used by GCC and Clang. This backend is not
  instantiated when the test suite itself is compiled by MSVC.
- `msvc`: the category-based MSVC detector. It is also instantiated by GCC and
  Clang so both algorithms are covered on those hosts.

### Constructor Detection Mode

- `shape`: reports constructor kind and arity without retaining argument types.
- `signature`: reports kind and arity and recovers the selected argument types.

The constructor-shape axis covers values, pointers, lvalue and
rvalue references, copy-only and move-only values, selected and annotated
dependencies, incomplete references, aggregates, default construction, generic
and ambiguous constructors, explicit constructors, initializer lists, and
same-arity overloads.

Defaulted, generic, ambiguous, initializer-list, and same-arity-overload shapes
exercise detector behavior but do not describe independently resolvable
dependency forms. They are marked `detector_only`; all other constructor shapes
must declare at least one shared dependency form.

The constructor argument-conversion sub-matrix crosses unique and shared
argument storage with value, lvalue-reference, const-lvalue-reference,
rvalue-reference, and pointer conversion categories. The pointer category only
applies to unique storage, matching the supported constructor-probe shapes.
Each conversion category references the same shared dependency-form catalog.

## Filters

The registration family forms candidate rows from:

`feature x feature_case x registration_mode x scope x stored_type x exposed_type x resolved_type x container`

It then keeps only rows that satisfy all rules below. The scenario family uses:

`scenario x supported_scenario_container`

The constructor-detection family is the unfiltered product:

`backend x detection_mode x constructor_shape`

Known unsupported backend/mode/shape cells remain in that product and generate
skipped tests with their limitation reason. Dependency shapes own limitations
that apply to their carrier/decorator combinations, so optional and variant
value recovery are explicit without disabling supported reference forms.
Representative nested standard and user-defined forwarding wrappers document
the outer-wrapper recovery limitation. An unconstrained forwarding wrapper also
records the separate shape-detection limitation caused by accepting the opaque
probe.

Standard wrapper combinations use a recursive composition model with regular,
move-only, and copy-only leaves. Its operators mirror every non-identity
dependency shape: pointer, const-pointer, shared-pointer, unique-pointer,
optional, array, and variant. The model has an explicit maximum depth of two. It
generates every unary application and every distinct canonical variant pair at
each level, then renders the C++ dependency type once. Constructor detection,
resolution, and invocation consume that same catalog. The latter two form the
full product:

`composition x operation x container x scope x compatible request_strategy`

The container axis reuses the core runtime, static, generic runtime/static, and
mixed containers. The scope axis covers shared, unique, and external storage.
Shared and external storage use a stable request; unique storage crosses value
and rvalue requests. Static-only external cells remain in the product as explicit
skips because external storage requires runtime registration. Mixed cells use a
static anchor and register the composition at runtime, so they exercise both
halves of the mixed container. The contract rejects missing product cells, so
adding a leaf, operator, container, scope, or request strategy expands resolution
and invocation without requiring hand-written wrapper cases.

Every product cell is generated. Supported cells compile and execute; unsupported
cells remain visible as skipped tests with a reason. Shared storage requires a
movable composition. The current container also cannot publish an exact composed
type when a raw or smart pointer is the outer wrapper around another wrapper, or
when pointer-to-const normalization occurs inside a wrapper. These limitations do
not disable supported stable inverse compositions such as
`optional<shared_ptr<T>>` and `optional<unique_ptr<T>>`, or exact owning
`variant<shared_ptr<T>, unique_ptr<T>>` requests. Owning optional requests with a
composed operand and owning arrays of optionals remain explicit unsupported
cells because resolution selects an inner conversion instead of materializing
the exact outer type.

Operators declare positional resolution limitations alongside their type and
copy/move rules. A limitation may select request strategies and operand operator
categories; scope rules declare materialization and registration requirements.
The row generator evaluates this metadata without procedural wrapper-specific
branches.

The complete product also records shape-detection limitations for optional
values containing pointers or copy-only array/variant composites. These cells
remain in the matrix as skips with their reason, like signature-recovery
limitations.

Its argument-conversion sub-matrix uses the supported combinations of:

`argument_storage x conversion_category`

- The container must support the registration mode.
- Static-only containers cannot use runtime registration.
- Runtime-only containers cannot use static bindings.
- Mixed containers require at least one static binding and may add runtime
  bindings.
- Runtime-key lookup features require lookup container traits.
- Runtime-key registration is runtime-only unless static fixed-key bindings are
  added to the library.
- `external` scope requires caller-supplied storage.
- `unique` scope can resolve owning values and wrappers, but not stable shared
  references across resolutions.
- `shared` can resolve stable references and pointers.
- `shared_cyclical` graph rows require two cyclical bindings; value storage
  exposes reference and pointer edges, while `shared_ptr` storage additionally
  exposes `shared_ptr` value, reference, and pointer edges.
- Interface resolution requires an exposed interface.
- Multiple-interface rows must verify at least two exposed interfaces from the
  same stored object.
- Runtime multi-interface rows must also verify that interface `shared_ptr`
  conversions share ownership. Static bindings currently cover reference
  identity only because their conversion cache does not expose those handles.
- Collection rows require more than one matching binding.
- Keyed collection rows require more than one matching binding for the same key.
- Keyed singular rows require exactly one binding for the requested key.
- Annotated rows require matching `annotated<..., Tag>` request and
  registration.
- Array rows must preserve the exact array shape.
- Variant held-alternative rows require the held alternative to be unique in the
  variant type.
- Factory rows require a constructor shape that would otherwise need the
  explicit factory.
- Local-binding rows must verify local lookup, host fallback, and local
  override.
- Local-binding collection rows must verify local and host collection merge.
- Nested-container rows must verify child lookup, parent fallback, and child
  override.
- Parent regression scenarios must verify recursion detection within each
  runtime container family and across every family pairing.

String-literal injection into a runtime string index remains a future feature.
The enabled external fixed-string test covers explicit external-key adaptation,
but the disabled literal-key test is not considered matrix coverage.

## Coverage Expectations

The generator should fail if:

- a feature produces no rows
- an explicit feature case produces no rows
- a registration mode produces no rows
- a scope produces no rows
- a stored type configuration ID produces no rows
- a resolved type category produces no rows
- a container produces no rows
- an applicable feature / registration mode / container combination produces no
  rows
- a filter rule is declared but never exercised
- a declared feature, feature-case, resolved-type, or lifetime policy is never
  selected
- a scenario or scenario container has no supported row
- a scenario or scenario-container axis contains duplicate identities
- a shared dependency form lacks constructor-detection, resolution, or invoke
  coverage required by its catalog entry
- a dependency provisioning does not map to compatible registration metadata
- a declared dependency form / provisioning / resolved type / mode / container
  combination has no resolution row
- an invoke callable requests a form unsupported by one of its provisionings
- an invoke callable, provisioning, mode, or container has no compatible row
- an invoke callable, provisioning, mode, or container axis contains
  duplicate identities
- a constructor backend / detection mode shard does not contain every
  constructor shape
- a constructor argument storage or conversion category has no compatible row
- two matrix families claim the same generated source path

Not every axis combination should exist. Completeness means every axis member
and filter rule is represented by at least one valid generated row, and each
applicable feature / registration mode / container combination has at least one
valid generated row.

## Generated Sources

Registration test executables are split by feature. Implementation and runner
sources are split by:

`feature x feature_case`

Features with only the default case keep a single source and runner named after
the feature. Dispatch-heavy features get one source and runner per case. The
split keeps translation units tied to matrix meaning instead of arbitrary shard
numbers, while limiting the amount of template-heavy container and generated
test code compiled by one compiler process.

Invocation has one executable and one source/runner pair per callable. Its rows
are the filtered product:

`callable x dependency_provisioning x registration_mode x container`

Constructor detection has one executable and four semantic source shards split
by `backend x detection_mode`. Every shard contains the complete
constructor-shape axis. A fifth shard contains the supported argument-storage
and conversion-category combinations.

Dependency composition has one executable per operation and shards by outer
operator and container. Generation also writes
`build/test/generated/matrix/dependency-composition-coverage.md`. The report
records generated, supported, and skipped cells by operation, operator,
container, scope, and request strategy, followed by the unsupported-reason
totals. These aggregates are contract-tested against the row catalog.

Behavioral scenarios retain executables named for their suites and are sharded
by scenario. A suite may combine registration and scenario sources; for
example, `nested_container` keeps native parent resolution in the registration
family and adds the standalone cross-parent scenario shards to the same test
executable.

Python selects, validates, names, and shards valid rows. Generic row behavior
lives in typed C++ policies, while complete behavioral and regression cases live
in scenario headers. Policy headers are split into core, resolution, collection,
aggregate, lifetime, and special-purpose families; each generated feature shard
includes only its selected policy and scenario headers.

The Python generator has a pytest suite, also registered with CTest, that checks
its row-count budget, axis and policy coverage, registration placement,
rendering stability, and stale generated-output cleanup. Run it with:

```bash
uv run --locked pytest -q test/matrix/generator_test.py
ctest --test-dir build -R dingo_matrix_generator_test --output-on-failure
```
