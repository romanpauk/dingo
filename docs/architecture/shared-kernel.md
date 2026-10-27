# Shared Kernel And Generated Coverage

Status: historical design note. The `test/matrix` corpus this document refers to
has been retired; see [Matrix Retirement](matrix-migration.md).

This page describes a design for removing the duplicated runtime and
compile-time implementations without replacing the library with a second
implementation, and for making the storage and conversion forms exhaustive by
construction. It is deliberately incremental: every step is independently
testable and every step has a cost gate against the current `master`.

## Implemented In This Slice

The first slice lands the shared decision helpers and the generated coverage
infrastructure described below. It changes no public API and no behavior.

- `binding/binding_selection.h` resolves a runtime selection and reports
  ambiguity or the missing path in one place.
- `binding/binding_resolution_policy.h` resolves two sources under one policy,
  so the hybrid container no longer spells out the static/runtime conflict,
  local-precedence, and missing cases itself.
- `binding/binding_collection.h` owns the empty-check, reserve, and append
  sequence used by the runtime and hybrid containers.
- `container.h` and `static_container.h` use the shared selection and missing
  helpers instead of repeating the parent fallback and unresolved request.
- `context/context_base.h` owns the dependency-resolution mixin used by both
  `runtime_context` and `static_context`; selected-key requests no longer have
  two copies.
- `container_collections.h` owns the public collection spellings once. The
  runtime and static containers provide only the internal
  `construct_collection_by_key` path, the keyed `append_collection`, and the
  keyed `count_collection`.
- `static/binding.h` uses the shared resolution-address kernel for the last-mile
  conversion, the same kernel the runtime binding uses.
- `binding/binding_resolution.h` now owns the storage holder, conversion-cache
  type computation, cache-slot construction policy, and registered-type
  descriptor used by both binding backends, in addition to the materialization
  and address helpers.
- Runtime value requests address consumed routes directly. A consuming scope
  materializes its source in the resolution context, so the route hands that
  address over with consume access instead of moving the value into a second
  context slot. The runtime path therefore constructs the result with one move,
  the same as the static container, and requests no longer need a typed maker
  indirection.
- `tools/model/` is the first coverage catalog. It enumerates every built-in
  storage scope by form, every selection policy by source status, and every
  scope/form/route-category row. Every route row is classified as supported,
  unavailable with a reason, or a tracked gap; the generator emits one
  translation unit of compile-time assertions for the supported rows and checks
  that every published route target is reachable from both request matchers,
  that the scope access satisfies each route, that an unavailable category
  publishes no route, and that the operation-kind, source-retention, internal
  conversion step, and target-shape sequences derived by `route_derivation.py`
  match. The recipes are ordered rules over semantic kind, source shape, and
  wrapper properties, so a new scope reuses them and a custom wrapper with the
  same shape follows the same rule instead of adding a row per form.
  Representative routes also assert that no conversion cache is published
  unexpectedly. Interface probes derive their value/object/rvalue counts from
  the same model by form and copyable interface value. Raw dynamic arrays are
  declared unavailable because the element count is not part of the type;
  unsized shared array handles under `shared_cyclical` are an explicit
  constraint for the same reason. `generated-check` fails when the library
  catalog and the model drift.
- Scope lifetime and source shape are descriptors now: `scope_info_of` derives
  consuming/stability/guard/retention from the generated policy, and
  `storage_yield_traits` derives the yielded type and source category from the
  storage shape. Both value and reference/dependency paths use them through
  `with_source_lifetime`; the old `with_*` functions remain as lifetime backends
  behind that dispatch.
