# Container Engine

Status: design note, slices 1-4 applied. The operations layer is complete.
`container_operations<Self, RuntimeSource>` owns the no-scope and dependency
`resolve` spellings, `construct`, `invoke`, the collection entries, and the
parent/miss hand-off. The id/key form serves runtime-backed containers, the
compile-time key form serves static-only containers, and the static
minimal-context and construct rules live beside the runtime ones. Only the
state/move policy remains.

[Shared-Kernel Next Steps](shared-kernel-next-steps.md) records the long-range
plan for one container engine. This page narrows the next change: what the
decision function is today, what is still duplicated, and the shape of the
commit that removes the duplication without adding a template level per
dependency link.

## What Already Exists

The source protocol is named on both backends:

- `static_registry`: `resolve_named`, `collect`, `fixed_count`
- the runtime store: `resolve_named`, `collect`, `count`

The singular and collection decision function is
[binding/container_selection.h](../../include/dingo/binding/container_selection.h).
It owns the order of sources, the static/runtime combination, ambiguity, the
parent hand-off, and the miss call. Both `runtime_container_impl` and
`static_container_impl` derive from it. The miss rule is
[binding/container_miss.h](../../include/dingo/binding/container_miss.h), and
the collection spellings are
[container_collections.h](../../include/dingo/container_collections.h).

So the plan's "one decision function" is not missing. What remains is that the
static-only front-end does not always go through it, and the mixed front-end
keeps a second construct decision beside it.

## What Is Still Duplicated

[static_container.h](../../include/dingo/static_container.h) repeats outer
operations that [container.h](../../include/dingo/container.h) already has:

- a public `resolve` with a minimal-context fast path for direct stored
  reference/pointer reads, and a general path through `container_selection`;
- a `construct` branch tree beside the mixed one;
- collection relays and the parent/miss wrappers;
- the move-deleted state policy.

[container.h](../../include/dingo/container.h) itself keeps two decisions:

- `container_selection` for singular and collection resolution;
- `construct`/`construct_mixed_request`/`construct_rvalue_request` for
  construction, including the static-only, runtime-only, and mixed cases.

The two front-ends agree on behavior but not on the code that expresses it.

## Target Shape

One operations layer with the decision order in one place, and per-mode facts as
constants:

```text
container_operations<Self, RuntimeSource>
  resolve(...)            -> source order, then the selected source
  construct(...)          -> the construct variant of the same order
  invoke(...)
  collection entry points

Self provides:
  has_static_source, has_runtime_source
  static_source(), runtime_source(), parent()
  static_ambiguity            (compile_error | ambiguous)
  allow_auto_construct        (bool)
  empty_collection            (compile_error | empty)
```

`static_container_impl` becomes a configuration with
`has_runtime_source = false`, `allow_auto_construct = false`, and
`static_ambiguity = compile_error`. The runtime-only container is the same
configuration with `has_static_source = false`. The mixed container is both
sources true.

The minimal-context fast path in `static_container` must move into the
operations layer as a policy decision, not stay a separate public `resolve`. If
it cannot move without a wrapper, it is not a configuration difference and the
target is wrong.

## Why Not A Wrapper

The static-only container has three dependency links of headroom on GCC; the
mixed container has more. Every additional function-template level per link
consumes that budget, and a wrapper around the current entry point adds one. The
operations layer must *replace* the static resolve entry, not call it. The
canaries are:

- `test/container/dependency_chain.cpp` (45 links static, 48 mixed on GCC);
- `test/registration/object_sizes.cpp` (state and binding sizes);
- the generated static and mixed resolution suites.

## Slices

1. **Name the operations layer without moving behavior.** Add
   `container_operations<Self, RuntimeSource>` and move `resolve` into it; make
   both fronts derive from it and remove the old public entries in the same
   commit. *(Applied for both no-scope `resolve` entries; `construct` remains.)*
   Gate: depth chains, object sizes, full suite.
2. **Move the minimal-context decision into the layer.** Replace the static-only
   public `resolve` fast path with a policy query the layer uses, so there is
   one entry. *(Applied: the minimal-context decision moved with the static
   resolve body.)* Gate: static codegen probe and object sizes.
3. **Move the construct tree.** Fold `construct`, `construct_mixed_request`, and
   `construct_rvalue_request` into the layer's construct variant, with the
   static/runtime/mixed differences as the constants above. *(Applied: both
   construction trees moved.)* Gate: behavior baseline, full suite, depth
   chains.
4. **Move invoke and the collection entry points.** They are the remaining
   public operations; their bodies are small, but moving them last keeps each
   slice reviewable. *(Applied: invoke and the collection entries moved.)*
5. **State and move policy.** Decide `runtime_container`'s fate separately:
   either an alias of `container` or a thin owner, with the stable-address
   contract preserved. Re-pin `object_sizes.cpp`.

Slices 1-4 change no behavior. Slice 5 is a state-model change and needs its own
baseline note.

## Risks

- **Depth.** The only hard failure mode. Run the dependency-chain test after
  every slice, on GCC and Clang.
- **Fast-path parity.** The static minimal-context path exists to keep the
  static codegen contract small. Moving it must preserve the generated symbol
  sizes, not just the runtime behavior.
- **Diagnostics.** The static `static_assert` wording is pinned by the literal
  tests. Policy constants must not flatten it into a generic message.
- **Move semantics.** `runtime_container` moves by ownership transfer and
  `container<bindings<...>>` is immovable by contract. A shared operations layer
  must not change either.

## Non-Goals

- No new source protocol names; that step is done.
- No runtime loop over source rules.
- No generated rule table on the static path.
- No behavior change hidden in the move. Any asymmetry becomes a policy
  constant, not a branch in the layer.
