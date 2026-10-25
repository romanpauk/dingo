# Cell Coverage Model

This directory is the first slice of the shared kernel's coverage model. It
owns the declarations that the library used to keep only in C++ and in the
hand-curated matrix tags.

- `storage_cells.py` enumerates every built-in storage scope by storage form
  cell. Each scope declares capabilities (consuming, cyclical, pointer source)
  and each form declares its semantic kind and array/handle facts; the exposure
  bits are derived from those capabilities rather than a per-form table. Raw
  arrays and the cyclical shared-array handle stay explicit unavailable
  exceptions. The model is compared against the scope policies and the
  `storage_form` enum, renders one compile-time assertion per cell, and rejects
  a catalog that is missing or stale. It also generates the committed
  `include/dingo/storage/scopes.gen.h` header, which owns the built-in
  `storage_scope_policy` specializations.
- `selection_rules.py` enumerates every selection policy and pair of source
  statuses. It compares the model against the C++ policy and status enums and
  renders the full rule table as compile-time assertions.
- `route_ledger.py` enumerates every scope, form, and route category with one
  explicit outcome: supported, unavailable with a reason, or a tracked gap.
  Supported routes also carry their expected operation-kind and source-retention
  sequence. Raw dynamic arrays are declared unavailable: the element count is
  not part of the type, so no scope can size, construct, or destroy them.
  Unsized shared array handles under `shared_cyclical` are an explicit
  constraint for the same reason.
- `outcome_baseline.py` pins the actual matrix outcomes and the actual behavior
  runtime outcomes in `test/baselines/model.json`. The behavior report records
  each executed case's counts and result (`ok` or an exception class).
  `check` fails on any difference; `update --reason ...` rewrites the baseline
  and records the declared change in `test/baselines/model-changes.json`.
- `property_cells.py` generates static assertions that compare the C++ wrapper
  properties (`is_owning`, `is_sharing`, `is_nullable`, `is_borrowable`,
  `is_array`, `is_variant`, `can_copy_out`, `can_take`) with the Python wrapper
  declarations for every built-in wrapper and the `test_optional`,
  `test_unique`, and `test_shared` families. A wrapper cannot silently drift
  from its declared semantics.
- `route_derivation.py` derives the published `ResolutionCandidate` sequence for
  a supported route from an ordered rule list over the form's semantic kind, the
  source shape the scope hands out (pointer, lvalue, rvalue, cyclical row
  pointer, or cyclical handle), and the stored wrapper properties. Each candidate
  carries its operation kind and source-retention flag. Rules do not
  name a storage form, so a custom wrapper with the same kind and properties
  follows the same rule as the built-in wrapper it mirrors. A supported cell
  with no matching rule fails validation instead of losing its kind coverage.
- `route_cells.py` instantiates the real storage type and resolution list for
  every supported storage cell, including bounded arrays and array handles. It
  checks value, object, and rvalue route availability against the ledger,
  requires every published route target to be reachable from both the static and
  runtime request matchers, checks that every route's required access is
  satisfied by the scope, checks the operation-kind and source-retention
  sequences derived by `route_derivation.py`, checks that each published route
  exposes a non-empty C++ conversion step list, and asserts that representative
  routes do not unexpectedly cache conversions.
- `wrapper_semantics.py` describes request shapes as a leaf wrapped in zero or
  more wrappers. A wrapper declares its semantics (`owning`, `sharing`,
  `nullable`, `borrowable`, `array`, `variant`, `in_place`), its C++ spelling,
  and which shapes it can wrap. The built-ins include const, references,
  pointers, optional, unique/shared handles, owning array handles, arrays, and
  variants; a user wrapper is another declaration with the same fields. The
  test-only families (`test_optional`, `test_unique`, `test_shared`, and
  `test_variant`) add one cross-wrapper pair each. Their built-in partner is
  derived by shape signature (kind plus owning/sharing/sole-owner/nullable/
  borrowable/array/variant properties), so a new family does not need a
  hand-written partner map.
- `route_rules.py` derives a request route from the wrapper semantics and the
  scope/form exposure. The rules are `access`, `borrow`, `move`, `copy_out`,
  `take`, `wrap`, `share`, `adopt`, `decay`, `select_alternative`,
  `nested_access`, `const_copy`, and `target`. The library still answers
  whether a route is published; the rule model names the first rule that blocks
  a request, so the report says `take: a borrowing scope cannot become an
  owner` instead of a structural category.
