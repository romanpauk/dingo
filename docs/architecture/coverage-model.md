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

## Operator classes

The per-cell claim files answer "can this row be consumed". They do not answer
"which constructor shapes does the operator support". Those are operator
classes, they are not tied to a cell, and they live at:

```text
resolution/runtime/global/construct/<shape>/<traits>.cpp
```

with one canonical storage per shape (shared/plain for references, pointers and
values; unique/plain for rvalue and move-only; the handle form for handles). The
same split will apply to `invoke`: per-cell claim files for breadth, a small
shape family for meaning.

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
