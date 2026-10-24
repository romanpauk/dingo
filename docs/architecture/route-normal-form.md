# Route Normal Form

This page is a proposal, not a description of the library. It records how the
conversion component derives routes today, where that reliance on derivation is weak, and a
design that would replace the derivation by a structural match with one indirect
call on the hot path. Nothing here is implemented, and the hypothesis it rests
on is not yet confirmed.

## Derivation Today

[conversion/route.h](../../include/dingo/conversion/route.h) derives the
reachable types of a binding when the binding is compiled. `derive` starts from the stored
type tree, borrowed as an lvalue or consumed as an rvalue as the scope materializes
it, and applies every form that creates no wrapper: reach the element, take it,
decay, release, upcast to each declared interface, a declared conversion, and
copy. It repeats until nothing new appears. Each result is a reachable type: a type tree, a
value category, and the rules that reached it.

The forms that create a wrapper, `wrap`, `adopt`, and `address`, are not derived,
since only the request names the kind it wants. They apply at the request, on
top of a reachable type the binding serves, up to `max_created_wrappers` (2) wrappers.

The set of reachable types is closed under the rules, so no list states which requests a
binding serves: a shared `std::unique_ptr<impl>` serves `iface*` because the
rules borrow, upcast, and take the address. A kind of the user is derived the
same way, by the forms it declares. The set is bounded:

- a derived type tree is at most one wrapper larger than the stored one
  (`derivation_bound`), and a route has at most `max_rules_per_route` (8) rules
- a scope that borrows, `shared` or `external`, offers no consuming form
- a rule applies only when its predicate holds: `copy` needs a copyable object,
  and `address` an addressable one; a request does not drop qualifiers
- a route that no rule makes does not exist, for example the adoption of an
  array into `std::unique_ptr<T[]>`, or `std::optional<impl>` to
  `std::optional<iface>`, which would slice
- of the routes that reach one reachable type the derivation keeps one: fewer rules, then
  no copy, then the order of the conversion rules; a variant keeps a sibling route per
  alternative, and the walk follows the alternative the object holds

### The Overflow Gap

A derivation holds at most `max_reachable_types` (96) reachable types. A derivation that would
produce more sets `reachable_types::overflow`, which nothing reads: no diagnostic reports
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
   is known; it publishes a thunk per reachable type
3. route choice: a request takes the best reachable type of the published set

Only the second use needs the reachable types to be enumerated in advance. The first and
the third could ask their question directly.

## Alternatives

From the smallest change to the largest:

1. Keep the derivation and make it safe: report the overflow, and compute the
   limits from the type tree.
2. Search from the request: search for a route from the stored type tree to the
   requested one, with the same rules, exploring only the requested type trees. A
   static container evaluates it per request when the request is compiled, and a
   runtime container on the first request, kept in the memo. A runtime execution
   still needs code for the stored type.
3. Thunks per wrapper: a runtime binding publishes a small table of operations per
   wrapper of its stored type tree, bounded by its depth and the forms of its kinds,
   not by the number of reachable types. No reachable type set exists, and neither does the reachable type
   limit.
4. Structural matching: if every route has the normal form below, a route is not
   searched at all but read off an alignment of the two type trees, in time linear
   in their depth.

## The Normal Form

The hypothesis is that every route the conversion rules selects has three
phases:

1. peel: borrow, take, decay, or release outer wrappers of the stored type tree
2. leaf rule: at most one copy, upcast, or conversion, possibly under the wrappers
   that are kept, as `std::shared_ptr<impl>` to `std::shared_ptr<iface>`
3. build: wrap, adopt, address, or share up to the requested type tree

The classification below confirms the three phases with one refinement: a
`share` may come before the upcast, so the second phase may be preceded by one.

When it holds, the request type tree is aligned against the stored type tree from the
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
  the stored type tree, so it runs at the request site

### The Binding: A Thunk Table

At registration the binding instantiates a thunk per wrapper `k` of its stored
type tree, per declared interface `i` or none, and per access, borrow or consume:

- borrow returns the address of wrapper `k`, after an upcast or a check of the
  held alternative
- consume constructs the object when the scope needs it, peels to wrapper `k`, and
  moves or converts it into a buffer the request supplies