- The direct/lift decision and execution are one `resolution_policy` in
  `include/dingo/static/resolution_policy.h`. It probes whether the static
  binding serves the wrapper route, exposes a compile-time or runtime-fallback
  mode, and provides `resolve` for the selected arm. The direct and lift bodies
  are passed as tagged callables, so the arm that is not selected is not
  instantiated. `container::resolve_entry`, the dependency origin overload,
  `static_container` public resolve, and the `static_container` origin overload
  all use the same executor. A selected binding exposes a `resolution_route` for
  one resolution and a `resolution_candidates` for its request list; both the
  runtime index dispatch and the compile-time static selection resolve through
  the same route types. Each resolution exposes a `conversion_descriptor` with
  its step list and retained cache types, and `conversion_walk` applies the
  steps as the single conversion executor. Leaf conversions stay the primitives;
  wrapper, alternative, retention, and source steps only compose them.
- Behavior coverage follows the same catalog. `behavior_cases.py` declares each
  form once (storage, factory, external instance) plus one expectation entry per
  scope, then derives the runtime and static product from the route ledger.
  Every supported form is behavior-tested or listed in `BEHAVIOR_GAPS` with a
  reason, so a new form or scope cannot silently lose execution coverage. The
  classes assert request identity, construction counts, optional copy and move
  counts, array element identity, leaf addresses, and the lifetime balance
  `destructions == constructions + copies + moves`.
- `include/dingo/storage/scopes.gen.h` owns the built-in `storage_scope_policy`
  specializations and the `shared_cyclical` form-list table. The per-scope
  headers keep their public tag and storage implementation, but the policy and
  per-form publication data is generated from the model, so a new scope or form
  cannot bypass the coverage product. The static graph uses the generated
  `allows_cycles` and `consumes` fields instead of hard-coding scope names.
- `tools/cost/measure.py` records object text/data/bss sizes and optional Clang
  time-trace instantiation counts, and compares two records. Wall time is
  reported as a trend only. The first slice uses it to track the cost of the
  shared helpers; closing the remaining compile-time gap is a separate
  optimization step, not a condition for landing the structural change.

The remaining sections describe the complete target design and the migration
steps that follow.

## The Problem

Dingo already knows every type at compile time in both registration modes.
`register_type<T>()` and `resolve<T>()` are templates in the runtime container
too. The real difference is narrower:

- a runtime binding table is open until the program runs, and a static binding
  table is closed when the container type is formed;
- a runtime key value may arrive at run time, while a static key value is part
  of the request type;
- a runtime binding owns arena state and can roll back, while a static binding
  owns inline state.

The implementation nevertheless decides most questions twice.

Shared today:

- registration normalization (`registration/type_registration.h` and
  `binding/binding_model.h`);
- storage classes and `storage_materialization_traits`;
- conversion operations in `resolution/resolution_operation.h`;
- low-level materialization and conversion helpers in
  `binding/binding_resolution.h`.

Implemented twice:

- request-to-route matching: `is_resolution_request_for_v` for static requests
  and `matches_resolution_request` over descriptors for runtime requests;
- lookup: type-list filtering and overload-set indexing for static bindings, and
  the lookup indexes for runtime bindings;
- selection, ambiguity, local precedence, parent fallback, collection merging,
  and auto-construction in `container.h`, `static_container.h`,
  `runtime/registry.h`, and `static/binding.h`;
- the binding instance and its resolver: `runtime_binding` plus
  `static_binding_resolver`;
- conversion-cache selection: per interface at runtime, per registration
  statically;
- cycle detection: a compile-time DFS in `static/graph.h`, and a recursion guard
  keyed by binding address at runtime.

That split explains the behavioral differences the matrix has to encode today:
external storage, duplicate handling, empty collections, auto-construction,
static ambiguity, runtime-key support, and container movability. It also
explains why a change in one path can pass its tests while the other path stays
wrong.

The same problem appears inside the storage and conversion tables. A scope
declares its policy once, but individual `(scope, form)` forms and conversion
routes are still C++ specializations. Each specialization is correct only if its
author remembers the form, writes its route, and adds it to the matrix. The
matrix keeps a separate, hand-curated approximation of the same table, so the
test model and the library can drift independently.

## What Failed Before

Two directions have already been tried.

