# Type Path Coverage

The coverage model is a type grammar, not a list of cases. A request or a
storage is a path from a leaf; support is a relation over those paths.

## The grammar

A path is a leaf plus a chain of constructors:

```text
Leaf(name)
  const Expr
  Expr &          Expr &&
  Expr *
  Expr [N]        Expr [N][M] ...
  Expr (&)[N]     Expr (*)[N]
  optional<Expr>
  unique_ptr<Expr>        unique_ptr<Expr[]>
  shared_ptr<Expr>        shared_ptr<Expr[]>
  variant<Expr, Expr...>
  annotated<Expr, Tag>
```

`tools/model/grammar/expr.py` owns the path constructors. They reject
combinations C++ itself rejects (reference to reference, optional of reference,
array of reference, variant of reference, array handle of array). `Expr.cpp()`
projects a path to a structured `CppType`; `grammar/cpp.py` is the single source
of C++ spelling. Storage forms and wrappers build the same `CppType` values
instead of formatting type strings, and no consumer parses a rendered type back
into parts.

`tools/model/grammar/enumerate.py` owns the bounded enumeration. Given a leaf it
produces one universe and projects it:

- `object_forms` — plain paths, wrappers, variants, and the access closure
  around composed objects (so `optional<T> &`, `shared_ptr<T> *`, and
  `variant<const T, long> &` are in the same universe as `T &`);
- `request_types` — objects plus the annotated, keyed, and interface qualifiers;
- `storage_types` — the shallower storage projection.

Consumers filter a projection. They do not enumerate types of their own, so a
new constructor or a missing access form is fixed in one place.

The constructor names themselves live in `grammar/expr.py` (`CONSTRUCTORS`); the
grammar derives its qualifier, array, and wrapper sets from that registry
instead of repeating them. `grammar_test.py` enforces the cross-module
invariants: every registered constructor must appear in the request projection,
every storage expression must map to a storage form, every wrapper-like
constructor must have wrapper semantics and a shape mapping, and every binding
constructor must have a binding. A constructor added to one side without the
other fails the suite.

A **case** is a quadruple:

```text
(scope, storage-path, binding, request-path)
```

The built-in scopes are the catalog scopes (`unique`, `shared`, `external`,
`shared_cyclical`). Storage paths are the forms the library can hold (plain,
pointer, bounded array, unique/shared handle, array handle, optional, variant).
The binding is what the registration exposes: concrete, interface, annotated,
keyed, collection, or local. A request qualifier only makes sense with the
matching binding — an `annotated<T&, tag>` request requires an annotated binding
with the same tag, a keyed request requires a keyed binding with the same key,
and an interface request requires an interface binding.

## Bounds

The grammar is infinite, so `tools/model/grammar/enumerate.py` enumerates a
bounded domain:

- qualifier depth 2;
- wrapper depth 2;
- array extents `2` and `3`, including `T[2][3]`;
- two-alternative variants from a small object pool;
- one annotation tag and one key.

Those bounds are deliberate. Raising them is a coverage decision, not a
correctness requirement. The current snapshot is:

- 42 plain paths, 180 wrapped paths, and 190 variant paths;
- 412 annotated, 412 keyed, and 412 interface request paths;
- 10,032 request paths over 105 storage slots;
- 4 bindings and 4 scopes, for 105,648 cases.

## Support classification

`tools/model/semantics/support.py` classifies every case as one of:

- `supported` — the model's route rules publish the request from the cell;
- `unavailable` — the scope/form does not expose the category;
- `unsupported` — a route rule blocks the request (`take`, `wrap`, `decay`,
  `nested_access`, `const_copy`, …);
- `unmodeled` — the path is outside the current cell/shape vocabulary.

`unmodeled` is the important signal: it means the grammar knows the path but the
model does not yet map it. It is never silently dropped. The report
distinguishes “the library cannot” from “the model does not say yet”.

The classification is path-local. A failing prefix prunes its whole subtree,
which is what keeps the domain tractable.

## The report

```bash
uv run --locked python -m tools.model.report.paths \
    --out build/paths.md \
    --json build/paths.json
```

The report contains:

- domain counts;
- outcome totals;
- per request family and per storage counts;
- the unsupported/unavailable rule histogram;
- unmodeled examples;
- path-extended shapes outside the bounded depth-2 catalog.

The report is the map for the next extension step. A family with a large
`unmodeled` count is a model gap; a family with a large `unsupported` count is a
library/route limitation. The “path-extended shapes” table is the set that
`type_matrix.py` merges into the compile-time matrix, so every supported path is
generated as a cell row.

