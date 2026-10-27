# Coverage Model

The space is large because the matrix multiplies independent concerns. The model
should factor them instead of reproducing the matrix product.

## Operations

| operation | target | what it adds | what it reuses | | --- | --- | --- | --- |
| `resolve` | a registered binding | storage-to-request routes, access, const,
wrappers, identity | — | | `construct` | an unregistered target | constructor
detection/selection, target construction, target lifetime | `resolve` of each
constructor argument | | `invoke` | a callable | callable detection/signature,
argument dispatch, return handling | `resolve` of each callable argument | |
`resolve_collection` | several bindings | multi-binding enumeration, collection
assembly | element bindings | | `construct_collection` | an unregistered
collection | collection construction and insertion policy | element
`construct`/`resolve` | | `nested_container` | parent/child containers |
container hierarchy and scope boundaries | `resolve` in each container | |
`mixed` | static + runtime registrations | mixed registration placement |
`resolve` and `register` | | `shared_cyclical` | a storage graph | cycle edges
and node materialization | `resolve` at graph nodes | | `dependency_composition`
| a recursive type composition | type-level composition and projection |
`construct`/`invoke`/`resolve` |

Construct and invoke are **not** alternative coverage for resolve. They are
compositions over resolve. A construct test proves that a dependency was
resolved and fed into a constructor; it does not re-prove the storage route.

## Factorization rules

Do not build:

```text
operator x every storage cell x every request shape x every binding
```

That is the matrix's over-approximation. Factor into:

1. **Atom coverage** — `resolve` over
   `(scope, storage, binding, request shape, traits, container configuration)`.
2. **Operator-specific axes**:
   - construct: target constructor shape (arity, argument categories, aggregate,
     detected/typedef/selected/annotated);
   - invoke: callable shape (signature, return, overload selection);
   - collection: collection type, key/index mode, element binding;
   - nested/mixed/cyclical: composition shape.
3. **Dependency categories** — value, lvalue reference, const lvalue reference,
   rvalue reference, pointer, const pointer, selected, annotated,
   optional/handle.

For an operator test, use one canonical storage per dependency category and
reuse the atom coverage for the storage. The operator test asserts the
composition, not the storage route.

## Claims over one row set

A row is a capability:
`(configuration, scope, storage, binding, request, traits)`. `resolve` is the
first claim about that row. `construct` and `invoke` are claims about the same
row through a different entry point, so they are rendered from the same manifest
instead of enumerating storage again:

```text
resolution/runtime/<scope>/<storage>/resolve.cpp     # claim: resolve
resolution/runtime/<scope>/<storage>/construct.cpp   # same rows
resolution/runtime/<scope>/<storage>/invoke.cpp      # same rows
```

The case path keeps the cell and swaps only the claim level:

```text
resolution/runtime/<scope>/<storage>/<claim>/<traits>/<binding>/<request>
```

One manifest feeds every claim file, so the row sets cannot drift. A claim file
holds no per-row knowledge: one generic target template consumes any request
type,

```cpp
template <typename Argument> struct probe_target {
  explicit probe_target(Argument) : constructed(true) {}
  bool constructed;
};
```

and `invoke([](Argument) { ... })` does the same for callables.

## What the claims already showed

The first run produced three findings that the resolve suite could not express:

| rows | resolve | construct | invoke | reason | | --- | --- | --- | --- | --- |
| 80 | ok | compile gap | ok | constructor parameter of `std::optional<...>` is
ambiguous in the argument adaptor | | 2 | ok | crash | ok | rvalue reference
into a unique bounded array is destroyed twice | | 257 | ok | skip | skip |
keyed rows need a lookup key that neither operator can supply |

Only the last class is a property of the operators. The first two are library
defects that were invisible while `construct` re-tested storage instead of
composing over it.

The 82 construct rows that cannot be instantiated are pinned by path in
`tools/model/emit/consumers.py`, with a reason, so a new gap cannot be absorbed
silently by a rule.

## The claim report

`cmake --build build -t claim-check` runs the resolve probe, the runtime claim
tests, and `tools/model/report/claims.py`, then writes
`build/test/claim-gaps.md`. Every `(row, claim)` pair is classified as:

| state | meaning | | --- | --- | | `ok` | resolve resolves the row and the
claim consumes it | | `declared` | the claim cannot consume the row for a
declared reason (keyed rows need a key) | | `shared gap` | resolve already skips
the row, so the claim adds nothing | | `recovered` | a declared reason no longer
applies; the declaration is stale | | `diverged` | resolve resolves the row but
the claim cannot consume it | | `missing` | the row was emitted without an
outcome line | | `pinned` | the claim file cannot contain the row; kept honest
by a negative check |

`diverged` and `missing` fail the target. Pinned rows are checked in both
directions: a compile pin must still fail to compile, and a runtime pin must
still fail instead of passing. A fixed bug therefore turns into a stale pin
instead of staying excluded silently.

The current report:

| claim | rows | ok | declared | shared gap | pinned | | --- | ---: | ---: |
---: | ---: | ---: | | `construct` | 2,009 | 899 | 257 | 771 | 82 | | `invoke` |
2,009 | 952 | 257 | 800 | 0 |

## Operator classes

The per-cell claim files answer "can this row be consumed". They do not answer
"which constructor shapes does the operator support". Those are operator
classes, they are not tied to a cell, and they live at:

```text
resolution/runtime/global/construct/<shape>/<traits>.cpp
```

with one canonical storage per shape (shared/plain for references, pointers and
values; unique/plain for rvalue and move-only; the handle form for handles).

`invoke` uses the same split at `resolution/runtime/global/invoke/<shape>`: no
arguments; lvalue, const-lvalue, value, rvalue, pointer, const-pointer and
shared-handle arguments; two arguments; `void` return; `std::function`, function
pointer and functor forms; explicit and overloaded signature selection;
move-only and reference returns; and a missing dependency that must throw. An
overloaded callable without an explicit signature is a declared gap.

## Collections

A collection is several bindings assembled into one value, so the mechanism has
three degrees of freedom: registration mode (static, or static plus runtime
merged), keyed or unkeyed, and collection shape (vector, or a map built with an
insert policy). `resolution/<mode>/global/collection.cpp` covers all of them in
five cases per mode. Element storage and request form are row probes, not
dimensions here.

The mixed cases are the interesting ones: one element is registered statically
and one at runtime, and the assembled collection must contain both.

## Lookup scope

Local bindings and nested containers are the same rule set — where a binding is
found — so they share one family at `resolution/runtime/global/lookup/<case>`
with five cases: a local binding is visible to its registration; it shadows a
root binding of the same type; a child inherits an unshadowed parent binding; a
child shadows a parent binding without changing the parent; a grandchild
inherits through the chain.

## Configurations

A configuration swaps a container trait under the same rows, so it has no
degrees of freedom beyond the trait and the row it is proved on:

```text
resolution/runtime/allocator/global/resolve.cpp
resolution/runtime/rtti/global/resolve.cpp
resolution/runtime/indexed/global/<key>/resolve.cpp
```

The allocator case asserts that bookkeeping allocation happened and that the row
still resolves; the RTTI case asserts the configured `rtti_type` and the same
row; the indexed cases assert lookup by key.
`test/support/model_configuration.h` owns the traits so the probes do not depend
on the matrix headers.

## Cycles

A cycle is two slots that each need the other, so the container publishes both
before filling them. `resolution/<mode>/global/cyclical.cpp` covers the two
storage representations (both nodes by value, both as `shared_ptr`) and the
mixed representation, in the runtime, static, and mixed containers. Each case
asserts that both edges point at the same instance: `a.peer == &b` and
`b.peer == &a`. The edge request form is the atom request space.

## Co-residence

A mixed container holds compile-time bindings and runtime registrations at the
same time. There are exactly two outcomes, so the family is two cases at
`resolution/mixed/global/`:

- `external_dependency` — a static binding resolves a dependency that only
  exists at runtime;
- `singular_ambiguity` — the same type exists on both sides, and a singular
  request throws `type_ambiguous_exception`.

## Producers

The producer family answers "does the capability survive a change of factory" at
`resolution/runtime/global/producer/<cell>`:

- `shared/plain` with detected construction, explicit `constructor<...>`,
  `function<...>`, `callable(...)`, `callable<Signature>(...)`, a
  `DINGO_CONSTRUCTOR` type, and an overloaded callable selected by signature;
- `shared/pointer`, `shared/shared_handle`, `unique/unique_handle`,
  `shared/alternative`, and `shared/optional` with a function factory and a
  callable factory.

Each producer gets the same three claims over the same dependency: `resolve`,
`construct`, and `invoke`, asserting the same value. A producer that quietly
changes the result fails. `unique_handle` is the one cell whose request is the
handle itself, because a unique scope cannot lend an lvalue.

## Completeness

Coverage is complete when:

1. every atom class is classified (supported, unavailable, unsupported,
   unmodeled) and every supported atom has a test;
2. every operator-specific class is classified and tested or declared
   unsupported with a reason;
3. every dependency category used by an operator maps to an atom class;
4. composition rules are validated (construct uses resolve, invoke uses resolve,
   collection uses element bindings, etc.);
5. the report shows atom coverage and operator coverage separately.

Coverage is **not** complete when every matrix row is reproduced. The matrix is
a cross-product of these axes; the model's claim is a factorization plus a
finite set of operator classes.

## Consequences for the migration

- `construct` is now split into the per-cell claim files and the
  `global/construct/<shape>` operator classes.
- `factory_override` is a registration operator: test factory forms x argument
  categories, not every storage.
- `invoke` has per-cell claim files; its callable-shape family is still to be
  added.
- collections test collection mode x element binding x container, not every
  element request.
- scenario/cyclical/composition families test composition shapes, not storage
  cross-products.
- A matrix feature can be deleted when its **operator classes** are covered,
  even if its row count is larger than the model count. The matrix row count is
  not the completeness criterion.