- Mirroring the `runtime/` and `static/` directory layouts makes the
  correspondence visible, but it leaves both decision chains in the code. File
  symmetry does not prevent semantic drift, and it adds files without removing
  logic.
- Replacing the internals with a second, spec-interpreting implementation
  duplicates the library and moves semantic decisions into an interpreter and a
  generator. That creates a new cost surface and a large conformance surface
  before it removes the old one.

This design takes the opposite route:

- decisions are shared as **algorithms**, not as mirrored files;
- the C++ backends remain thin, monomorphic adapters;
- the generator emits **data and coverage**, not a second copy of the library;
- the migration is incremental and each stage has a measurable cost gate.

## One Decision Kernel, Two Zero-Cost Adapters

The target pipeline is:

```text
registration facts
      |
      v
binding table ----------> selection ----------> binding kernel ----------> value
 (static type list)         one algebra          one resolution path
 (runtime arena)            over candidates
 (mixed: both)
```

Only three things differ between the domains:

1. how candidates are enumerated;
2. how a selected candidate is dispatched;
3. what owns binding state.

None of those differences is a selection or conversion rule. The rules live
once.

### Binding Facts

`type_registration` and `binding_model` already produce the right facts:

- interfaces, key, scope, storage, factory, conversions, dependencies, local
  bindings;
- the stored type and whether the interface is used as the stored leaf.

Keep them as the single normalization step. They are already shared and should
not be regenerated.

### Binding Kernel

Split the current binding into a decision-free kernel and an erasure adapter.

```cpp
// One binding, no erasure, no selection decisions.
template <typename Resolution, typename Slot, typename StatePolicy>
class binding_core {
public:
  template <typename Request, typename Context>
  Request resolve(construction_scope, Context &, Host &);

  template <typename Request, typename Context>
  resolved_address resolve_address(construction_scope, Context &, Host &);
};

// Runtime adds the type-erased entry point only where erasure is required.
template <typename Resolution, typename Slot, typename StatePolicy>
class runtime_binding final
    : public binding_core<Resolution, Slot, StatePolicy>,
      public runtime_binding_interface<...> {
  static resolved_address dispatch_request(...);
};

// Static uses the same core without a vtable and without an erased request.
template <typename Resolution, typename Slot, typename StatePolicy>
using static_binding = binding_core<Resolution, Slot, StatePolicy>;
```

The kernel owns the part that is identical today:

- materialize the source through `storage_materialization_traits`;
- apply the guards and the construction scope;
- select the conversion operation carried by the resolution;
- retain converted values in the conversion cache when required;
- publish or consume the result through the shared request helpers.

The state policy owns only the part that is genuinely different:

| Concern | Runtime state | Static state | | --- | --- | --- | | Storage
placement | container arena | inline slot | | Lookup state | mutable index |
type-list selection | | Rollback | transaction callbacks | not applicable | |
Child container | arena-constructed | inline static container | | Dispatch |
erased thunk | direct template call |

This removes `static_binding_resolver`, `static_binding_activation`, and the
separate static materialization/caching ladder. It does not put a vtable or an
erased request on the static path: `runtime_binding` is an adapter around the
shared core, not the core itself.

The typed static request and the erased runtime request meet at one typed kernel
call. A small request adapter supplies the few operations the kernel needs
(target descriptor, registered descriptor, result extraction); the static
adapter is a compile-time constant, the runtime adapter reads the existing
`instance_request`.

### Selection

Selection is currently expressed as a mixture of type-list helpers, runtime
lookup calls, and `if constexpr` branches in four container files. Define one
candidate-set vocabulary and one selection algebra over it.

A candidate set provides:

- a status (`found`, `missing`, `ambiguous`);
- a count (a constant for static candidates, a size for runtime candidates);
- a visit operation that calls a typed callback for each candidate;
- an identity test (`interface` and `key`) used by the shared filter.

The adapters are:

