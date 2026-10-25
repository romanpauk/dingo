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
- `behavior_cells.py` generates behavior classes for plain shared, unique,
  external, and shared-cyclical bindings, shared/unique handles, optional,
  variant, raw pointer, and bounded array storage in the applicable runtime and
  static containers. The tests check construction, copy, move, identity,
  repeated resolution, array identity, and destruction after the container
  scope ends. Runtime and static unique values agree on move and destruction
  counts.
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
