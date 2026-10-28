# Conversion Path

Status: design note, slice 1 applied. No behavior is changed by this page.

Slice 1 added `detail::conversion_properties` in
[type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h):
`produces_object_address`, `identity_copy`, `available`, `kind`, and
`required_access` are now read from the selected conversion.
[object_routes.cpp](../../test/registration/object_routes.cpp) uses the trait
for its shared-route soundness checks across the plain, pointer, wrapper,
alternative, array, and custom-wrapper matrix, replacing the local copies of the
same two predicates. Slices 2 and 3 below remain.

[Conversion Model](conversion-model.md) describes how a request is selected and
executed today. This page records the one structural duplication left in that
pipeline and the target shape for removing it: the selected conversion already
knows its route, but storage pairing decides the same thing again from the type
traits.

## Current Shape

Three artifacts describe a route:

- **Selection**
  ([type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h))
  builds a conversion tree for a target, source, and access. The tree root is
  one of `identity`, `reference`, `address`, `array_reference`, `array_pointer`,
  `pointer_access`, `dereference`, `borrowed`, `retained`, `wrapper`,
  `target_alternative`, `alternative`, `traits`, or `construction`, and nested
  conversions carry the rest of the path.
- **Execution**
  ([resolution_operation.h](../../include/dingo/resolution/resolution_operation.h))
  maps that tree to a flat `conversion_descriptor`: ordered steps, cache types,
  and source-retention requirements. `conversion_walk` executes the steps. This
  is already single-source: the descriptor is derived from the selected tree,
  and the walker does not classify target and source again.
- **Storage pairing**
  ([type_storage_traits.h](../../include/dingo/storage/type_storage_traits.h))
  decides which published request shapes name the same object. It has its own
  predicates: `is_object_address_candidate_v`, `is_default_object_route`,
  `is_default_object_request`, `is_value_copy_candidate_v`,
  `is_default_value_route`, and `is_default_value_request`.

The duplication is in the third artifact. Pairing reconstructs a decision the
selected conversion already made, then reconstructs the conversion itself:

- `pair_object_requests` asks `is_default_object_request` whether a pointer
  request shares its reference request.
- `shared_object_resolutions` then builds the route with
  `default_object_conversion_t`, which re-selects `address_type_conversion` or
  `traits_type_conversion` from the source category.

So a new route kind, or a change to the default object/copy rule, has to be
taught to both the selection tree and the storage predicates. That is the fourth
encoding the [complexity review](complexity-review.md) records.

## Target

Attach the route properties to the selected conversion tree, and let storage ask
the tree instead of re-deriving:

```text
conversion tree (type/) -> properties + steps (resolution/)
                                  |
                     storage pairing reads properties
```

The properties belong to the conversion, not to the storage:

- `available` — the path can serve the request;
- `kind` — the root `conversion_kind`, for diagnostics and tests;
- `produces_object_address` — the path yields the address of an existing object
  (today's "default object route");
- `identity_copy` — the path copies the stored object as itself (today's
  "default value route");
- `required_access` — `borrow` or `consume`, already on the tree;
- `retains_source` — the path keeps a retained frame, already on the descriptor;
- `cache_types` and `temporary_types<Storage>` — already on the descriptor.

`produces_object_address` and `identity_copy` are the two facts storage pairing
needs. They are derived once, from the root conversion and the default-ness of
its leaf, in the same place that builds the tree.

### Shape

The tree keeps its current form. A property trait beside it answers the
questions storage asks:

```cpp
template <typename Conversion> struct conversion_properties {
  static constexpr bool available = Conversion::available;
  static constexpr conversion_kind kind = Conversion::kind;
  static constexpr bool produces_object_address = false;
  static constexpr bool identity_copy = false;
};

template <typename Target, typename Source>
struct conversion_properties<address_type_conversion<Target, Source>> {
  static constexpr bool produces_object_address = true;
  // identity_copy is true when the target is the exact stored object and the
  // source is not consumed; the value form is then a plain copy.
};
```

The detail is in the specializations: `identity_type_conversion`,
`reference_type_conversion`, `address_type_conversion`, `traits_type_conversion`
with default arguments, and the wrapper/alternative/retained roots that
explicitly do *not* share an object route. A custom `type_conversion_traits`
still produces `traits_type_conversion`, so it participates through the same
property trait without a new extension point.

Pairing then becomes:

```cpp
template <typename Pointer>
static constexpr bool shared_pointer =
    has_reference_request<Pointer> &&
    type_conversion_path_t<Pointer, source, borrow>::properties::
        produces_object_address;
```

and the shared route takes the operation from that same path instead of calling
`default_object_conversion_t`.

## Precedence

The selection order is currently written three ways in one file:

- the `conversion_kind` enum;
- the `conversion_fallback_candidate` specializations;
- the necessary-condition chain in `first_conversion_fallback`.

They are intentionally lazy: a later candidate is not instantiated until every
earlier one has failed. A single ordered rule table would be easier to read, but
it must preserve that laziness. The recommendation is a second slice:

1. Name one ordered list of rules, each with an id, a necessary condition, and a
   candidate builder.
2. Generate the candidate specializations and the condition chain from that
   list, or replace both with a recursive walk over the list that keeps each
   candidate incomplete until needed.
3. Keep the enum as the public property vocabulary; it is read by the operation
   steps and by tests.

This is lower priority than the property trait. The property trait removes a
cross-file sync hazard; the precedence table removes a within-file one.

## Migration Slices

Each slice is one commit. The behavior and type-table baselines must not change.

1. **Add `conversion_properties` and cross-check it.** Compute the new
   properties beside the tree and assert in tests that, for every built-in shape
   and storage form, they agree with the existing storage predicates. No
   behavior change.
2. **Switch pairing to the properties.** Replace `is_default_object_request` and
   `is_default_value_request` with the property queries, and take the shared
   route's conversion from the selected path. Delete the re-derived predicates
   and `default_object_conversion_t` when they have no callers. Gate: type
   table, behavior, object sizes, and the custom wrapper tests.
3. **Collapse the precedence encodings.** Do this only after slice 2 is stable.
   Gate: compile cost and template depth, since the candidate walk is
   depth-sensitive.

Do not flatten the conversion tree into a runtime step list as the only
representation. The tree carries the composition of wrappers, alternatives, and
retained conversions; the flat descriptor is its execution projection.
Collapsing the two would move the same logic into the walker and lose the
compile-time checks that selection provides.

## Relation To The Model

The `spec` model already describes routes as derivations over wrapper semantics
and scope exposure (`spec/model/semantics/routes/rules.py`). The C++ properties
should use the same vocabulary where it is shared:

- `access` and `borrow` map to `required_access`;
- `copy_out` maps to `identity_copy` for a value request;
- `take`, `wrap`, `share`, `adopt`, `select_alternative`, `nested_access`
  describe the composed path rather than a storage pairing fact.

The model remains an oracle for expectations, not the source of truth for
whether the library compiles. The library keeps answering whether a route is
published; the model keeps naming the first rule that blocks it.

## Non-Goals

- No public `type_conversion_traits` change. Custom wrappers keep the same
  `convert`/`required_access` contract.
- No flat runtime conversion graph.
- No generated C++ rule table from `spec/`.
- No behavior change. A route that is unavailable today stays unavailable; only
  the place that decides it moves.
