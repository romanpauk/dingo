# Compile-Time Design

Dingo is a header-only library, so its internal structure directly affects the
amount of work repeated in every consumer translation unit. Compile-time
performance is therefore an architectural constraint, not only a local
metaprogramming concern.

A useful approximation is:

```text
compile cost = semantic shapes x eagerly instantiated branches x work per branch
```

The largest compile-time improvements have reduced one or more of those factors.
They have come from changing which template owns expensive work, not from
replacing one expensive trait with a marginally faster equivalent.

## Preferred Pipeline

Template-heavy code should follow this pipeline where possible:

```text
concrete user types
    -> cheap shape or tag classification
    -> lazy selection of one semantic recipe
    -> recipe rebound to concrete leaf types
    -> thin typed adapter
    -> canonical or type-erased runtime implementation
```

Customization is resolved at the typed edge. The reusable middle of the pipeline
should depend only on properties that change its behavior.

## Template Identity Follows Behavior

Every template parameter creates another dimension of possible specializations.
A parameter should remain on an expensive template only when it affects at least
one of:

- object layout
- overload or specialization selection
- generated behavior
- a required compile-time result

Routing-only information should be removed before entering the expensive
implementation. Examples include lookup keys that only choose an index, parent
types passed through as opaque pointers, factory implementation types that do
not affect resolution, and callback closure types that only invoke shared setup.

The preferred shape is a small typed adapter around a narrower implementation:

```cpp
template <typename Request>
auto resolve(Request request) {
  return resolve_erased(describe_type<Request>(), make_erased_request(request));
}
```

The erased implementation can still call a canonical thunk when type-specific
behavior is required. Erasure is useful at the boundary between type selection
and runtime execution; it should not discard information still needed to make
the selection.

## Instantiate Branches Lazily

`std::conditional_t` selects one result type, but its template arguments must
first be formed. This is eager when a branch names an expensive nested result:

```cpp
using result = std::conditional_t<condition, first,
                                  typename expensive_tail<T>::type>;
```

Use a boolean partial specialization when the unselected branch must remain
uninstantiated:

```cpp
template <bool Selected, typename T> struct select_result;

template <typename T> struct select_result<true, T> {
  using type = first;
};

template <typename T> struct select_result<false, T> {
  using type = typename expensive_tail<T>::type;
};
```

The same rule applies to recursive searches. Once a constructor, lookup entry,
conversion, or missing dependency has been found, later candidates should not be
named. `if constexpr` is suitable inside functions and constant-evaluation
lambdas; partial specialization is usually clearer for type-producing code.

## Classify Before Probing

Expensive traits should be reached through a cheap necessary-condition test.
Useful classifiers include:

- value, reference, pointer, array, wrapper, or alternative shape
- lvalue or rvalue source category
- default or user-provided conversion traits
- borrow or consume access
- empty, singular, or plural cardinality

The classifier must not claim that a conversion is valid. It only chooses the
first candidate that could be valid; the selected candidate still performs its
complete semantic check.

Keep classifiers small and specific. Additional dispatch layers are not free,
and a generalized classifier can cost more to parse and instantiate than the
work it skips.

## Normalize to Semantic Shapes

When behavior depends on wrapper or storage shape rather than concrete leaf
types, calculate it once with a representative leaf and materialize the recipe
for the real type afterward:

```text
shared_ptr<service_a> --\
shared_ptr<service_b> ----> pointer-like borrowed shape -> shared recipe
user_handle<service_c> --/                                  -> rebind leaf
```

Normalization must be driven by extension traits such as `type_traits`,
`alternative_type_traits`, and `rebind_leaf_t`. Do not hard-code standard
library wrappers: user-defined pointer-like, owning, optional, and alternative
types must be able to share the same machinery.

Use an explicit eligibility gate before naming the normalized path. If the
recipe cannot be materialized without changing custom behavior, use the exact
type path. Unsupported types should not pay for both normalized and exact
resolution.

## Keep Cold Work Shared

Registration, exception formatting, rollback setup, and transaction setup are
cold runtime operations but can be expensive to compile. Move their common parts
behind non-template or canonical functions and keep large cold functions out of
callers with `DINGO_NOINLINE` where appropriate.

A typed edge should generally do only the following:

1. Validate type-specific requirements.
2. Produce descriptors, recipes, or canonical thunks.
3. Invoke the shared implementation.
4. Adapt the erased result to the requested C++ type.

Reusable named actions are preferable to unique lambdas when the action is
passed into templated transaction or rollback machinery. The callback type
should not own generic frame setup or teardown.

## Avoid Generic Work for Known Cases

Prefer direct implementations for common cardinalities and known shapes:

- empty lists produce an empty result directly
- singular lists avoid concatenation and deduplication
- disjoint result categories are concatenated without another uniqueness pass
- common default conversions bypass recursive fallback selection
- types with a declared construction shape bypass generic constructor probing

Likewise, use a direct language expression when it answers a narrower question
than a general-purpose standard trait. For example, `noexcept(fn())` can be
cheaper and more precise than generic invocability machinery when invocation
validity is already established.

## Materialize Once and Keep the Call Path Flat

A resolution request should select its route and materialize its storage source
once. Do not repeat source extraction for value, reference, pointer, or
conversion branches when the request descriptor can select among them after a
single materialization.

Avoid forwarding ladders and nested generic lambdas in hot template paths.
Inlining a trivial adapter can remove one specialization, but a large cold
operation should remain behind a shared boundary. Optimize template ownership
separately from runtime call count and generated code size.

## Preserve Semantics Before Reducing Probes

Constructor and structural-conversion probes encode observable compatibility. Do
not reduce their depth, remove placeholder conversions, or predefine wrapper
families merely to reduce compilation. Such changes can break aggregates,
optional and alternative wrappers, move-only values, or user-defined handles.

Prefer opt-in known-shape traits and necessary-condition dispatch. The generic
path remains the compatibility fallback.

## Review Checklist

For a new or modified template, check:

- Does every template parameter affect this specialization's behavior?
- Does an unselected branch name a recursive or otherwise expensive result?
- Can a cheap shape or cardinality check avoid the expensive trait?
- Can equivalent concrete types share a canonical recipe?
- Is normalization driven by public extension traits and guarded by an exact
  fallback?
- Is a unique lambda causing shared setup to be instantiated repeatedly?
- Are list concatenation or uniqueness operations redundant for this
  cardinality?
- Can cold formatting, transaction, or registration work move behind a shared
  boundary?
- Was source materialization or route selection repeated?
- Did the change improve measured frontend time across representative inputs?

Fewer template events do not guarantee a faster compiler frontend. Validate
structural changes with the profiling procedure in
[Development](../development.md#compile-time-profiling), and retain a change
only when repeated measurements show a useful improvement without changing
supported semantics.