## Current gap

At the time of writing the report classifies 105,648 cases:

- plain: 42, wrapped: 180, variant: 190, annotated: 412, keyed: 412, interface:
  412;
- 10,032 request paths, 105 storage paths, 4 bindings, 4 scopes;
- 1,080 supported;
- 52,823 unavailable;
- 32,584 unsupported;
- 19,161 unmodeled.

`unavailable` means the storage does not expose the category or the binding
kind. `unmodeled` means the model has no rule or registration model for the
case. The domain is generated in full: qualified requests are classified even
when the storage does not expose that binding kind.

## Test coverage comparison

The report compares the supported set with what the generated C++ suites
exercise:

- 1,080 supported paths;
- 1,080 covered by the compile-time type/binding matrices and const probes;
- 1,037 covered by the runtime behavior, annotated, keyed, interface, and const
  probes;
- 0 supported paths have no compile-time test;
- 0 supported paths have an undeclared runtime gap;
- 43 supported paths are declared runtime gaps.

The declared runtime gaps are 35 concrete top-level handle/cyclical/
consuming-borrow forms and 8 consuming-scope interface forms. They are listed
with reasons in the report. Annotated runtime covers direct leaf access and
native wrapper requests (`annotated<optional<T>, tag>`,
`annotated<shared_ptr<T>, tag>`, and their const variants). Keyed runtime covers
every supported direct-leaf keyed case.

Const storage is classified by the conversions a const leaf can offer: a copy, a
const borrow, or one indirection. Inline storage cannot lend an indirection, a
const handle cannot be wrapped into a second handle, and a variant request must
keep the stored alternative's constness. The runtime probes are generated from
that same supported case set; there is no request candidate list in the emitter.

The 475 path-extended shapes are mostly access forms around composed objects:
`optional<T> &`, `shared_ptr<T> *`, `variant<const T, long> &`, and arrays of
wrapped pointers. They are merged into the type matrix, so the compile-time
matrix and the runtime behavior suite consume them instead of keeping a separate
hand-written list.

The type matrix now enumerates **587 shapes** over 32 cells (**18,784 probes**),
and the binding matrix adds **254** compile-time registration probes. The
behavior suites run the same catalog; the behavior outcome baseline remains 95
rows because the new shapes are not top-level behavior cases. The matrix
baseline was updated with the declared reason
`render C++ types and factories from the structured form model`.

## Test generation

The supported paths feed two layers:

- **compile time** — every supported path becomes a type-level probe
  (`static_assert`) for route reachability, kind, retention, step, and target
  shape. This is where exhaustive coverage lives.
- **runtime** — one representative per equivalence class
  `(scope, storage family, request class, route, lifetime class)` runs through
  the behavior harness. The class table explains why the representative covers
  the rest.

The unannotated supported paths are consumed by `emit/matrix.py` and
`emit/behavior.py`. Annotated paths are emitted by `emit/families/annotated.py`,
const-qualified storage by `emit/families/const.py`, direct-leaf keyed paths by
`emit/families/keyed.py`, and direct interface paths by
`emit/families/interface_runtime.py`. Wrapped keyed conversions are outside the
keyed capability; wrapped interface paths and consuming-scope interface paths
remain the next emission targets. The matrix already has `full` and `portable`
profiles; the path model gives the same split without keeping two sets of axes.

## Extending the domain

1. Add or change a constructor in `type_expr.py` and its applicability rule.
2. Add the storage/request spelling to `support.py` if the model vocabulary
   covers it.
3. Add a support rule where the library has a real constraint.
4. Run `paths.py`; the unmodeled/unsupported counts tell you what remains.
5. Wire the new supported paths into the compile-time or runtime consumer.

## Profiling

The bounded enumeration is cheap. On the current tree:

- `domain.counts()` (including the full case product): ~2.3 s;
- full report (`1.7M` classifications): ~12 s without profiling.

The hot path is expression hashing/equality, not rule evaluation. The
enumeration and classification functions are cached with `functools.lru_cache`;
profiling with `cProfile` is the first step when bounds grow:

```bash
uv run --locked python -m cProfile -s cumulative -m tools.model.report.paths \
    --out /tmp/paths.md
```

Do not raise the bounds until the report runtime and the generated C++ volume
are measured. The path table is also the place to introduce a representative
profile when the full grammar becomes too large to compile exhaustively.
