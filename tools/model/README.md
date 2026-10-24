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
- `route_cells.py` instantiates the real storage type and resolution list for
  every enabled non-array storage cell. It checks value, object, and rvalue
  route availability against the scope/form model, so a cell that stops
  publishing a category fails to compile.
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
