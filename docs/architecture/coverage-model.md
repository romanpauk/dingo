# Coverage Model

The space is large because the matrix multiplies independent concerns. The
model should factor them instead of reproducing the matrix product.

## Operations

| operation | target | what it adds | what it reuses |
| --- | --- | --- | --- |
| `resolve` | a registered binding | storage-to-request routes, access, const, wrappers, identity | — |
| `construct` | an unregistered target | constructor detection/selection, target construction, target lifetime | `resolve` of each constructor argument |
| `invoke` | a callable | callable detection/signature, argument dispatch, return handling | `resolve` of each callable argument |
| `resolve_collection` | several bindings | multi-binding enumeration, collection assembly | element bindings |
| `construct_collection` | an unregistered collection | collection construction and insertion policy | element `construct`/`resolve` |
| `nested_container` | parent/child containers | container hierarchy and scope boundaries | `resolve` in each container |
| `mixed` | static + runtime registrations | mixed registration placement | `resolve` and `register` |
| `shared_cyclical` | a storage graph | cycle edges and node materialization | `resolve` at graph nodes |
| `dependency_composition` | a recursive type composition | type-level composition and projection | `construct`/`invoke`/`resolve` |

Construct and invoke are **not** alternative coverage for resolve. They are
compositions over resolve. A construct test proves that a dependency was
resolved and fed into a constructor; it does not re-prove the storage route.

## Factorization rules

Do not build:

```text
operator x every storage cell x every request shape x every binding
```

That is the matrix's over-approximation. Factor into:

1. **Atom coverage** — `resolve` over `(scope, storage, binding, request
   shape, traits, container configuration)`.
2. **Operator-specific axes**:
   - construct: target constructor shape (arity, argument categories,
     aggregate, detected/typedef/selected/annotated);
   - invoke: callable shape (signature, return, overload selection);
   - collection: collection type, key/index mode, element binding;
   - nested/mixed/cyclical: composition shape.
3. **Dependency categories** — value, lvalue reference, const lvalue
   reference, rvalue reference, pointer, const pointer, selected, annotated,
   optional/handle.

For an operator test, use one canonical storage per dependency category and
reuse the atom coverage for the storage. The operator test asserts the
composition, not the storage route.

## Why current construct coverage is misleading

The current `construct` family registers a dependency in every supported
storage cell and constructs one reference-taking target. That:

- over-covers storage: the same `resolve(dependency &)` behavior is already
  covered by `resolve`;
- under-covers target shapes: only one constructor shape is exercised;
- cannot answer whether every constructor shape works with every dependency
  category.

The corrected shape is:

```text
resolution/runtime/<configuration>/global/construct/<target>/<argument-category>/<traits>.cpp
```

with canonical storages per argument category:

- value: `shared/plain`
- lvalue reference: `shared/plain`
- const lvalue reference: `shared/plain`
- rvalue reference: `unique/plain`
- pointer: `shared/pointer`
- optional: `shared/optional`
- handle: `shared/shared_handle`

The same principle applies to invoke, factory, collections, and nested/mixed
composition.

## Completeness

Coverage is complete when:

1. every atom class is classified (supported, unavailable, unsupported,
   unmodeled) and every supported atom has a test;
2. every operator-specific class is classified and tested or declared
   unsupported with a reason;
3. every dependency category used by an operator maps to an atom class;
4. composition rules are validated (construct uses resolve, invoke uses
   resolve, collection uses element bindings, etc.);
5. the report shows atom coverage and operator coverage separately.

Coverage is **not** complete when every matrix row is reproduced. The matrix is
a cross-product of these axes; the model's claim is a factorization plus a
finite set of operator classes.

## Consequences for the migration

- `construct` should be refactored from per-storage to target-shape x argument
  category, with canonical storages.
- `factory_override` is a registration operator: test factory forms x argument
  categories, not every storage.
- `invoke` is a callable operator: test callable shapes x argument categories.
- collections test collection mode x element binding x container, not every
  element request.
- scenario/cyclical/composition families test composition shapes, not storage
  cross-products.
- A matrix feature can be deleted when its **operator classes** are covered,
  even if its row count is larger than the model count. The matrix row count is
  not the completeness criterion.