```cpp
struct static_candidates {  // closed type list
  using bindings = type_list<...>;          // computed from binding facts
  static constexpr auto status = ...;
};

struct runtime_candidates { // open lookup result
  binding_range range;                       // pointers and keys
  binding_status status;
};

template <typename Static, typename Runtime>
struct merged_candidates { ... };            // local precedence and conflict
```

The one algebra then implements, in `binding/binding_selection.h`:

```cpp
select_one(candidates, request);      // found / missing / ambiguous
select_many(candidates, request);     // count and ordered visit
select_with_fallback(local, host, parent, request);
```

Ambiguity, local-over-host precedence, parent fallback, collection merging, and
the empty-collection rule are written once against the vocabulary. Static
`count` and `status` are `constexpr`; runtime `count` and `status` are the
existing lookup values. A branch on `is_static_source_v<Source>` selects the
adapter, not another copy of the rule.

The containers keep only their ownership and transaction wrappers:

- `container` supplies local runtime candidates, host runtime candidates, host
  static candidates, and the parent;
- `static_container` supplies their static counterparts;
- an explicit parent is just another candidate source.

This is the point where the behavioral asymmetries become decisions instead of
accidents. The design should make one decision per rule and apply it to every
source:

| Rule | Decision | | --- | --- | | Singular ambiguity | `found` with count
greater than one is ambiguous in every source | | Local precedence | local
candidates shadow host candidates of the same interface and key | | Collections
| merge local, host, and parent candidates in declared order | | Empty
collection | an explicit query with no candidates is an empty collection | |
Auto-construction | tried only after local, host, and parent candidates are
missing | | Duplicate registration | rejected when the declaring lookup
cardinality is `one` |

If a rule must differ, it is a declared property of the container or request,
not a separate code path in one backend.

### Dispatch

Dispatching a selected candidate is the only unavoidable variant point:

- static: index into a type list and call the kernel directly;
- runtime: call the thunk stored with the selected binding;
- mixed: do either, according to the source that produced the candidate.

This stays explicit and small. It must not grow into a second resolution
pipeline.

## Generated Tables And Coverage

The generator is a **coverage compiler**, not a code multiplier. It computes the
declared product of axes, emits the data tables the kernel consumes, and emits
the probes that prove every form is covered. It does not emit one C++
specialization per form.

### Catalog

Add `tools/model/` as the single declarative catalog shared by the library
generator and the matrix generator. The catalog is typed Python records, not C++
text. The first records are:

```python
scope(name="shared", stable=True, consumes=False, slot="lazy",
      exposures={...}, guard="while_unresolved")
form(name="shared_handle", borrowed=[...], consumed=[...])
route(source_form=..., target_form=..., access=..., operation=...,
      cache=..., retention=...)
key(kind="runtime", binding_time="runtime")
axis(name="container", values=["runtime", "static", "mixed"])
```

The catalog owns the semantic choices. The existing hand-written
`storage_scope_policy`, form traits, and conversion primitives remain the
implementation of those choices, because they manipulate C++ types and names.

### Products And Forms

Every axis product is materialized as rows, including intentional gaps:

- a row has an outcome: `available`, `unavailable`, or `compile_error`;
- an outcome always has a reason;
- absence of a row is an error.

For storage, the product is scope × form × exposed request category. For
conversion, it is source layer kind × target layer kind × scope access × special
form (array, alternative, optional, custom). For containers, it is registration
mode × container kind × request decoration. A rule is a function over a row; an
override is an explicit row. There is no third state in which a form is simply
forgotten.

The generator emits:

- a compact `constexpr` table per component
  (`include/dingo/generated/storage policies.gen.h`, `route_probes.gen.h`);
- one aggregate `static_assert` that the table has exactly the expected number
  of rows and that every row names a known operation;
- a coverage manifest mapping every row to at least one test case or one
  compile-fail probe;
- matrix axes, features, and capability tags derived from the same rows;
- test shards and compile-fail cases into the build directory.

The generated headers are committed because Dingo is header-only and consumers
must not need Python. The generator itself runs only in development builds.
`generate-check` regenerates into a temporary directory and fails on any
difference, stale file, or hand edit.

