# Cell Coverage Model

This directory is the first slice of the shared kernel's coverage model. It
owns the declarations that the library used to keep only in C++ and in the
hand-curated matrix tags.

- `storage_cells.py` enumerates every built-in storage scope by storage form
  cell. It compares the model against the scope policies and the `storage_form`
  enum, renders one compile-time assertion per cell, and rejects a catalog that
  is missing or stale. It also generates the committed
  `include/dingo/storage/scopes.gen.h` header, which now owns the built-in
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
- `route_cells.py` instantiates the real storage type and resolution list for
  every supported storage cell, including bounded arrays and array handles. It
  checks value, object, and rvalue route availability against the ledger,
  requires every published route target to be reachable from both the static and
  runtime request matchers, checks that every route's required access is
  satisfied by the scope, checks the modeled operation-kind and source-retention
  sequences, and asserts that representative routes do not unexpectedly cache
  conversions.
- `interface_cells.py` instantiates the real interface resolution kernel for
  storage exposed through `dingo::interfaces<...>`. It checks value, object, and
  rvalue availability and exact route counts, requires every published target to
  be reachable from both request matchers, and checks that the scope access
  satisfies every interface route. Single- and multiple-interface bindings are
  covered.
- `behavior_cells.py` generates one behavior class per supported scope/form
  cell in the applicable runtime and static containers. Form registration and
  expectations are data: a form is one table entry, and its scope/container
  product is derived from the route ledger. The tests check request identity,
  construction counts, copy and move counts, array element identity, leaf
  addresses, repeated resolution, and the lifetime balance after the container
  scope ends. Copy and move counts are derived from a request class
  (`borrow_value`, `consume_value`, `consume_rvalue`, `reference`, `handle`,
  `array`) instead of being hand-written per cell, so a count that does not
  match the request shape fails the build. Only the external registration move
  and the storage-shape construction counts are explicit data. A consumed raw
  pointer or row pointer transfers ownership to the caller, and its behavior
  case deletes the result before checking the balance. Multi-route cells add
  secondary cases: the unique plain cell also moves the rvalue route, and the
  shared alternative cell also copies the variant wrapper value. Supported
  cells without a resolvable behavior case are listed in `BEHAVIOR_GAPS` with a
  reason, so a cell cannot silently lose its test. A separate interface case
  list exercises storage exposed through `dingo::interfaces<...>`: shared
  references, pointers, and shared handles, plus unique owning handles. These
  cases use the same request-class cost model and assert the same construction,
  copy, move, and lifetime balance checks. Multiple-interface bindings are
  covered too: each interface is resolved through the same implementation, and
  the secondary request check proves both interfaces alias one object. A
  copyable interface value case covers value slicing, including the tracked
  copy count. Annotated behavior cases cover shared annotated concrete values
  and annotated interface references, pointers, shared handles, and handle
  references.
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
2. Add the name, representative type, and exposure behavior to the model.
3. Run `cells-check`; the generated assertions are the list of cells that must
   be implemented.

When the scope policies change, regenerate the committed header:

```bash
uv run --locked python tools/model/storage_cells.py \
    --out build/model/storage_cells.cpp \
    --header include/dingo/storage/scopes.gen.h \
    --include include
```
