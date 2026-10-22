# Route Normal Form

This page is a proposal, not a description of the library. It records how the
engine derives routes today, where that reliance on derivation is weak, and a
design that would replace the derivation by a structural match with one indirect
call on the hot path. Nothing here is implemented, and the hypothesis it rests
on is not yet confirmed.

## Derivation Today

[core/engine/derive.h](../../include/dingo/core/engine/derive.h) derives the
terms of a binding when the binding is compiled. `derive` starts from the stored
shape, borrowed as an lvalue or consumed as an rvalue as the scope materializes
it, and applies every form that creates no layer: reach the element, take it,
decay, release, upcast to each declared interface, a declared conversion, and
copy. It repeats until nothing new appears. Each result is a term: a shape, a
value category, and the rules that reached it.

The forms that create a layer, `wrap`, `adopt`, and `address`, are not derived,
since only the request names the kind it wants. They apply at the request, on
top of a term the binding serves, up to `max_created_layers` (2) layers.

The set of terms is closed under the rules, so no list states which requests a
binding serves: a shared `std::unique_ptr<impl>` serves `iface*` because the
rules borrow, upcast, and take the address. A kind of the user is derived the
same way, by the forms it declares. The set is bounded:

- a derived shape is at most one layer larger than the stored one
  (`derivation_bound`), and a route has at most `max_rules_per_route` (8) rules
- a scope that borrows, `shared` or `external`, offers no consuming form
- a rule applies only when its predicate holds: `copy` needs a copyable object,
  and `address` an addressable one; a request does not drop qualifiers
- a route that no rule makes does not exist, for example the adoption of an
  array into `std::unique_ptr<T[]>`, or `std::optional<impl>` to
  `std::optional<iface>`, which would slice
- of the routes that reach one term the derivation keeps one: fewer rules, then
  no copy, then the order of the algebra; a variant keeps a sibling route per
  alternative, and the walk follows the alternative the object holds

### The Overflow Gap

A derivation holds at most `max_terms` (96) terms. A derivation that would
produce more sets `terms::overflow`, which nothing reads: no diagnostic reports
it, and the reference implementation has no counterpart. A binding over the
limit would publish an incomplete set, and some requests of it would be refused
as not found or not convertible with no explanation. The route tables do not
reach the limit, but a deep stack of wrappers or many interfaces could.

The smallest fix is a catalog diagnostic that rejects such a binding, when it is
compiled in a static container and when it is registered in a runtime one.

## Why Derivation Is Needed

The derivation serves three uses:

1. registration: whether a binding serves each interface it declares, and which
   interfaces and leaves name it
2. runtime publication: a runtime binding is type-erased, so the code that walks
   its stored object is instantiated when it is registered, before any request
   is known; it publishes a thunk per term
3. route choice: a request takes the best term of the published set

Only the second use needs the terms to be enumerated in advance. The first and
the third could ask their question directly.

## Alternatives

From the smallest change to the largest:

1. Keep the derivation and make it safe: report the overflow, and compute the
   limits from the shape.
2. Search from the request: search for a route from the stored shape to the
   requested one, with the same rules, exploring only the requested shapes. A
   static container evaluates it per request when the request is compiled, and a
   runtime container on the first request, kept in the memo. A runtime execution
   still needs code for the stored type.
3. Thunks per layer: a runtime binding publishes a small table of operations per
   layer of its stored shape, bounded by its depth and the forms of its kinds,
   not by the number of terms. No term set exists, and neither does the term
   limit.
4. Structural matching: if every route has the normal form below, a route is not
   searched at all but read off an alignment of the two shapes, in time linear
   in their depth.

## The Normal Form

The hypothesis is that every route the algebra selects has three phases:

1. peel: borrow, take, decay, or release outer layers of the stored shape
2. leaf rule: at most one copy, upcast, or conversion, possibly under the layers
   that are kept, as `std::shared_ptr<impl>` to `std::shared_ptr<iface>`
3. build: wrap, adopt, address, or share up to the requested shape

When it holds, the request shape is aligned against the stored shape from the
leaf outward, each step is checked against its access, the scope, and its
predicates, and the route follows. A variant is peeled by a branch on the held
alternative when the route is walked, so it needs no sibling routes.

## One Indirect Call

A runtime container knows the stored type `S` only at the binding and the
requested type `R` only at the request site, and no code is compiled for the
pair. The runtime backend already splits each route at a boundary: the binding
walks to it behind one indirect call, and the request completes it inline. The
normal form makes the split structural:

- peel and the leaf rule need `S` and the declared interfaces, so they run in
  the binding
- build needs only `R` and the core it starts from, the part of `R` that matched
  the stored shape, so it runs at the request site

### The Binding: A Thunk Table

At registration the binding instantiates a thunk per layer `k` of its stored
shape, per declared interface `i` or none, and per access, borrow or consume:

- borrow returns the address of layer `k`, after an upcast or a check of the
  held alternative
- consume constructs the object when the scope needs it, peels to layer `k`, and
  moves or converts it into a buffer the request supplies

The table has a few entries: depth times interfaces, independent of the number
of routes.

### The Request: Every Split Inline

`R` of depth `d` divides into build layers and a core in at most `d + 1` ways,
each with or without a copy of the core. The request site knows `R`, and so
every core type, and instantiates the build of each split inline, chosen by a
`switch` on a small index.

### Choosing Once

On the first request of `R`, the request site matches `R` against the descriptor
the binding publishes, the stored shape and the interfaces, which is data and
matches at runtime. The match yields the thunk, `k`, `i`, and the access, and
the index of the split. The memo keeps both, keyed by the request type, as it
keeps a selection now.

### The Hot Path

1. a hit in the memo yields the thunk and the split
2. one indirect call to the thunk returns an address or fills the buffer
3. a `switch` on the split runs the inline build: the address, the wrapper, the
   adoption into the handle of `R`, or the copy

A binding whose object is stable can keep the final address in the memo and make
no call, as it does now. A static container needs none of it: the match is
`constexpr`, and the route compiles to straight-line code.

## What Must Hold

- the normal form: an upcast under a kept layer, `std::shared_ptr<impl>` to
  `std::shared_ptr<iface>`, is one more thunk of the same kind, an upcast at
  layer `k` that rebuilds the handle
- conversions: `type_conversion_traits` needs the leaf and the interface, both
  known at registration, so a conversion is a thunk per declared interface
- the preference: when several splits match, the matcher picks the route the
  algebra prefers, fewer rules, then no copy, then the order of the algebra

The cost of a request changes too. The memoized path keeps one indirect call,
but the first request of a runtime binding computes its route instead of
comparing published terms. The benchmarks and the contract probes measure it.

## Next Step

A read-only script over the reference implementation,
[spec/reference/routes.py](../../spec/reference/routes.py), and the route table
classifies every route the reference selects: whether it is peel, at most one
leaf rule, and build, and whether a structural matcher selects the same route.

- If the hypothesis holds, matching replaces the search in the reference first,
  the tables check the C++ change against it, and the thunks per layer follow.
- If it does not, the exceptions name the rules that need a search, and the
  search from the request still removes the derivation in advance.