### Why This Is Correct By Construction

There are three distinct correctness levels.

1. **Library-owned forms.** The generator enumerates the full product and emits
   a table with an explicit outcome for every form. A missing or duplicate form
   is a generator failure and a compile-time assertion. A new built-in scope or
   operation cannot be added by writing one specialization and forgetting the
   rest; it must be declared, and the declaration forces a complete row.
2. **Extension points.** User-specialized traits (`type_conversion_traits`,
   wrapper traits, lookup definitions, factories, collections, RTTI) remain
   hand-written by design. The generator emits one compile probe per documented
   extension point and the matrix exercises every built-in member through it.
   The generator cannot make a user's specialization correct, but it can make a
   missing or untested built-in path impossible to miss.
3. **Behavior.** The catalog also drives a small reference evaluator used by the
   tests. It computes the expected outcome for each row, including which
   operation is selected and whether a request is legal. Generated tests compare
   the C++ result with that outcome. Because both are derived from the same
   table, the test does not merely repeat the implementation; it checks the
   emission and the C++ consumer against an independent evaluator.

A structural check (`generated-check`) additionally verifies that the library
does not contain built-in `storage_policy` or route specializations for forms
that the catalog says are generated. Those specializations become a compile-time
error or a lint failure. This prevents the old habit from returning.

### Test Cost

The full Cartesian product is too large to compile as behavior tests. The
catalog therefore assigns every row to an equivalence class and a
representative, and the generator enforces a coverage invariant:

- every row is covered by a compile/instantiation probe;
- every behavior class has one or more executable cases per applicable container
  mode;
- boundary forms (empty, unavailable, ambiguous, non-copyable, non-movable,
  runtime key, parent/local) are declared as their own classes;
- shards are sized by the current matrix batching limits, so adding a form does
  not recompile the world.

This is coverage by construction without a combinatorial build.

The implemented model uses that split directly:

- **Structural forms** (storage traits, route availability, access, operation
  kinds, retention) are compile-time assertions. The full storage product is one
  translation unit; the full route product is another; both stay around a second
  to compile.
- **Behavior forms** are execution cases derived from the same scope/form
  catalog. Each form declares its registration once and one expectation per
  scope; the runtime and static containers are generated. Forms the library
  publishes but cannot serve are listed as explicit behavior gaps with a reason.
- **Request shapes are derived from wrapper semantics, not listed.** A shape is
  a leaf wrapped in zero or more wrappers, and each wrapper declares what it can
  do (`owning`, `sharing`, `nullable`, `borrowable`, `array`, `variant`) and how
  it is spelled. The catalog includes const, references, pointers, optional,
  owning handles, owning array handles, arrays, and variants. The test-only
  custom families add one cross-wrapper pair each, so custom optional, unique,
  shared, and variant wrappers run through the same generated matrix and
  behavior suites. The route rules (`copy_out`, `borrow`, `move`, `take`,
  `wrap`, `share`, `adopt`, `decay`, `select_alternative`, ...) derive the first
  rule that blocks a request and name it in the report. Every derived shape is
  applied to every storable form through the real resolution kernel. The report
  has the direct route, the lifted request, the top-level verdict, the blocking
  rule, and the published route targets. A request for a wrapper the form does
  not store is lifted from the wrapper's value: value wrappers copy or move, and
  owning handles only move from a consumed value so a borrowed instance cannot
  become an owner. A matching route can still be unsafe at the container
  boundary: a consuming scope may lend an lvalue reference to a longer-lived
  dependent, but a borrow returned from the container would outlive the
  transaction that produced it. The report marks those rows as top-level gaps
  instead of hiding them.
- **The supported top-level rows execute.** `type_behavior.py` resolves every
  supported shape twice through the behavior registrations, checks stable
  identity and fresh distinctness, releases only pointers whose ownership the
  consuming scope transferred, and asserts the lifetime balance after each
  block. The catalog runs in two translation units: the runtime container and
  the static container for every statically bindable form (`external` is
  runtime-only). The shape catalog and the top-level verdict are shared with the
  matrix, so the executable set is derived rather than maintained separately.
  This is the foundation for deriving behavior and decorator cases from shapes
  instead of listing supported requests by hand.