The table has about as many entries as the node count times the interfaces plus
one, independent of the number of routes, and a variant adds its alternatives;
see [What The Classification Found](#what-the-classification-found).

### The Request: Every Split Inline

`R` of depth `d` divides into build wrappers and a core in at most `d + 1` ways,
each with or without a copy of the core. The request site knows `R`, and so
every core type, and instantiates the build of each split inline, chosen by a
`switch` on a small index.

### Choosing Once

On the first request of `R`, the request site matches `R` against the descriptor
the binding publishes, the stored type tree and the interfaces, which is data and
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

- the normal form: an upcast under a kept wrapper, `std::shared_ptr<impl>` to
  `std::shared_ptr<iface>`, is one more thunk of the same kind, an upcast at
  wrapper `k` that rebuilds the handle
- conversions: `type_conversion_traits` needs the leaf and the interface, both
  known at registration, so a conversion is a thunk per declared interface
- the preference: when several splits match, the matcher picks the route the
  conversion rules prefers, fewer rules, then no copy, then the order of the conversion rules

The cost of a request changes too. The memoized path keeps one indirect call,
but the first request of a runtime binding computes its route instead of
comparing published reachable types. The benchmarks and the contract probes measure it.

## What The Classification Found

[test/routes/normal_form.py](../../test/routes/normal_form.py) reads the routes
the reference implementation selects, over the cases that
[test/next/conversion_cases.py](../../test/next/conversion_cases.py) enumerates,
and classifies each by the word its stages spell: peel, a rule of a leaf, an
upcast under a kept wrapper, and build. It changes nothing.

### The Form Holds

- The wrappers of the library: 62,832 requests, 2,635 served, 20 distinct stage
  words, none longer than 4 steps. 2,599 of the routes (98.63%) are peel, then at
  most one rule of a leaf and one upcast, then build. The other 36 are two
  sequences, `share > upcast_consumed` and `take > share > upcast_consumed`: a
  sole-owner handle that is shared and then upcast.
- The wrapper semantics of the user: with the 12 wrappers of one argument of the
  route probes (3 that stand in for the library, 3 of the same operations, and 6
  mutations that lack `share`, `adopt`, `wrap`, `borrow`, `copy`, or `retain`), a
  variant, and an array, 836,640 requests, of which 11,914 are served, give the
  same 20 words, none longer than 4 steps, and the same two sequences outside the
  form. A wrapper semantics chooses its operations from the closed set of the
  conversion rules and each operation has one stage, so the form is a property
  of the operations, not of any semantics. A mutation changes which routes exist,
  not their form.

### The Order Of Sharing And Upcasting

A sole-owner handle requested as a sharing handle of an interface can be shared
and then upcast, which the reference selects because `share` comes first in the
order of the operations, or upcast and then shared, which is also a route since
that handle has `upcast_consumed`. The results are the same type, but the two
delete the object differently: a `std::shared_ptr` made before the upcast keeps
the deleter of `impl`, and a `std::unique_ptr` upcast first deletes through
`iface`, which needs a virtual destructor. The reference does not model the
destructor, the C++ does differ, and the form should keep the order the reference
selects: peel, an optional `share`, the rule of a leaf and the upcast, build.
Making the upcast first would change 36 of the 2,635 routes of the table.

### Memory And Compile Time

The form does not save memory. The thunks a binding has today, one for each
reachable type, are about as many as the table of the proposal, one for each node
of the stored type tree and each interface or none:

| binding | reachable types | nodes x (interfaces + 1) |
|---|---|---|
| shared `impl`, two interfaces | 4 | 3 |
| shared `std::shared_ptr<impl>`, two interfaces | 10 | 6 |
| unique `std::unique_ptr<impl>`, two interfaces | 6 | 6 |
| shared `std::optional<std::shared_ptr<impl>>` | 12 | 9 |
| shared `std::variant<impl, other>` | 8 | 9 |
| shared `std::shared_ptr<std::variant<sp, up, other>>` | 16 | 21 |

It is fewer for a plain handle, equal for a unique pointer, and more for a
variant, whose alternatives are positions of their own. The memory that matters
is not the thunks but the constants sized by capacity, about 100 KB in each
binding type, and exact sizes remove it without the form, see M10.5 in
[spec-driven-design.md](spec-driven-design.md#m105-exact-sizes).

The derivation is not a cost of compiling either: constant evaluation is 2% of
the compile of 20 bindings on GCC and 4% on Clang, and code generation, which
follows the number of thunks, is most of the rest. The form moves work and does
not remove it: the first request of a type matches the type trees, and the
request site instantiates up to `d + 1` splits for a request of depth `d`.

## What Remains To Check

The classification is necessary and not sufficient. A matcher has to

1. align the request with the stored type tree, so that every build wrapper sits
   on a core that is a subtree of the stored type tree, with the leaf replaced by
   an interface at most
2. choose among the routes that tie as the reference does: fewer rules, then no
   copy, then the order of the operations, with `share` before the upcast

The next step is a matcher in the reference implementation, run beside
`resolve` over the 2,635 routes of the library and the 11,914 with the wrappers
of the user, and compared route by route. The proposal is worth more than the
derivation it replaces only if the matcher agrees, and even then it is a
simplification of the model, not a saving of memory. The exact sizes of M10.5
come first.