- `type_matrix.py` composes every shape from the wrapper semantics and applies
  it to every storable scope/form cell through the real resolution kernel. It
  writes a `type-matrix.md` report with the direct route, the lifted request,
  the top-level verdict, the route rule for every miss, and the published route
  targets per cell. Supported rows are answered by the library, not by a
  hand-written case list; the report is the exhaustive support table. The depth
  bound is an explicit model limitation. A route that only works as a dependency
  of a longer-lived construction is reported as `top-level = no`: a consuming
  scope can lend an lvalue reference to its dependent, but a borrow returned
  from the container itself would outlive the transaction that produced it.
  A shape that is not a direct target is reported as `lifted` when its value is
  resolvable: value wrappers copy or move their value, and owning handles only
  lift from a consumed value. The report groups misses by rule.
- `type_behavior.py` executes the top-level half of that matrix. Every
  supported shape is resolved twice through the same registration recipe as the
  behavior cells, stable scopes are checked for identity, fresh scopes for
  distinctness, owned raw pointers are released according to the transfer
  contract, and every block finishes with the lifetime balance intact. The
  runtime suite uses `container<>`; a second translation unit runs the same
  catalog through `static_container` for every statically bindable cell
  (`external` is runtime-only). The shape catalog and the top-level verdict come
  from `type_matrix.py`, so the suite follows the library instead of listing
  supported requests by hand. Unowned or non-transferred pointers are never
  released, and the two consuming lvalue-borrow shapes are emitted as explicit
  top-level gaps rather than silently skipped.
- `interface_cells.py` instantiates the real interface resolution kernel for
  storage exposed through `dingo::interfaces<...>`. It checks value, object, and
  rvalue availability and exact route counts, requires every published target to
  be reachable from both request matchers, and checks that the scope access
  satisfies every interface route. The counts come from
  `route_derivation.interface_route_counts`, keyed by the exposed semantic kind and
  whether the interface is a copyable value. Single- and multiple-interface
  bindings are covered.
- `behavior_cells.py` generates one behavior class per supported scope/form
  cell in the applicable runtime and static containers. Form registration and
  expectations are data: a form is one table entry, and its scope/container
  product is derived from the route ledger. The tests check request identity,
  construction counts, copy and move counts, array element identity, leaf
  addresses, repeated resolution, and the lifetime balance after the container
  scope ends. The primary request, relation, request class, and construction
  count are derived from the semantic form kind, wrapper properties, and scope.
  Copy and move counts come from the derived request class (`borrow_value`,
  `consume_value`, `consume_rvalue`, `reference`, `handle`, `array`) instead of
  being hand-written per cell, so a count that does not match the request shape
  fails the build. Only the external registration move remains explicit form
  data. A consumed raw
  pointer or row pointer transfers ownership to the caller, and its behavior
  case deletes the result before checking the balance. Multi-route cells add
  derived secondary cases: the unique plain cell also moves the rvalue route,
  and the shared alternative cell also copies the variant wrapper value.
  Supported
  cells without a resolvable behavior case are listed in `BEHAVIOR_GAPS` with a
  reason, so a cell cannot silently lose its test. Interface and annotation
  scenarios are derived from the behavior forms plus a request-form rule set,
  not a flat list of complete cases. Forms with no interface or annotation
  route are listed in `INTERFACE_GAPS` / `ANNOTATION_GAPS`. The interface cases
  exercise storage
  exposed through `dingo::interfaces<...>`: shared
  references, pointers, and shared handles, plus unique owning handles. These
  cases use the same request-class cost model and assert the same construction,
  copy, move, and lifetime balance checks. Multiple-interface bindings are
  covered too: each interface is resolved through the same implementation, and
  the secondary request check proves both interfaces alias one object. A
  copyable interface value case covers value slicing, including the tracked
  copy count. Annotation recipes cover shared annotated concrete values,
  annotated interface references, pointers, shared handles, and handle
  references, and external annotated value, reference, pointer, and
  shared-handle registrations. Negative tests refuse unannotated and wrong-tag
  requests and reject duplicate annotated registration.
- `templates/` contains logic-free Jinja templates. The Python model computes
  rows; the templates only lay them out.
- `*_test.py` is the pytest suite for the models and their deterministic
  output.

The generated translation units are built by `test/CMakeLists.txt` into
`build/test/generated/model/`. They are development artifacts, not shipped
headers, so consumer builds do not need Python.

`cells-check` regenerates both outputs and fails on any difference. The same
check also validates the model against the C++ catalogs, so adding a built-in
scope, storage form, selection policy, or status without extending the model is
an error rather than an untested cell.

Add a form or scope by extending the model first:

1. Add the C++ declaration.
2. Add the name, representative type, semantic kind, and capability facts to
   the model. Exposure is derived from the scope and form capabilities.
3. Run `cells-check`; the generated assertions are the list of cells that must
   be implemented.

When the scope policies change, regenerate the committed header:

```bash
uv run --locked python tools/model/storage_cells.py \
    --out build/model/storage_cells.cpp \
    --header include/dingo/storage/scopes.gen.h \
    --include include
```