- **The matrix keeps the scenarios that a form catalog cannot express**:
  constructor detection, invocation, dependency graphs, collections, keys,
  annotations, parent containers, caching, transactions, error paths, custom
  RTTI and allocators. Those are not derivable from a scope/form product.

Compile cost is measured per translation unit with
[`tools/cost/tu_report.py`](../../tools/cost/tu_report.py) and the `test-cost`
target. On the development host the model units compile in roughly one second
each, except the behavior unit, which is the largest and is the first candidate
for a form-based split. The generated matrix units dominate compile time (one
matrix shard has exceeded twenty seconds) because they instantiate many distinct
types per unit, not because of header parsing. Reducing distinct instantiations,
and sharding by instantiation cost rather than line count, is the effective
lever; precompiled headers were measured here and did not pay for themselves
because CMake builds one per test target.

### Matrix Migration Order

Anything that is a function of the scope/form catalog belongs in the generated
model suites. In priority order:

1. **Lifetime and count rows** (`lifetime_counts`, plain value rows). The
   behavior classes assert construction, copy, move, destruction, and the
   lifetime balance, including the unique rvalue route, so the whole feature is
   now model-owned.
2. **Plain registration rows**: one stored type, direct value/reference/pointer
   request, no wrapper, key, or feature. These are fully covered by behavior
   classes and can leave the matrix product.
3. **Forms already covered behaviorally**: variant, optional, pointer, bounded
   array, unique/shared handles, array handles. The shared and unique variant
   wrapper values are behavior-tested, so both `variant_value` rows are dropped;
   the alternative reference and pointer rows stay because the model does not
   execute them. Keep the remaining type-variety rows (nested wrappers, custom
   wrappers, interfaces) that the model does not express.
4. **Container variants** (indexed, indexed DSL, allocator, custom RTTI).
   Replace the full container cross product with one representative registration
   recipe per container and feature. Container mechanics are orthogonal to the
   stored form, so multiplying every form by every container adds instantiations
   without adding a new failure mode.
5. **Interface and annotated exposure**. Behavior cases cover shared interface
   references, pointers, shared handles, unique owning handles, multiple
   interfaces on one binding, and a copyable interface value. Interface route
   probes instantiate the real resolution kernel, assert exact
   value/object/rvalue route counts, and require every published target to be
   reachable from both request matchers with satisfied scope access. The matrix
   interface surface is therefore fully model-covered: `resolve_interface` and
   `resolve_unique_wrapper` produce no executables, and their exposure axis
   members are declared model-covered in the matrix manifest. Annotation recipes
   now cover shared annotated concrete values (plain, shared pointer, optional,
   pointer storage), annotated interface references, pointers, shared handles,
   and handle references, and external annotated value, reference, pointer, and
   shared-handle registrations. Negative cases refuse unannotated and wrong-tag
   requests and reject duplicate annotated registration. The matrix annotation
   rows are fully model-covered, so `annotated` produces no executable.
6. **Keys, collections, local bindings.** Add model axes only if the generated
   product stays compact; otherwise keep them in the matrix.

Do **not** migrate constructor detection, invocation and dependency
provisioning, dependency compositions, shared-cyclical graphs, or the imperative
parent/runtime regression scenarios. They describe behavior that is not a
function of a scope/form pair.

Each step should land with a coverage manifest: the model declares the forms it
covers, the matrix declares the registration rows it still owns, and a test
fails when the union no longer covers the catalog. That makes the removal
incremental and safe. `model_coverage.py` was that manifest while the matrix
existed: `FEATURE_RESTRICTIONS` lists the dropped rows with the model forms that
justify them, and the generator test fails if any claimed form loses its
behavior class.

