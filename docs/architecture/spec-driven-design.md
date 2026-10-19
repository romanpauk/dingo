# Spec-Driven Design

This page describes the target architecture in which Dingo's semantics are
written once in a language-agnostic specification, and the runtime, static, and
mixed containers are equivalent by construction. It also tracks the
implementation of that architecture. The other pages in this section describe
the code as it is today.

Everything below the public API may be replaced completely. The current
implementation is the behavioral baseline and a source of lessons, not a
starting point the new implementation has to evolve from.

## Goals

- Runtime, static, and mixed containers accept the same programs and behave the
  same way. Equivalence follows from the structure of the code, not from tests
  alone.
- Storages, lookups, and request handling are unified the same way: one
  construct per concept, with the differences expressed as data.
- One specification is the source of truth for the library semantics: shapes,
  scopes, routes, requests, bindings, selection, graph rules, and diagnostics.
- Every spec decision lives in generated code. Hand-written code is limited to
  primitives that make no spec decisions, so it has nothing that can drift.
- Every public operation and component has contracts on compile cost, object
  size, runtime cost, code size, inlining, and header cost. The contracts are
  declared in the spec and enforced whether the code is generated or
  hand-written.
- Each C++ standard's features and each compiler's requirements are handled
  without changing which programs compile or how they behave.
- The public API is defined in the spec and preserved: include paths, names,
  extension points, and configuration macros.
- No behavior changes unintentionally. The new implementation is tested
  differentially against a pinned baseline of the current library, and every
  intended change is declared and justified.

## Non-Goals

- Generating a runtime library and a static library separately from the spec.
  That moves the equivalence problem into the generator.