Behavior copy and move counts are not hand-written per form. Each behavior case
declares a request class, and the generator derives the minimum count from it:
one copy per borrowed value resolution, one move per consumed value resolution,
one library move plus one caller move for a consumed rvalue reference, and no
tracked transfers for references, owning handles, and arrays. Only storage-shape
construction counts and the external registration move stay explicit. A consumed
raw pointer or row pointer transfers ownership to the caller, so its behavior
case deletes the result and checks the lifetime balance.

The behavior rvalue case pins the construction count, the move count, and the
lifetime balance. The static container reaches the same move count as the
runtime container because a static binding now materializes a consuming source
in its context, exactly like the runtime binding, instead of keeping a local
source and copying the value into a second context slot. The static execution
graph accounts for that source wrapper in its temporary slot, size, and
destructor bounds; the wrapper stays trivially destructible when the value does,
so those bounds do not grow for trivial types.

## Cost Guarantees

The design adds no abstraction that survives into the generated code as runtime
work.

### Binary Size

- The static path instantiates `binding_core` directly; it does not inherit the
  erased interface, does not carry a vtable, and does not materialize an erased
  request.
- The runtime path keeps the existing single thunk. The shared kernel is inlined
  into that thunk exactly as the current resolver is.
- Generated tables are `constexpr` data used only during compilation. They do
  not become runtime tables or object-file data.
- Selection over static candidates is constant-folded; ambiguity and presence
  checks add no runtime branch.
- The existing code-size probes (`nm` symbol sizes and `objdump` instruction
  probes) become the hard gate. A new probe is added only when a new operation
  is added, and its budget is recorded with the operation.

### Compile Time

- The generator emits data plus one generic consumer, not one class per form.
  Header parse cost therefore grows with the number of operations, not the
  product of forms.

- Static selection stays type-list based and is not replaced by a runtime
  interpreter.

- Kernels remain lazily instantiated: a request instantiates the route it uses,
  not every route that a binding could publish.

- Shape-keyed computations stay shape-keyed (`storage_shape` already does this),
  so bindings that differ only in factory or scope tag share instantiation.

- The compile-cost gate counts instantiations from `-ftime-trace`, not wall
  time. The checks are:

  - no request kernel is instantiated for a registration that is never resolved;
  - route derivation instantiations grow with distinct shapes, not bindings;
  - a static request instantiates no runtime-only selection code;
  - a runtime container instantiates no static-only graph code;
  - the generated headers stay within their include-cost budget.

### Runtime Cost

- Static resolution performs no lookup and no indirect call; it becomes the same
  direct kernel call as today.
- Runtime resolution keeps one memoized selection per request type and at most
  one indirect call, as today. The shared selection algebra must not add a
  second lookup or a second cache.
- Object sizes are checked against a contract: `container<>` and
  `static_container` stay independent of the registration count; static binding
  state lives outside the container instance.
- Existing benchmarks and allocation-counting tests remain the gate; the first
  implementation step must not change their results.

## Migration Plan

Every step is a normal change on the main branch, not a long-lived rewrite
branch. Each step keeps the public API and the observable behavior, and each
step ends with the full quality gate plus the cost comparison.

### Step 0: Record The Baseline

Status: model outcome baseline committed; cost probes record the current values
for per-change comparison.

Build a small cost harness that records, for `master`:

- object sizes of representative containers;
- code-size probe symbol sizes and instruction counts;
- per-header preprocessed size and `includes-compile` time trend;
- instantiation counts from Clang `-ftime-trace` for a few scaling programs;
- benchmark results and allocation counts.
- the matrix/behavior outcome baseline, updated only with a declared reason.

The harness is only useful if it is stable. Gate on deterministic counts and
sizes; track wall time as a trend, not a hard failure.

### Step 1: Unify Selection

Status: in progress. The shared selection, combined-policy, missing, collection,
context dependency-resolution, and public collection-spelling helpers are in
use. The container-specific source adapters are still separate and are the next
removal.