- Using the generator to multiply variants. Generation serves unification, see
  [Unification Rules](#unification-rules).
- Putting C++ text into the spec. The spec stays language-agnostic, and C++
  lives in the generator's templates as the C++ projection.
- Preserving internal headers, names, directory layout, or structure.

## Current State

This section records what the new implementation must stay compatible with and
what it should not repeat.

All types are known statically in both domains. `register_type<T>()` and
`resolve<T>()` are templates in the runtime container too. The domains differ
only in when the binding set is closed and when runtime key values arrive. The
code nevertheless implements most decisions twice.

Shared today:

- registration normalization in
  [binding_model.h](../../include/dingo/binding/binding_model.h)
- the storage classes and `storage_materialization_traits`
- the conversion operations in
  [resolution_operation.h](../../include/dingo/resolution/resolution_operation.h)

Implemented twice:

- Request-to-route matching: `is_resolution_request_v` in
  [resolution.h](../../include/dingo/resolution/resolution.h) for static
  requests, `matches_resolution_request` over `type_descriptor` in
  [binding_resolution.h](../../include/dingo/binding/binding_resolution.h) for
  runtime requests.
- Lookup: type-list filtering and an overload-set index for static bindings; the
  base index and the array, ordered, and unordered backends under
  `include/dingo/lookup/` for runtime bindings.
- Singular ambiguity: counting over type lists in the static registry,
  `equal_range` over lookup indexes in the runtime registry.
- Cross-source conflicts, local-binding precedence, parent fallback, collection
  merging, and auto-construction on a miss, hand-coded separately in
  [container.h](../../include/dingo/container.h),
  [static_container.h](../../include/dingo/static_container.h),
  [registry.h](../../include/dingo/runtime/registry.h), and
  [activation_set.h](../../include/dingo/static/activation_set.h).
- Route lists: static value requests use separate value routes, while the
  runtime list publishes one shared address route.
- Conversion-cache types: per interface at runtime, per registration statically.
- Cycle detection: a compile-time DFS in
  [graph.h](../../include/dingo/static/graph.h), and `recursion_guard` keyed by
  binding address at runtime.

Shared helpers that would remove some of this duplication exist but are unused:
`construct_binding_collection` and `count_binding_collection` in
[binding_collection.h](../../include/dingo/binding/binding_collection.h),
`make_runtime_selection`, and the `prefer_primary` resolution policy.

Behavioral asymmetries:

- External scope is runtime-only, because static storage must be default
  constructible.
- Duplicate registrations throw at runtime registration when the lookup
  cardinality is `one`. Static bindings accept duplicates and fail only on a
  singular request.
- An empty collection is a `static_assert` statically and an exception at
  runtime, unless an explicit collection lookup is declared.
- `static_container` does not auto-construct on a miss. `container<bindings<>>`
  does.
- The same static ambiguity is a compile error in `static_container` and an
  exception in `container<bindings<>>`.
- Fixed `key_type<K, V>` keys are supported only by `static_container` with
  declared associative lookups.
- Runtime containers are movable. Static containers are not.

Compiler-specific code: `_MSC_VER` selects between implementations in
constructor detection, in `constructor_traits<std::array>`, and in a few storage
and container internals. Other `_MSC_VER` uses are warning pragmas and
`__declspec` annotations. The constructor-detection matrix already compiles and
tests the MSVC detection backend on Linux. The other implementation branches are
tested only by MSVC builds.

The [generated matrix](../../test/matrix/README.md) keeps its own hand-curated
approximation of the conversion table: capability tags such as
`stable_concrete_storage`, `supported_modes` sets, and per-provisioning case
tuples. It can drift from
[storage_scope_policy.h](../../include/dingo/storage/storage_scope_policy.h).
Identity and behavior expectations live in C++ policies, not in the model.

## One Semantics, Three Binding Times

Every part of the library belongs to one of three layers.

Facts are computed per binding from its types: interfaces, key, scope, the
routes it can serve, and its declared dependencies. They are template code run
at registration and are identical in both domains, because they are the same
template.

Decisions are functions of a binding table and a request: selection, ambiguity,
precedence, collection merging, graph validation, and the route that serves a
request. They are written once as `constexpr` functions over plain descriptors,
in one engine.

- Static: the table is `constexpr` and the engine runs at compile time.
- Runtime: the table lives in the container arena and the engine runs at
  runtime. Results are memoized per request type.
- Mixed: the static part of the table is evaluated at compile time and passed as
  a constant input. The runtime part is merged by the same function. This is
  partial evaluation of one function.

Execution is the typed kernels: construction, storage, materialization, and
conversion. Generated code composes them from hand-written primitives. The
static backend calls a kernel directly. The runtime backend calls the same
instantiation through a thunk.

Both backends are kept free of decisions:

```text
static:  constexpr table -> engine::select -> index -> type_list_at<I> -> kernel
runtime: arena table     -> engine::select -> index -> thunks[I]       -> kernel
```

A backend that makes no decision cannot diverge from the other one.

## Unification Rules

The generator is a tool for unification. If two pieces of generated code differ
only in data, they are one C++ construct consuming different data.

### Domains

Static, runtime, and mixed containers share the engine, the descriptors, and the
kernels. They differ only in where the binding table lives and how the selected
index is dispatched.

### Storages

There is one storage construct. A scope is a descriptor: lifetime slot (none,
lazy, two-phase, or external reference), materialized category, stability,
source access, and phases. The storage selects its slot primitive and its
materialization from the descriptor. The generator emits the scope descriptors
and tag types, not one storage class per scope. A new scope is a spec entry and
needs new C++ only if it needs a new slot primitive.

### Lookups

There is one lookup semantics: a filter over the binding table on interface
identity, key kind, and key value. Static and runtime containers run the same
filter. The binding time of a key value is independent of the binding time of
the table, so none, typed, fixed, and runtime keys go through the same filter in
every container.

The index structures, the base index and the dense array, ordered, and unordered
backends, are accelerations chosen by key kind and lookup declaration. Each is
declared in the spec with the contract that it returns what the filter returns,
and differential tests check each one against a linear scan.

### Requests

Requests have one normalization and one matcher over descriptors.

### Variant Points

Separate code is allowed only at a declared variant point:

- binding-time dispatch: static index dispatch or a runtime thunk
- a [language tier](#language-tiers)
- a [compiler variant](#compiler-variants)
- an index structure, as described under [Lookups](#lookups)

Each variant point is declared in the spec with a justification. The generator
emits no code variant that the spec does not declare, and the list of variant
points is reviewed like an API.

## Specification

The spec lives in a `spec/` directory at the repository root as a typed Python
package that exports JSON. The test matrix already uses Python, and the format
should stay data until the rules stabilize. A dedicated rule language can come
later if the data form proves insufficient.

The content is language-agnostic: shapes are abstract constructors, and a C++
projection maps them to C++ spellings.

### Public API

The public API is the only part of the current library the new implementation
must keep. The C++ projection lists it explicitly:

- public include paths: the container headers, `container.h`,
  `runtime_container.h`, and `static_container.h`, and the headers users include
  for scopes and factories, such as `storage/shared.h`, `storage/unique.h`,
  `storage/external.h`, `storage/shared_cyclical.h`, `factory/constructor.h`,
  `factory/function.h`, and `factory/callable.h`
- public names and signatures: the containers, `bindings`, `bind`, `scope`,
  `storage`, `factory`, `interfaces`, keys, `annotated`, the registration and
  resolution functions, and the exception types
- the extension points users specialize, as documented in
  [Extending Dingo](extending-dingo.md), and `DINGO_CONSTRUCTOR`
- the configuration macros in [config.h](../../include/dingo/base/config.h)

Everything not listed is internal. A change to the public API is a behavior
change and is declared like one.

### Shape Algebra

A shape is a leaf wrapped in layers. Layer kinds are abstract classes of handles
defined by their traits, owning, sharing, nullable, and borrowable, such as a
sole-owner handle, a sharing handle, an optional value, an alternative, a
pointer, a reference, and an array. Leaf extraction and rebinding are defined
generically on the algebra.

The spec does not name concrete wrapper types. Users extend the library with
their own sole-owner, sharing, and optional handles through the wrapper-traits
extension point, so a concrete wrapper is mapped to a layer kind by that
extension point in the C++ library, not by the spec. The standard library
wrappers, `std::unique_ptr`, `std::shared_ptr`, and `std::optional`, are the
built-in uses of the same extension point and get no special treatment.

### Scopes

```yaml
shared:          { slot: lazy,         materializes: lvalue, stable: true,  source_access: borrow }
unique:          { slot: none,         materializes: rvalue, stable: false, source_access: consume }
external:        { slot: external_ref, materializes: lvalue, stable: true,  source_access: borrow, needs: instance }
shared_cyclical: { slot: two_phase,    extends: shared, phases: [publish, complete] }
```

### Routes

Route rules are the core of the spec. They replace the `storage_scope_policy`
cells and the matrix capability tags.

```yaml
identity:       { from: X,        to: X,         access: inherit }
address_of:     { from: lvalue X, to: Ptr X,     access: borrow, requires: stable }
deref:          { from: Ptr X,    to: route(X),  access: inherit }
wrapper_borrow: { from: W<X>,     to: route(X),  where: W.borrowable, access: inherit }
copy_out:       { from: X,        to: Val X,     requires: copyable(X), access: borrow }
retain:         { from: S,        to: T,         via: convert, cache: T }
```

A route is a derivation in this rule system. Availability, the access
requirement, and cache and retention needs all come from the same derivation, as
in the [Conversion Model](conversion-model.md).

### Requests And Keys

Request forms are the product of shape, carrier, and decoration. Keyed,
annotated, selected, collection, and `exact_lookup` requests are decorations.
The spec defines normalization to a lookup key and the four key kinds: none,
typed, fixed, and runtime. A key value has its own binding time, independent of
the binding table's.

### Lookups

The lookup filter, and the index structures with the key kinds and lookup
declarations that select them. See [Lookups](#lookups).

### Bindings And Factories

A binding has interfaces, a key, a scope, a stored type, a factory kind, local
bindings, and a dependency visibility: declared or detected.

### Selection

```text
singular(req) = first non-empty of:
    local.static ∪ local.runtime
    container.static ∪ container.runtime
    parent.singular(req)
    auto_construct(req)            # if constructible
  then: |c| == 1 -> ok, |c| > 1 -> ambiguous, empty -> not_found

collection(req) = concat(local, container, merge_parent ? parent : empty)
  empty -> as declared by the lookup
```

### Graph Rules

A cycle is legal if and only if every node in it is `shared_cyclical`. Each
check runs when its edges become known: declared dependencies at container type
formation, detected dependencies during resolution.

### Diagnostics

Each diagnostic has an id, a condition, a C++ exception type, and a message
template:

```yaml
ambiguous_binding:
  message: "request {request} matches {count} bindings: {candidates}"
  exception: type_ambiguous_exception
  fallback_carrier: dingo_error_ambiguous_binding<Request, Candidates...>
```

Whether a diagnostic is a compile error is derived, not chosen per container. A
diagnostic is reported at compile time if and only if no runtime extension of
the sources that are still open can change the outcome.

- A singular ambiguity among static bindings in the root scope is final, because
  runtime registrations only add candidates. It is a compile error in
  `container<bindings<>>` as well as in `static_container`.
- A missing binding in a mixed container remains a runtime error, because a
  runtime registration may still provide it.

### Binding-Time Constraints

A feature may be missing from one domain only through a declared binding-time
constraint with a justification, such as "needs a value that is only known at
runtime". An undeclared domain gap is a bug in the implementation, not a
filtered combination.

### Variant Points

The declared variant points with their justification. See
[Variant Points](#variant-points).

### Language Capabilities And Compiler Variants

The language capabilities the library uses, how each one is detected, the
compiler variants, and which construct has an implementation per capability or
variant. See
[Language Tiers And Compiler Variants](#language-tiers-and-compiler-variants).

### Contracts

Contracts are part of the spec, next to the semantics. They bind every public
operation and every component, including the hand-written facade and primitives:

- compile cost: the instantiations an operation may produce and how they grow
- object size: the size of a container and what each binding adds to it
- runtime cost: indirect calls, lookups, and allocations on the resolution path
- code size: the size of the code a resolution compiles to
- inlining: which functions are inlined into their caller, which stay out of
  line, and which are cold
- header cost: the preprocessed size and compile time of each public header

```yaml
contracts:
  static_container:
    object_size: sum of binding slots, nothing for unused features
  container<>:
    object_size: fixed, independent of the number of registrations
  register_type<T>:
    compile: binding facts and binding-side thunks only, no request kernels
    inline: never
  resolve<T>:
    compile: selection_v<T> and one kernel per used route
    runtime.static: no indirect calls, no allocations
    runtime.runtime: one memoized lookup, one indirect call, no allocations
      after the first resolution
    inline.static: always
    inline.runtime.memoized: always
    inline.runtime.first_resolution: never
    code_size: bounded per probe and compiler
  diagnostics.throw:
    inline: never, cold
```

Limits are recorded from the current code first and then tightened. Per-compiler
limits stay possible, as in the existing
[code size probes](../development.md#code-size-tests). The generator emits the
inlining annotations through the existing `DINGO_ALWAYS_INLINE` and
`DINGO_NOINLINE` macros, and hand-written code uses the same macros as declared
by its contract. See [Contract Enforcement](#contract-enforcement).

### Behavior Changes

The intended differences from the baseline library, each with the affected
cases, the baseline and new outcomes, and the decision that justifies it. See
[Compatibility With The Current Library](#compatibility-with-the-current-library).

## Engine And Backends

Descriptors are plain structs: the existing `type_descriptor`, plus
`route_desc`, `binding_desc`, and `request_desc`. Type identity is the address
of a per-type tag. Tag addresses compare for equality in constant expressions
but cannot be ordered or hashed there, so the engine relies on equality only.
The static backend uses indices into the closed table. The runtime backend
hashes the address.

The engine is a set of `constexpr` functions over fixed-capacity array views,
generated as straight-line code from the selection and graph rules.
Straight-line code is cheaper to evaluate at compile time than a generic
interpreter over rule data. That works in C++17. C++20 adds transient
`constexpr` allocation and `consteval`. The engine returns a `selection` with a
status and the selected indices.

The static backend:

```c++
constexpr auto s = engine::select(static_table, request_desc<T>);
if constexpr (s.status == status::ok)
  return kernel<type_list_at_t<s.index, bindings>>(...);
else
  fail<s.status, T>();  // generated from the diagnostics catalog
```

The runtime backend calls the same `engine::select` over the arena table and
memoizes the result per request type in the existing type cache. The hot path
stays one indirect call through a thunk. The benchmarks and the
[code size probes](../development.md#code-size-tests) guard it.

The mixed backend passes the static candidates of a request as a constant input
and the runtime candidates as a dynamic input to the same function.

## Language Tiers And Compiler Variants

### Language Tiers

The library detects language capabilities, not standard versions. It replaces
`DINGO_CXX_STANDARD` with feature-test macros and builtins, as
[callable.h](../../include/dingo/factory/callable.h) already does for
`std::move_only_function`.

- User-generated `static_assert` messages (P2741), detected by
  `__cpp_static_assert >= 202306L`, replace fixed literal messages in which
  types are visible only in instantiation notes.
- Pack indexing `Ts...[I]`, detected by `__cpp_pack_indexing`, replaces the
  `__type_pack_element` builtin (Clang, GCC 14 and later), which replaces a
  constant-depth fallback.
- `= delete("reason")`, detected by `__cpp_deleted_function`, replaces silently
  deleted overloads.
- C++20 transient `constexpr` allocation, `consteval`, and concepts replace
  two-pass fixed-array sizing and SFINAE.
- C++23 `if consteval` replaces `std::is_constant_evaluated`.
- C++26 reflection (P2996) and expansion statements, as compilers ship them,
  replace constructor detection in `factory/detail/`,
  [type_name.h](../../include/dingo/base/type_name.h), tag-based type ids, and
  type-list algorithms. A `std::meta::info` is a value the engine can operate on
  in `consteval` code before splicing the result back into a type.

All conditional compilation lives in one mechanics header at layer 0. Every
header above it, generated or hand-written, calls tier-agnostic primitives:

```c++
// base/mechanics.h: the only file with tier conditionals
#if __cpp_pack_indexing >= 202311L
template <std::size_t I, class... Ts> using pack_at = Ts...[I];
#elif __has_builtin(__type_pack_element)
template <std::size_t I, class... Ts>
using pack_at = __type_pack_element<I, Ts...>;
#else
template <std::size_t I, class... Ts>
using pack_at = /* constant-depth index lookup */;
#endif

template <diag Code, class... Subjects> constexpr void fail() {
#if __cpp_static_assert >= 202306L
  static_assert(dependent_false<Subjects...>,
                diag_message<Code, Subjects...>());
#else
  static_assert(dependent_false<Subjects...>, diag_literal<Code>);
#endif
}
```

`diag_message` is a `constexpr` fixed-string builder that inserts type names. It
and `diag_literal` are generated from the diagnostics catalog, so the C++17 and
C++26 messages come from one entry.

The library keeps one header tree with capability gates rather than a generated
tree per standard. Dingo is header-only, and a program may compile translation
units with different `-std` flags. Tiered constructs therefore must not change
layouts or other ABI-visible types. They change only how a metafunction is
computed or how a diagnostic is reported. The object size contracts are checked
at every tier, which catches a tier that changes a layout.

Choosing a tier may change diagnostics and compile cost, never which programs
compile or how they behave. The conformance suite runs at every tier to show it,
and a tier-gated feature such as `std::move_only_function` support is the only
declared exception.

### Compiler Variants

The spec separates compiler-specific code into two kinds.

Implementation variants are alternative standard C++ implementations of one
primitive, such as the portable and MSVC constructor-detection backends or the
two placement constructions of `std::array`. Each variant is named in the spec.
The compiler selects the default variant, and a macro such as
`DINGO_VARIANT_CONSTRUCTOR_DETECTION=msvc` overrides it. Every variant that is
standard C++ is compiled and tested on every compiler that accepts it, so the
MSVC variants run in Linux CI. The constructor-detection matrix already does
this for its MSVC backend; the spec makes it the rule.

Compiler annotations are warning pragmas, `__declspec(empty_bases)`, and
`__forceinline`. They are emitted through mechanics macros and do not change
behavior. The object size contracts catch layout effects such as missing empty
base optimization.

Real MSVC builds stay covered by the Windows CI jobs and by the
[msvc-wine image](../../docker/ubuntu25-toolchains/README.md).

## Compile-Time Cost

These principles are written into the spec, ordered by expected payoff.

### Precompute The Closed Semantics

Route derivation for the built-in shapes is a finite, per-layer table: layer
kind, scope, and request layer kind to operation, access, and stability. The
generator evaluates the route rules offline and emits `constexpr` data. Nested
shapes are handled by a `constexpr` walk over a short array of layer kinds
instead of template recursion. Custom wrappers enter the table through their
declared traits. Custom `type_conversion_traits` remain a lazily instantiated
template escape hatch per source and target pair. Because the table is data, the
runtime backend matches against it directly instead of instantiating route
templates.

### Split Operations At The Erasure Boundary

Binding-side code depends only on binding types: the stored type to an object
address, and pointer adjustment per declared interface. Request-side code
depends only on the request type: binding a reference, copying, wrapping. The
binding and request cross product is instantiated only at static use sites, and
only for the pair that is used. The shared address route already follows this
rule.

### Convert Types To Values Once

A `descriptor_v<T>` variable template turns a type into a descriptor at the
boundary, and everything after that is a `constexpr` function. `constexpr`
evaluation is not memoized, so each decision a request needs is cached in a
variable template:

```c++
template <class T>
constexpr selection selection_v = engine::select(table, descriptor_v<T>);
```

### Keep Template Parameters Narrow

Each construct is parameterized by exactly the inputs it declares. Shape-level
computations take the shape with the leaf abstracted, as `lookup_type_t` already
does with `runtime_type`, so all `shared_ptr<X>` bindings share one shape
instantiation. Unlike the current
`storage<Tag, Registered, Stored, Factory, Conversions>`, route planning does
not depend on the factory.

### Check Eagerly On Descriptors, Instantiate Kernels Lazily

Cycles, duplicates, missing declared dependencies, and ambiguity are checked
when the container type is formed, using descriptors only. Factory calls,
storage materialization, and conversion `apply` are instantiated only when a
request reaches them. A binding that is registered but never resolved costs one
descriptor.

### Avoid Known-Expensive Patterns In Core

- recursive type-list algorithms
- `std::tuple` as the binding store
- `std::conditional_t` where both branches are instantiated
- SFINAE overload sets over all bindings
- `<memory>`, `<variant>`, and `<optional>` in core headers; wrapper support
  moves to opt-in headers listed by the shape projection

## Contract Enforcement

Contracts are checked on all code. Generated code follows the cost principles by
construction, but a wrong template produces violations everywhere, and the
hand-written primitives and facade can introduce them too.

### Instantiation Contracts

Each internal construct declares its inputs and how many instantiations it may
produce:

```yaml
cost:
  descriptor:     { inputs: [type],                    per: distinct type }
  route_plan:     { inputs: [scope, shape],            per: distinct shape }
  binding_facts:  { inputs: [binding],                 per: binding }
  selection:      { inputs: [request, table],          per: request type }
  kernel:         { inputs: [binding, route],          per: used route, lazy }
  static_kernel:  { inputs: [binding, route, request], per: used pair, static only }
```

The generator emits scaling probes per tier: synthetic programs with N bindings,
chain depth D, K distinct shapes, and R request types. They compile with Clang
`-ftime-trace`, through the same
[time-trace](../development.md#compile-time-profiling) workflow used for manual
investigations. A checker counts instantiations of each `dingo::` entity and
asserts the contract:

- `route_plan` instantiations equal the number of distinct shapes, not the
  number of bindings
- registering without resolving produces no `kernel` instantiations
- instantiation counts grow linearly in N and D
- a higher tier never instantiates more than a lower tier

Instantiation counts are deterministic and gate CI. Wall time is too noisy to
gate on and is tracked as a trend.

### Object Size

Generated probes check `sizeof` of containers against the object size contracts,
for growing numbers of bindings, for every tier, and on every target platform.

### Runtime Cost And Code Size

The existing code size probes, `check_codegen_probe_sizes.py` and
`check_codegen_probe_instructions.py`, become generated from the runtime cost
and code size contracts. Conformance tests use a counting allocator to check the
allocation contracts.

### Inlining

The generator emits a codegen probe for each inlining contract and checks the
object code on the compilers where it can be inspected:

- `always`: the probe function contains no call to a Dingo symbol
- `never`: the function exists as an out-of-line symbol
- `cold`: the function is placed in a cold section where the compiler supports
  it

### Header Cost

Each public header is compiled alone against a budget for preprocessed size and
compile time, building on the existing `includes-compile` target.

## Generated And Hand-Written Code

Code drifts from the spec when it encodes spec decisions. Generated code holds
every spec decision, and hand-written code holds none. Code whose content varies
with spec data is generated. Code that the spec could describe only by
containing C++ text is hand-written.

Generation guarantees consistency with the spec, not correctness. The generator
and its templates are hand-written too, so generated code is tested like any
other code. See [Testing And Conformance](#testing-and-conformance).

### Generated

- the spec vocabulary: scope, shape kind, key kind, route operation, and
  diagnostic enums, the descriptor structs, and the public tag types such as
  `scope<shared>` with their mapping to descriptors
- the per-layer route and scope tables as `constexpr` data
- the engine: selection and graph rules as straight-line `constexpr` code
- the one storage construct and the one lookup filter, each consuming
  descriptors, and the selection of index structures per key kind
- the backend glue: route operation to primitive dispatch, static index
  dispatch, and runtime thunk tables
- the tier and compiler-variant branches of the mechanics header
- the inlining annotations required by the contracts
- the diagnostics: `diag_message`, `diag_literal`, the carrier templates, and
  the exception mapping
- the documentation of conversion tables and diagnostics
- the conformance tests, the differential tests, the diagnostics tests per tier,
  and the contract probes

### Hand-Written

- lifetime slots: lazy, two-phase, and external reference
- factory invocation and constructor detection, with their compiler variants
- conversion primitives: address-of, dereference, wrapper get and borrow through
  user traits, copy, and move
- the index structures: the dense array, ordered, and unordered backends
- the arena, the allocators, and the shape of the type-erasure thunks
- the primary templates of the user extension points, `type_traits` and
  `type_conversion_traits`, because they are a C++ contract users specialize
- a thin public facade, `container`, `resolve<T>`, and `register_type`, which
  forwards to generated code; it is bound by the [contracts](#contracts) like
  everything else

The primitives sit in a header layer below the generated code and must not
include it, so they cannot know scope names, shape kinds, rule order, or
diagnostic text. `layers-check` enforces this.

### Rules For Generated Code

- Logic lives in Python and the Jinja templates contain no logic. Python
  computes the data, and the templates only lay it out. Python never contains
  output text: every line of a generated file, including its marker, its license
  block, macro names, and conditions, comes from a template under
  `tools/generator/templates/`, and so does the manifest.
- Generate data plus small generic C++ rather than near-identical copies of
  code. Every translation unit that includes a generated header parses it, so
  one specialization per cell costs more than one table and one consumer. The
  header cost contracts keep this in check.
- Generated code reads like hand-written code. It is formatted with
  clang-format, names are stable, and names or comments carry the spec id, such
  as `// spec: routes.wrapper_borrow`, because compiler errors and debugger
  sessions land in it.

## Generated Code Lifecycle

Generated library headers are committed. Dingo is header-only, and consumers
using FetchContent or a package must not need Python. Generated tests and probes
are written to the build directory at build time, as the matrix is today.

### Ownership

- Every generated file starts with a marker line naming the generator and the
  spec entries it comes from, and saying that the file must not be edited.
- Generated headers use the `.gen.h` suffix and live in the layer directory they
  belong to, so `layers-check` applies to them unchanged.
- A committed manifest, `spec/generated.manifest`, lists every generated path
  with a content hash.

### Regeneration

The `generate` target:

- computes the complete output set from the spec
- writes only files whose content changed, so unchanged headers do not trigger
  rebuilds
- deletes every file listed in the previous manifest that is no longer generated
- refuses to overwrite or delete a file that is not in the manifest or does not
  carry the marker, and reports it instead
- writes the new manifest

Output is deterministic: iteration is sorted, output contains no timestamps,
absolute paths, or environment data, Jinja and clang-format versions are pinned
in the `uv` lock, and generated files use LF line endings through
`.gitattributes`.

In development builds the build depends on `generate`, so editing the spec or a
template and rebuilding regenerates the headers. Consumer builds never run the
generator.

### Checks

The `generate-check` target is part of `check`. It generates into a temporary
directory and fails on any difference from the tree: changed content, missing
files, stale files that carry the marker or appear in the manifest but are no
longer generated, and hand edits detected by the manifest hashes.

CI additionally runs `generate` after `check` and fails when
`git status --porcelain --untracked-files=all` is not empty, uploading the diff
as a failure artifact. That catches uncommitted additions, modifications, and
deletions. The check runs on Linux and Windows to show that generation does not
depend on the platform.

## Compatibility With The Current Library

The public API is preserved, and the internals are free.

### Public Surface

The new implementation provides the [public API](#public-api) listed in the
spec, so existing user code and the examples compile unchanged. A generated
check compiles each public header alone and every example against the new
implementation.

### Internals

Everything else can be replaced completely: headers, names, directory layout,
and header layers. No internal header is kept for compatibility. Current code
that is already decision-free, such as constructor detection, may be moved into
the primitive layer instead of being rewritten, but it is moved, not shared
between the two implementations.

### Development Strategy

The new implementation is developed in the same tree, in its own internal
directories, next to the current one. A development option,
`DINGO_IMPLEMENTATION=next`, makes the public headers forward to the new
implementation. A CI job builds with the option from the first milestone on and
runs the cases the new implementation supports so far, together with the
differential tests and the contract probes. Consumers keep the current
implementation until the switch.

The switch happens when the new implementation passes the conformance suite, the
differential tests with only declared behavior changes, and the contract probes
on every CI job. It makes the new implementation the only one and deletes the
current internals and the option.

### Differential Testing

A pinned baseline of the current library is built from a fixed commit, initially
`ced6760`, in a separate build. Every conformance and matrix case compiles
against both the baseline and the working tree and emits an outcome record: the
result or diagnostic, values, identity relations, construction, copy, move, and
destruction counts, and allocation counts. Compile-time diagnostics are compared
by whether the case compiles and by diagnostic class, because the message text
is expected to change. The two builds never share a translation unit, so there
is no ODR conflict.

Only cases written against the public API take part. Existing tests that use
internal names, such as the constructor-detection matrix family, are ported to
the primitive layer or replaced by spec conformance cases.

A comparator diffs the records. Every difference must match an entry in the
spec's behavior changes, and every entry must match an observed difference, so
stale entries fail too:

```yaml
behavior_changes:
  static_ambiguity_in_mixed_container:
    cases: mixed/*/singular_ambiguous_static
    baseline: throws type_ambiguous_exception
    now: compile error ambiguous_binding
    decision: see Decision Log
```

The contract probes run against the baseline too. A regression in instantiation
counts, object size, code size, or inlining is a failure unless it is declared
as a behavior change.

### Moving The Baseline

The baseline moves only deliberately. Moving it absorbs the declared behavior
changes, which are removed from the spec and recorded in
[History](../history.md) for the release notes.

## Testing And Conformance

The spec has a Python reference implementation that computes the expected
outcome of every case: success or diagnostic, object identity, and copy, move,
and construction counts.

- Every spec case runs in every domain configuration: static, runtime, mixed,
  parent, and local bindings. A domain is excluded only by a declared
  binding-time constraint.
- The conformance suite runs at every language tier and with every compiler
  variant the compiler accepts.
- Each index structure is tested against a linear scan of the binding table.
- Diagnostics tests compile each failing case per tier and check the formatted
  message on C++26 and the type names in the instantiation note before that.
- Differential tests compare outcomes with the
  [baseline](#differential-testing).
- Contract probes enforce the [contracts](#contract-enforcement).

How much a conformance test proves depends on how the code was produced:

- Tables precomputed by the same Python code the reference implementation uses
  are not checked by comparing them with the reference, because that comparison
  is circular. It tests only the emission and the C++ code that reads the table.
  The rules themselves are tested by property tests at the spec level, for
  example "borrow-only storage never yields a consuming route" and "every route
  to a stable source preserves identity".
- Code translated from rules, such as the engine, is an implementation
  independent of the reference implementation, which interprets the same rules.
  Cross-testing the two checks the translation.

The [generated matrix](../../test/matrix/README.md) migrates to consuming the
spec. Its capability tags and hand-written policies are replaced by expectations
from the reference implementation.

## Open Decisions

Writing the spec requires one answer for each current asymmetry. Each entry
records the decision when it is made.

- External scope in static containers. Decision: the container takes the
  instance of every external binding as a constructor argument, in the order of
  the bindings. "Static storage must be default constructible" is an
  implementation limit, not a binding-time constraint.
- Runtime key values against a static table. Proposal: selection over key values
  runs at runtime even when the table is static. Decision: open.
- Duplicate registrations. Either reject them at registration in both domains or
  accept them in both. Decision: open.
- Empty collections. One rule for both domains. Decision: open.
- Auto-construction on a miss in `static_container`. Decision: open.
- Fixed keys in runtime registration. Decision: open.
- Movability of static containers. Decision: open.
- Spec location and format. Decision: a typed Python package under `spec/`
  exporting JSON, see M1.1.
- Generated file naming. Decision: the `.gen.h` suffix in the layer directory,
  see M1.2.
- Generated tests in the repository. Proposal: generate them at build time and
  do not commit them. Decision: open.
- Baseline commit. Decision: `ced6760`, recorded in
  `test/differential/baseline-commit`, see M0.2.
- Location of the new implementation. Decision: `include/dingo/core/`, its final
  location, so the switch deletes the current internals without moving the new
  ones. It is a layer-0 module until the primitive layer of M2.3 refines it.
- The wrapper extension point. Decision: `dingo::wrapper_traits`, declared in
  `dingo/type/type_traits.h`, which the standard wrappers specialize too. A
  specialization declares the layer kind, the family, the element types,
  `rebind`, `element`, and `make`; an owning handle adds `adopt`, a sharing
  handle a scope publishes before its element exists also adopts an address with
  a deleter, and a sole-owner handle adds `share`, which hands its element to
  the sharing handle of its family. A handle converts to the handle of an
  interface by its converting constructor. `dingo::type_traits`, the per-scope
  `dingo::storage_traits`, and `dingo::is_interface_storage_rebindable_v` leave
  the public API, declared as the behavior change `wrapper_extension_point`.
- Development strategy. Proposal: in the same tree behind
  `DINGO_IMPLEMENTATION=next`, rather than on a long-lived branch. Decision:
  open.
- Constant-time type identity on GCC with UBSan. A type identity is the address
  of a per-type tag, and GCC rejects comparing the addresses of two distinct
  objects in a constant expression under `-fsanitize=null`,
  `-fsanitize=nonnull-attribute`, or `-fsanitize=returns-nonnull-attribute`,
  which `-fsanitize=undefined` enables, so a static container does not compile
  there. Every address spelling fails, a variable template, a function, a member
  array, or a member object. Decision: a GCC defect, handled by the compiler
  variant `named_type_id`. The library detects the defect by whether the
  comparison is a constant expression in a template argument, and then compares
  type names in constant evaluation and addresses at runtime;
  `DINGO_VARIANT_NAMED_TYPE_ID` overrides the detection.

## Risks

- Large static graphs can reach `constexpr` evaluation step limits, so the
  engine's algorithms must stay linear.
- MSVC's `constexpr` evaluator is the slowest and the most likely to crash, as
  the compiler exclusions in the matrix already show.
- Instantiation detail from `-ftime-trace` exists only on Clang. Counts are
  gated on Clang, on the assumption that the structure carries over to GCC and
  MSVC.
- Inlining can be checked only where object code can be inspected reliably.
  Probes on other compilers report instead of gating.
- Reflection stays an optional tier until GCC, Clang, and MSVC all ship it.
- Static error messages must not regress before C++26. The generated carrier
  templates keep type names visible in instantiation notes.
- A bug in a generator template is replicated across all generated code. The
  conformance tests and contract probes run on the generated output, not on the
  templates.
- Users read, debug, and receive compiler errors in generated headers, so their
  readability is a requirement, not a nicety.
- The differential harness doubles the build of the conformance suite. It can
  run on a subset of CI jobs, as the `full` matrix profile does.
- Two implementations coexist until the switch. The development option must
  never reach consumer builds, and the period should stay short by growing the
  supported cases milestone by milestone.

## Implementation Tracking

Each work item has a status: not started, in progress, done, or dropped. When an
item changes status, update it here in the same change, and add a dated entry to
the [decision log](#decision-log) when the change makes or revises a decision.

### M0: Baseline

#### M0.1: Contract Baseline

Status: done.

Write synthetic scaling programs and a `-ftime-trace` instantiation counter, and
record instantiation counts, container object sizes, inlining, and header costs
for the current code, next to the existing code size probes.

Done when the probes run as a lit test and the baseline values are committed.

[`test/contracts/probe.py`](../../test/contracts/probe.py) records the baseline,
as described in [Contract Baseline](../development.md#contract-baseline).
Inlining is recorded as the calls each code size probe makes and the number of
out-of-line Dingo functions. Header cost is recorded as preprocessed Dingo
lines; compile time is not deterministic and is not recorded. Baselines exist
for GCC 16 and Clang 22 on x86_64 with C++20. CI has no Clang 22 job, so
instantiation counts are checked locally only until a CI Clang toolchain has a
baseline.

Observations from the baseline:

- Bindings that are never resolved are not free. With 32 such bindings, a static
  container instantiates 6450 Dingo class templates and a runtime container
  9043, against 8880 and 9503 when every binding is resolved.
- With 32 bindings, runtime containers instantiate 58 to 100 percent more Dingo
  function templates than static containers for the same bindings.
- A static container is 8 bytes per value binding, and a mixed container adds
  112 bytes to it. A runtime container is 8 bytes.

#### M0.2: Differential Harness

Status: done.

Build the pinned baseline separately, emit outcome records from the public-API
matrix cases, add the comparator, and add an empty behavior changes list.

Done when the matrix outcomes of the working tree compare equal to the baseline
in CI.

The harness is described in
[Differential Tests](../development.md#differential-tests). Records are written
per assertion, so a changed expectation shows the observed value of both sides.
A self-test that changed the text of `type_not_found_exception` was reported as
32 undeclared differences, including message lengths observed by unrelated
parent-container cases, and passed once declared. The `differential` workflow
has not run on GitHub yet.

### M1: Spec And Generator

#### M1.1: Spec Package And Schema

Status: done.

Create `spec/` with the typed schema and JSON export.

The package is described in [Spec Package](../development.md#spec-package). The
schema covers every section of the [specification](#specification). All sections
are empty except the C++ language capabilities, which the later milestones build
on. Public API entries are language-agnostic, and their C++ spellings live in
the C++ projection as API bindings.

#### M1.2: Generator Infrastructure

Status: done.

Add the generator with Python data computation and logic-free Jinja templates,
markers, the manifest, stale file removal, deterministic output, LF line
endings, the `generate` and `generate-check` targets, and the CI step that
checks for a clean tree.

Done when `check` runs `generate-check`, CI fails on uncommitted generated
changes, and a first generated header is committed.

The generator is described in
[Generated Code](../development.md#generated-code). The first generated header
is `include/dingo/core/base/capabilities.gen.h`, which defines a `DINGO_HAS_*`
macro per language capability for the mechanics of M2.2. Build ordering after
`generate` is verified with Ninja only. The clean-tree CI step has not run on
GitHub yet.

#### M1.3: Public API

Status: done.

Record the public include paths, names, extension points, and configuration
macros of the current library in the spec.

Done when the generated public surface check passes against the current
implementation.

The spec records 93 entries: 22 include paths, 54 names, 11 extension points,
and 6 configuration macros, taken from the documentation, the README, and the
examples. Names reachable only through `dingo::detail` are internal. The check
is described in [Public API Check](../development.md#public-api-check).

#### M1.4: Contracts Schema

Status: done.

Specify contracts for the public operations and components, including inlining,
seeded with the baseline values from M0.1.

The spec records 38 contracts across object size, compile cost, runtime cost,
code size, inlining, and header cost, each linked to the baseline metrics that
observe it or marked as not observable with the reason.
[Contract Evaluation](../development.md#contract-evaluation) reports which hold
on the current implementation. On the recorded baselines, 14 hold on both
toolchains. Two are violated on Clang 22: two static resolution probes still
call Dingo functions, and chain instantiations grow slightly faster than
linearly. The others, including every contract on a construct of the new
implementation and every runtime cost contract, are not observable by the
current probes.

#### M1.5: Shapes And Scopes

Status: done.

Specify the shape algebra, the C++ projection, and the four scopes as
descriptors.

The spec records 8 layer kinds, 10 storage forms as patterns over layer kinds,
and the four scopes with their slot, materialized category, stability, source
access, recursion guard, source retention, and the request categories they
publish per storage form. `shared_cyclical` publishes the leaf and a sharing
handle whatever the stored form, which its descriptor records as its own
exposure rule. The C++ projection names only the scope tags. The
[Scope Check](../development.md#scope-check) compares the descriptors with the
current scope policies.

The current implementation classifies stored shapes with dedicated cases for the
standard sole-owner, sharing, and optional handles instead of the wrapper-traits
extension point user wrappers go through. The new implementation classifies
every wrapper through the extension point.

#### M1.6: Route Rules

Status: in progress.

Specify the route rules with access, stability, cache, and retention.

The spec records 25 route rules over terms of a shape and a value category:
borrowing through layers, copying, taking addresses, the consuming rules that
take, wrap, and transfer ownership, and the upcasts that reach declared
interfaces, including a kept sharing handle of an interface. Layers carry an
opaque wrapper family: a converted handle keeps its family, and a layer a rule
creates may be of any family. They are described in
[Route Conformance](../development.md#route-conformance). Remaining: custom
conversion traits, and the conversion cache beyond the kept interface handle.

#### M1.7: Reference Implementation For Routes

Status: in progress.

Implement route derivation in Python, producing the expected outcome, identity,
and lifetime counts of a request, with spec-level property tests for the rules.

`spec/reference/routes.py` computes registration, lookup, service,
constructions, copies, identity, and stability for a scope, a stored shape, the
declared interfaces, and a request, and is checked by property tests over every
stored shape of up to two layers. The route conformance check compares it with
46,956 cases extracted from the baseline in the runtime, static, and mixed
containers, covering standard and user wrappers and bindings with one and two
interfaces. Every difference is declared by one of 15 behavior changes:

- borrowing is compositional: every reachable object is borrowed and every
  copyable one copied, through any combination of layers
- borrowing never moves: no container serves an rvalue reference from a
  borrowing scope, which the mixed container did
- consuming is compositional: the element of a consumed optional can be taken,
  and what is taken can be wrapped or transferred
- consumed objects are never borrowed: the runtime and mixed containers returned
  a reference to a destroyed object
- consuming moves minimally: every container makes the fewest moves any of them
  makes
- eager registration checks: static and mixed containers reject a binding their
  scope cannot store when it is declared
- shared_cyclical stores flat shapes: a nested sharing handle aborted on an
  lvalue reference request
- shared_cyclical stores no pointers: the stored pointer was null
- shared_cyclical exposes no arrays: the policy exposed them, but no array
  binding compiled
- user wrappers are layers: shared_cyclical rejected user sharing handles,
  because only the standard handles have storage cells
- interface bindings compose with layers: interfaces of a leaf held in an
  optional or an alternative are reached by borrowing
- interface bindings serve an interface: a binding none of whose interfaces any
  request can reach is rejected instead of accepted and never found
- interface handles are retained: the mixed container returned a reference to a
  converted interface handle that was already destroyed
- interfaces do not rebind storage: a binding with one interface behaves as one
  with several
- static containers take external instances, following the Open Decisions
  proposal

Remaining: the subjects of M1.6.

### M2: New Implementation Foundations

#### M2.1: Implementation Switch

Status: done.

Create the directory of the new implementation, add the
`DINGO_IMPLEMENTATION=next` development option that makes the public headers
forward to it, and add the CI job that builds with the option.

Done when the CI job builds the new implementation and runs the cases it
supports so far.

The generator emits `include/next/<path>` for every public include path of the
spec. Each forwards to the facade of the new implementation under
`include/dingo/core/api/`, or reports that the new implementation does not
provide it yet. `DINGO_IMPLEMENTATION=next` puts `include/next` first on the
include path of the `dingo` target, and the `dingo_next` target always does, so
the tests of the new implementation build in every development build next to the
current implementation. The CI job is deferred: no CI runs until the new
implementation is complete.

#### M2.2: Mechanics

Status: done.

Add the mechanics header with capability detection, the language tiers, the
compiler variants with override macros, the annotation macros, `pack_at`,
`dependent_false`, and `fail<>`. It is generated from the language capabilities
and compiler variants in the spec.

Done when no header outside the mechanics header tests a standard version or a
compiler, and every standard C++ variant is tested in Linux CI.

`include/dingo/core/base/` holds the generated `capabilities.gen.h` and
`variants.gen.h`, the configuration macros of the public API in `config.h`, and
`mechanics.h` with the type list, `dependent_false`, and `pack_at_t`. Each
variant point of the spec gets a `DINGO_VARIANT_*` macro that defaults to its
capability and can be defined to override it, and every variant the compiler
accepts is tested: by `dingo_next_test` and by a lit test that forces the
portable implementation. `fail<>` moves to M5.3, where the diagnostics it
reports are generated. Compiler variants use the same override macros and arrive
with the primitives that need them in M2.3. The new implementation has its own
header layers in `tools/layers.py`, and neither implementation includes the
other.

#### M2.3: Primitive Layer

Status: in progress.

Add the decision-free primitives: lifetime slots, factory invocation and
constructor detection, conversion primitives, index structures, the arena, and
the thunk shape. Add the layer rule to `layers-check`.

Done when no primitive header refers to a scope, shape kind, rule, or
diagnostic.

`core/primitives/shape.h` describes the shape of a C++ type as a constexpr
preorder array of nodes, comparable in constant expressions, without knowing the
layer kinds of the spec. `core/projection/layer_traits.h`, a layer between the
vocabulary and the engine, classifies C++ types into layer kinds: pointers,
references, and arrays as language layers, and the standard wrappers through the
same `layer_traits` a user wrapper specializes. `core/primitives/slot.h` holds
the lazy and external lifetime slots; the lazy slot reports a recursion while
its object is being constructed and is empty again after a failed construction.
`core/primitives/constructor.h` detects the constructor of a type and calls it
with arguments that resolve their parameter types, the `detected_constructor`
factory kind of the spec. Remaining: the two-phase slot, the compiler variant of
constructor detection for MSVC, the declared, function, and callable factories,
conversion primitives, index structures, the arena, and the thunk shape.

#### M2.4: Vocabulary And Descriptors

Status: in progress.

Generate the vocabulary enums, the public tag types, the descriptor structs, and
`descriptor_v`.

`core/vocabulary/vocabulary.gen.h` holds the layer kinds with their traits, the
value and request categories, the storage forms, the scope descriptors with
their exposure tables, and the route rules as constexpr data, generated from the
spec. `shape_v<T>` is the descriptor of a type. Remaining: the public tag types.

#### M2.5: Walking Skeleton

Status: in progress.

Make `container<>` and `static_container` serve shared and external bindings end
to end through the descriptors, the engine, and both backends, before the
remaining primitives grow around them.

`core/engine/execute.h` walks the rules of a term on C++ objects, one operation
per rule as the rule table describes it, through `layer_traits`. The static
backend selects the binding and the term at compile time and walks it directly.
The runtime backend publishes the terms of a binding at registration with a
thunk per term, and a request selects with the same engine functions. A last
copy, address-of, or decay of a borrowed object depends only on the requested
type, so the runtime thunk returns the borrowed object and the request completes
it, and a shared value is copied once in both backends. `core/api` holds the
facade of `dingo/container.h`, `dingo/static_container.h`, and the scope
headers, with the registration vocabulary and the public exceptions, and the
scope tags are generated from the spec. `dingo_next_test` exercises it through
the public include paths. A shared binding constructs its object by the detected
constructor, resolving each argument from the container through the backend that
serves the request, and constructs the element of every layer in place.

A unique binding constructs its object for every request, and the walk applies
the consuming rules to it: it moves an element out of a layer, shares the
element of a sole-owner handle, and converts a consumed handle to the handle of
an interface. The plan of a term divides it at its boundary. The binding walks
the rules up to a last copy, address-of, or decay, or up to the first rule that
creates a wrapper of any family, and the request applies the rest: it wraps into
its optional or adopts into its handle. The binding upcasts the pointer it hands
over when the request upcasts the handle it creates. A runtime container erases
the binding at the boundary and receives the boundary object by value through a
function of its type, which the request checks, so neither container moves an
object the reference does not. A request of an rvalue reference to an object it
constructs refers to an object the container keeps until it is destroyed, as the
current implementation does. A shared binding keeps the handle of an interface
converted from its stored handle in the conversion cache of its slot, so a
request can refer to it.

A shared_cyclical binding publishes its object before it constructs it: a plain
leaf in its slot, and a sharing handle, a standard or a user one, as a handle
that adopts a block whose deleter destroys the element only once it is
constructed. A request during the construction refers to the published object,
so objects can depend on each other in a cycle; an alternative is not published,
since reaching its element reads it. When the outermost publication fails, every
slot published during it is reset, as the current implementation does.

The route extractor runs against the next implementation with
`--implementation next`, and `conform.py --exact` compares its table with the
reference implementation without behavior changes. For shared and external
bindings in runtime and static containers, 23,478 cases including user wrappers,
registration, service, copies, moves, temporaries, constructions, identity, and
stability match the reference without exception, and so do the 31,304 cases of
all four scopes. A static container takes the instance of every external binding
as a constructor argument. Remaining: the messages of the exceptions, and an
interface a published leaf inherits virtually, which cannot be reached before
the leaf is constructed.

### M3: Routes And Storage

#### M3.1: Route Table

Status: done.

Generate the per-layer route table as `constexpr` data.

The route rules are part of `vocabulary.gen.h`. A rule is a layer kind and value
category it starts from and a layer kind and value category it derives, which
every rule of the spec is; the generator rejects any other.

#### M3.2: Request Matcher

Status: done.

Implement one descriptor-based request matcher, used by both backends.

`core/engine/derive.h` registers a binding, finds and derives the terms it
serves, and selects the term that serves a request, as constexpr functions over
shapes. It is cross-tested against the reference implementation:
`test/next/engine_cases.py` generates 35,616 cases into the build tree, over
every scope, a copyable, a move-only, and an implementation leaf with one and
two interfaces, stored shapes of up to two layers in the standard and a user
wrapper family, and every request kind, and `dingo_next_test` requires the
engine to compute the same registration, lookup, service, and rules.

#### M3.3: Storage Construct

Status: not started.

Implement the one storage construct that selects its slot primitive and
materialization from the scope descriptor.

Done when adding a scope with an existing slot primitive requires only a spec
entry.

#### M3.4: Route Conformance

Status: not started.

Generate route conformance tests across runtime, static, and mixed containers
from the reference implementation, and run the route cases differentially
against the baseline.

Done when the tests pass and every difference from the baseline is either fixed
or listed in [Open Decisions](#open-decisions).

The baseline side exists: the route extractor and the conformance check of M1.7.
Remaining: extracting the same table from the new implementation and requiring
it to match the reference implementation exactly.

### M4: Lookup And Selection

#### M4.1: Lookup And Selection Spec

Status: not started.

Specify the lookup filter, the index structures, and the selection rules, and
extend the reference implementation with them.

#### M4.2: Lookup Filter

Status: not started.

Implement lookup as one filter over the binding table for every key kind, with
the index structures as declared accelerations tested against a linear scan.

#### M4.3: Generated Engine

Status: not started.

Generate `engine::select` from the selection rules for singular and collection
requests, local bindings, parent fallback, and auto-construction, and cross-test
it against the reference implementation.

Following the route engine of M3.2, the selection engine is a generic
interpreter of generated selection rule tables rather than generated code.

#### M4.4: Static Backend

Status: not started.

Implement static and local static resolution on the engine.

#### M4.5: Runtime Backend

Status: not started.

Implement runtime resolution on the engine, memoized per request type.

#### M4.6: Mixed Backend

Status: not started.

Pass static candidates as a constant input and merge runtime candidates.

### M5: Graph Validation And Diagnostics

#### M5.1: Graph Rules

Status: not started.

Specify and generate cycle and missing-dependency checks on descriptors, with
one rule for compile-time and runtime detection.

#### M5.2: Diagnostics Catalog

Status: not started.

Specify every diagnostic with its condition, exception type, and message
template.

#### M5.3: Generated Diagnostics

Status: not started.

Generate the diagnostics enum, `diag_message`, `diag_literal`, the carrier
templates, the exception mapping, and the `fail<>` mechanic that reports a
diagnostic at compile time.

#### M5.4: Compile-Time Detection Rule

Status: not started.

Derive whether a diagnostic is reported at compile time from the sources that
remain open.

#### M5.5: Diagnostics Tests Per Tier

Status: not started.

Generate lit tests checking the diagnostic text per tier.

### M6: Resolve Asymmetries

Status: not started.

Implement each decision recorded in [Open Decisions](#open-decisions), each
declared as a behavior change against the baseline.

### M7: Contract Enforcement

Status: not started.

Generate the instantiation, object size, runtime cost, code size, inlining, and
header cost probes from the contracts, replace the hand-written code size
limits, and gate CI on them.

### M8: Language Tiers

Status: not started.

Use concepts and `consteval` on C++20, formatted `static_assert` messages, pack
indexing, and `= delete("reason")` on C++26, and add reflection as an optional
tier.

### M9: Switch

#### M9.1: Full Conformance

Status: not started.

The new implementation passes the conformance suite, the differential tests with
only declared behavior changes, and the contract probes on every CI job.

#### M9.2: Switch And Removal

Status: not started.

Make the new implementation the only one, delete the current internals and the
development option, and move the baseline.

#### M9.3: Matrix Migration

Status: not started.

Make the generated matrix consume the spec and retire its hand-curated
capability tags.

## Decision Log

- 2026-10-19: Use one decision engine over descriptors, evaluated at compile
  time or at runtime, instead of generating separate runtime and static
  implementations from the spec.

- 2026-10-19: Keep one header tree with capability gates confined to a mechanics
  header, instead of a generated tree per C++ standard.

- 2026-10-19: Treat compile-time cost as a spec-level contract enforced by
  instantiation counts, not by wall time.

- 2026-10-19: Generate every construct that encodes a spec decision, including
  the engine and the backend glue. Hand-written code is limited to decision-free
  primitives and a thin facade, separated by a header layer. This revises the
  earlier plan to hand-write the engine.

- 2026-10-19: Contracts on compile cost, object size, runtime cost, code size,
  inlining, and header cost are part of the spec and apply to generated and
  hand-written code alike.

- 2026-10-19: Use the generator for unification: one construct per concept, with
  separate code only at declared variant points. Storages become one construct
  over scope descriptors, and lookups one filter with index structures as
  accelerations.

- 2026-10-19: Commit generated library headers, track them in a manifest that
  drives stale file removal, and fail CI when regeneration changes the tree.

- 2026-10-19: Treat compiler workarounds as named variants, tested on every
  compiler that accepts them.

- 2026-10-19: Test the new implementation differentially against a pinned
  baseline of the current library. Behavior and contract changes must be
  declared in the spec.

- 2026-10-19: Everything below the public API may be replaced completely. The
  new implementation is developed in the same tree behind a development option
  and replaces the current internals in one switch once it passes conformance,
  differential, and contract checks. The public API is recorded in the spec.

- 2026-10-19: Contract baselines are keyed by compiler family, major version,
  architecture, and C++ standard, and act as upper limits. Header cost is
  measured as preprocessed Dingo lines rather than compile time, so it is
  deterministic.

- 2026-10-19: The differential harness records an observation per matrix
  assertion rather than a pass or fail per case, and runs as a separate
  workflow, because the build workflow is reused by the release workflow on a
  source tree without git history.

- 2026-10-19: Public API entries name operations and extension points
  language-agnostically. Their include paths, names, and macros are C++
  projection data, so the semantic sections contain no C++ text.

- 2026-10-19: Development builds regenerate before compiling, controlled by
  `DINGO_GENERATE_ON_BUILD`. CI turns it off so that `check` verifies the
  committed tree, then regenerates and requires a clean tree. The clang-format
  configuration treats the generated-file marker as a comment pragma, so it is
  never reflowed.

- 2026-10-19: The spec names no concrete wrapper types. Layer kinds are classes
  of handles defined by traits, and concrete wrappers, the standard ones
  included, map to them through the wrapper-traits extension point of the C++
  library, which users extend with their own handles.

- 2026-10-19: The scope descriptors and route rules of the spec describe the
  target semantics, not the baseline. Defects of the baseline found by the route
  extraction, a reference to a destroyed object and two shared_cyclical shapes
  that abort or publish a null pointer, are fixed in the new implementation and
  declared as behavior changes. A borrowing scope never serves an rvalue
  reference, and the minimal move count of any container is the contract.

- 2026-10-19: Interfaces and user wrappers follow the same rules as the leaf and
  the standard wrappers. A user wrapper is a layer of the kind its traits
  declare, in every scope. A binding with interfaces is found only by them and
  reaches them by conversion, with one interface as with several. A binding
  whose interfaces no request can reach is rejected.

- 2026-10-19: No CI runs until the new implementation is complete. The new
  implementation is built and tested locally through the `dingo_next` target.

- 2026-10-19: The engine is a generic constexpr interpreter of the generated
  rule tables instead of code generated per rule. It makes no spec decision of
  its own, it is cross-tested against the reference implementation, which
  interprets the same rules independently, and it is far less code. A route
  derives shapes at most one layer larger than the stored shape, and ties are
  broken by the order of the rules in the spec, so a runtime binding publishes
  the same terms before a request as a static one selects from.

- 2026-10-20: A wrapper becomes a layer through one public trait,
  `dingo::wrapper_traits`, with the layer kind, the family, the element types,
  `rebind`, `element`, and `make`. The standard wrappers use it as a user
  wrapper does, and the storage traits and the interface rebinding trait leave
  the public API. The public API check requires an entry a behavior change
  declares with an `api:` pattern to be missing from the current implementation.

- 2026-10-20: The unique scope constructs an object per request, and the plan of
  a term divides it at its boundary between the binding and the request, the
  same in both backends: a layer a rule creates from a leaf or a pointer is of
  any family, so only the request names its wrapper type. An owning handle's
  wrapper traits add `adopt`, and a sole-owner handle's `share`. A request of an
  rvalue reference to an object it constructs refers to an object the container
  keeps, and a binding keeps a converted interface handle in the conversion
  cache of its slot.

- 2026-10-20: A static container takes the instance of every external binding as
  a constructor argument, in the order of the bindings.

- 2026-10-20: The rejection of constant address comparisons under GCC's null and
  nonnull sanitizers is a compiler defect. The compiler variant `named_type_id`
  detects it and compares type identities by name in constant evaluation.

- 2026-10-20: shared_cyclical publishes a plain leaf in its slot and a sharing
  handle as a handle of a block whose element it constructs later, through the
  `adopt` with a deleter of the wrapper traits, so user sharing handles are
  stored like the standard one. A failed outermost publication resets every slot
  it published.