Introduce the candidate-set vocabulary and move the selection rules from
`container.h`, `static_container.h`, `runtime/registry.h`, and
`static/binding.h` into `binding/binding_selection.h`. Keep the existing binding
implementations. This is a pure behavior-preserving refactor and should remove
more code than it adds.

### Step 2: Unify The Binding Kernel

Extract `binding_core` from `runtime_binding` and make `static_binding_resolver`
call it through the static state policy. Keep the runtime erasure adapter
separate. Delete the duplicated static materialization and cache ladder.

### Step 3: Generate Storage And Route Tables

Status: in progress. The storage scope policies are generated into
`include/dingo/storage/scopes.gen.h`; the storage scope/form pairs and selection
policies are generated as coverage probes. The remaining route forms and the
matrix consumption still need to move to the catalog.

Move the built-in `(scope, form)` and route forms into the catalog. Emit the
tables and coverage assertions, then replace the per-form specializations one
group at a time:

1. scopes × plain values;
2. handles (`unique`, `shared`) and their arrays;
3. alternatives and optionals;
4. custom conversions and retention;
5. arrays and bounded arrays.

Each group lands with its generated tests and its cost comparison.

### Step 4: Generate Matrix Inputs

Make the matrix consume the catalog directly for scopes, forms, routes, modes,
and capability tags. Delete the hand-curated approximations. Keep the
hand-written C++ fixtures and policies; they are test code, not duplicated
semantics.

### Step 5: Delete The Duplicated Backends

Once selection, the binding kernel, and the tables are shared, the remaining
runtime/static code is only state and dispatch. Collapse the duplicated
registry/context/activation helpers into those adapters. The `runtime/` and
`static/` directories should then contain state and dispatch only, not rules.

### Rollback

Each step is mergeable on its own. The only temporary switch is an internal
implementation macro used by a step's own tests; it is removed before the next
step, so the tree never contains two supported implementations.

## Rejected Alternatives

- **Mirror only.** Keeps two decision chains and adds files. Rejected.
- **One class with a virtual backend for both domains.** Adds a vtable, an
  erased request, and dynamic dispatch to the static path. Rejected on size and
  cost grounds.
- **A runtime interpreter over generated tables.** Solves the static/runtime
  split by making both slow. Rejected.
- **Generate one specialization per form.** Header and compile cost grow with
  the product; it also multiplies the effect of a template bug. Rejected in
  favor of data plus one consumer.
- **Replace the library with a second implementation and diff it.** It creates a
  second library to maintain and defers the hard integration until the end.
  Rejected in favor of incremental, tested steps.
- **Require C++20 concepts for the source vocabulary.** Dingo supports C++17.
  Use `if constexpr` and detection idioms in the adapters; concepts can be an
  optional readability layer later.

## Open Decisions

- The exact home of the catalog: extend `test/matrix/axes` into a shared
  `tools/model/`, or keep the matrix model and add a library-facing projection.
  The design prefers the former because two catalogs will drift.
- Whether a runtime key value over a static table is part of the first pass or a
  declared follow-up. The selection algebra makes it a source property, not a
  separate backend.
- Whether cycle detection can share one graph representation. The compile-time
  graph has closed edges; the runtime guard has dynamic edges. The shared part
  is the rule ("a cycle is legal only when every node is `shared_cyclical`"),
  not the traversal.
- The exact contract values for header cost and code size. They start at the
  `master` baseline and are tightened only after the structure is shared.

## Acceptance For The First Change

The design is working when the first step lands with:

- no change to public headers or behavior;
- fewer lines in the container and registry selection paths;
- tracked object size, code-size probe results, instantiation counts, and
  benchmark results, with any regression recorded rather than hidden;
- a `generate-check` and `generated-check` target that fail when a declared form
  has no implementation or coverage.

The first slice records cost through `tools/cost/measure.py`. Closing a
compile-time regression is an explicit follow-up optimization; it does not block
the structural change, but it stays visible in the cost records.
