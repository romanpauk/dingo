# Model Generator Handover

Date: 2026-10-27

This is the handover for the `tools/model` coverage generator after the
per-storage request-derivation refactor.

## Current status

The tree is green:

- `pytest tools/model/tests test/matrix/generator_test.py`: 299 passed
- `dingo_model_test`: 1013 tests passed, including both leaf traits
- `cells-check`: passes
- `ctest`: 77/77 passed
- `resolve-check`: 274 tests, report written to `build/test/resolve-gaps.md`
  (2,009 calls, 772 overclaims)
- `format-check` and full `check`: pass

Current model snapshot (both leaf traits):

- storage paths: 105
- derived request paths: 12,540
- cases (copyable domain): 132,060
- supported: 2,041 (copyable 1,092, move-only 949)
- compile-tested: 2,041
- runtime-tested (key-mapped): 1,955
- compile gap: 0
- undeclared runtime gap: 0
- declared runtime gap: 86 (43 per trait)

Supported by binding (copyable / move-only):

- concrete: 778 / 677
- keyed: 139 / 118
- annotated: 99 / 78
- interface: 42 / 42
- annotated interface: 34 / 34

The runtime-tested count is a key mapping, not an execution log. The request
based families (`behavior_cells`, `annotated`, `keyed`, `const`, and the
interface families) call `resolve` per row. The concrete shape matrix guards
each probe with `lifted_request_supported`; when the model overclaims a shape
the block compiles out, but `report/coverage.py` still maps the key. Comparing
the supported concrete keys with `build/test/type-matrix.json` shows 253 of 487
copyable keys (354 of 730 copyable cases, 313 of 651 move-only cases) have no
library route. The next coverage step is to either tighten `derive_route` for
those nested shapes or have `type_behavior` log the probes it actually executes.

A 10-storage runtime probe splits the concrete failures by form: 388 value
requests, 8 lvalue references, 8 pointers, and 26 rvalue references. An extra
wrapper of the same kind is **not** inherently unsupported: by value,
`optional<optional<T>>` and `variant<variant<T,long>,long>` resolve (the stored
wrapper is copied into the new one). What fails is an access form to the
wrapper-of-wrapper (`optional<optional<T>>&`, `*`, `&&`), a kind-changing value
(`optional<variant<T,long>>`, `optional<shared_ptr<T>>` from an optional), or a
nested handle value (`shared_ptr<shared_ptr<T>>`, `unique_ptr<shared_ptr<T>>`).
Tighten the value-wrapping and access rules separately; do not reject same-kind
wrappers wholesale.

Declared runtime gaps (per trait):

- 35 concrete top-level handle / cyclical / consuming-borrow forms
- 8 consuming-scope interface forms

The declared gaps are listed with reasons in `build/paths.md`
(`### Declared runtime gaps`).

## Architecture

The model now lives in packages under `tools/model`:

```text
grammar/      type grammar, C++ type algebra, storage forms, wrappers
domain/       stored types -> request closure -> binding projections -> cases
semantics/    support rules, capabilities, routes, extended shapes, gaps
emit/         C++ generators, templates, compile binding matrix
report/       coverage report, outcome baseline
tests/        pytest suite
legacy/       request/binding algebra still consumed by test/matrix
```

Imports flow downwards only:

```text
grammar -> domain -> semantics -> emit -> report
```

`tools/model/tests/architecture_test.py` fails on upward imports.

### Grammar

- `grammar/expr.py` — path constructors and `CONSTRUCTORS` registry.
- `grammar/cpp.py` — structured `CppType` and the only C++ spelling renderer.
- `grammar/enumerate.py` — bounded object/request/storage projections.
- `grammar/shapes.py` — wrapper semantics and request shapes.
- `grammar/catalog.py` — scopes, storage forms, `FormConstructor`, `Factory`.
- `grammar/storage.py` — `StoragePath`, `storable`, const helpers.
- `grammar/traits.py` — `LeafTraits`, `COPYABLE`, `MOVE_ONLY`; the leaf
  copy/move dimension used by the trait matrix.
- `grammar/bindings.py` — `Binding` and binding projections.

### Domain

`domain/requests.py` is the current derivation pipeline:

```text
seed = {T, stored expression}
  -> bounded constructor closure (DEPTH = 2)
  -> concrete / annotated / keyed / interface projections
```

`domain/cases.py` builds cases from each `StoragePath` and its projections. This
replaced the old global `request_forms() x storage` product.

Important: `DEPTH = 2` is the current coverage bound. It is conservative.
Experiments showed:

- depth 2: ~132k cases, 1,092 copyable supported
- depth 3: ~857k cases, 4,299 supported
- depth 4: ~6.8M cases, 29,303 supported

Do not raise the depth without validating the route/support rules for the new
combinations. The earlier global-list model overclaimed many of them.

### Semantics

- `semantics/support.py` — classification rules. It contains:
  - concrete route/exposure rules
  - const-storage conversion rules
  - keyed capability rules (direct leaf only; consuming-scope restrictions)
  - interface direct-target rules
  - annotated wrapper matching and const matching
- `semantics/capabilities.py` — derived exposed binding kinds per scope/form.
- `semantics/cells.py` — supported scope/form cell product.
- `semantics/gaps.py` — explicit gap registry and `declared_runtime_gap(...)`.
- `semantics/extended_shapes.py` — shapes outside the base shape catalog.

### Emit

- `emit/matrix.py` — concrete compile-time shape matrix.
- `emit/resolve.py` — storage-driven runtime probe. One shard per
  `(scope, form)` cell registers each storage once per binding and calls
  `resolve` for every supported case. Outcomes are recorded, not asserted.
- `emit/traits.py` — copyable/move-only compile-time matrix. It builds each
  supported cell for both leaves and asserts the library's move-only route set
  is the copyable set minus the routes that copy the leaf. The copy boundary is
  derived by `semantics/routes/derivation.py:leaf_copy_required`.
- `emit/binding_matrix.py` — compile-time `dingo::bindings<dingo::bind<...>>`
  probes for annotated/keyed/interface cases.
- `emit/families/`:
  - `behavior.py` — runtime behavior cases
  - `const.py` — const storage runtime probes
  - `annotated.py` — annotated runtime probes
  - `keyed.py` — direct-leaf keyed runtime probes
  - `interface_runtime.py` — stable-scope interface runtime probes
  - `storage.py`, `route.py`, `interface.py`, `property.py` — compile probes
- `emit/templates/` — Jinja layouts.
- `emit/common/recipe.py` — registration/instance renderer.
- `emit/common/validation.py` — library catalog validation.

### Report

- `report/paths.py` — Markdown/JSON coverage report.
- `report/resolves.py` — per-storage resolve outcome report. It splits covered
  calls, model overclaims, declared gaps, and stale declarations.
- `report/coverage.py` — supported vs tested vs declared gaps.
- `report/baseline.py` — pinned matrix/behavior outcomes.

## Generated C++ suites

Current generated test counts (copyable + move-only):

- `keyed_cells`: 139 + 118
- `annotated_cells`: 99 + 78
- `behavior_cells`: 93 + 86
- `const_cells`: 52 + 28
- `type_behavior`: 47 + 47
- `storage_cells`: 41
- `interface_runtime`: 68 + 68
- `route_cells`: 33
- `interface_cells`: 7
- `annotation_cells`: 3 + 3
- `type_matrix`, `trait_matrix`, `selection_rules`, `property_cells`: 1 each

The behavior move-only variant writes its outcomes to
`behavior-outcomes-move-only.jsonl`; the baseline check merges it with the
copyable file.

Generated files are under `build/test/generated/model/` and are compiled into
`dingo_model_test`.

## Key invariants

- `tests/grammar_test.py` checks constructor registry consistency.
- `tests/architecture_test.py` checks layer imports.
- `tests/binding_matrix_test.py` checks compile binding rows are supported.
- `tests/keyed_cells_test.py`, `interface_runtime_test.py`,
  `annotated_cells_test.py`, `const_cells_test.py` check each runtime family.
- `coverage.py` splits untested supported cases into undeclared gaps and
  declared gaps. Do not make a missing case disappear without a reason.

## Work status

### 0. Storage-driven resolve probe (in progress)

`cmake --build build -t resolve-check` builds the hierarchy
`runtime/<scope>/<storage>.cpp` (63 storage files, 2,009 tests) and writes
`build/test/resolve-gaps.md`. A file is one storage path, a suite is that
storage path, and a test is one `(trait, binding, request)`. A resolve that
throws is recorded and the test is skipped. The probe fixtures are
`copyable`/`movable`; the behavior families keep the lifetime-counting `tracked`
fixture. Current result:

- 2,009 expected calls, 1,061 covered;
- 772 overclaims (all concrete);
- 28 declared failures and 148 stale declarations.

The overclaims are concentrated in `bounded_array` (370), `shared_handle` (96),
`shared_array_handle` (84), `pointer` (60), and the handle/optional forms. By
access they are 704 value, 36 rvalue, 24 pointer, 8 lvalue. This is the work
queue for tightening the model:

1. `declared_runtime_gap` labels nested-handle access as "top-level handle
   access"; those 28 are real failures, not declared gaps.
2. Remove the stale handle-access declarations (144 resolve today).
3. Fix the bounded-array request grammar: requests always use extent 2, so
   `T[3]` and `T[2][3]` storages are probed with the wrong request.
4. Tighten value wrapping for handles and kind-changing inline wrappers.
5. Once the overclaims reach zero, make the resolve probe the runtime oracle and
   retire the type-behavior key mapping.

### 1. Copyable / move-only leaf traits

Done:

- `grammar/traits.py` declares `LeafTraits` with `copyable`/`movable`.
- `Case` carries `traits`; `iter_cases(..., traits)` stamps the domain.
- `classify(..., traits)` rejects copying routes for a move-only leaf, const
  rvalue requests, and const inline external registrations.
- `emit/traits.py` asserts the trait delta against the real resolution kernel
  for every supported cell and shape (const rvalue shapes are excluded because
  the lift trait overclaims them).
- Every runtime family emits a move-only variant: `behavior_cells`,
  `annotated_cells`, `keyed_cells`, `const_cells`, `interface_runtime`, and the
  shape-level `type_behavior`.
- `emit/binding_matrix.py` includes move-only registration probes.
- `Shape.with_leaf()` now rebases the `int` base leaf, so the type-behavior
  guards run real leaf-typed requests instead of compiling out.
- Coverage keys include the trait; `build/paths.md` has a `## Leaf traits`
  table. Baselines include the move-only behavior outcomes.

Remaining work:

1. The library's compile-time lift trait still answers for const rvalue requests
   that the runtime container cannot materialize. Rejecting them in the lift
   layer would let the trait matrix drop its exclusion.
2. `report/paths.py` still classifies the copyable domain in its outcome and
   per-family tables; the trait table is aggregated separately. If the model
   needs per-trait rule histograms, iterate both traits there too.

### 2. Annotated interfaces (done)

`Binding` has an `annotated_interface` kind carrying both the tag and the
interface list. The request projection derives
`annotated(interface(request, interface_type), tag)`, and the binding maps it to
`annotated<...interface_type..., tag>` for requests and
`annotated<interface_type, tag>` for registration. Support requires both the
annotated and interface capabilities to be exposed, and the interface runtime
family emits the composed rows for both leaf traits. The binding matrix probes
the composed registration.

### 3. Raise the per-storage request depth

Current `DEPTH = 2` is conservative. Measured with the current model (five
bindings, both leaf traits):

- depth 2: 2,508 request forms, 264,120 cases, 2,041 supported, 170 shapes;
- depth 3: 20,030 request forms, 2,141,450 cases, 8,098 supported, 1,145 shapes.

Almost all depth-3 growth is concrete (3,971 copyable / 3,541 move-only); keyed,
annotated, interface, and annotated-interface counts do not change. The type
matrix, trait matrix, and type-behavior suites would grow from 222 to about
1,145 shapes (roughly 5x). `type_matrix.cpp` is already the compile-time
bottleneck, so consider a compile-only depth-3 probe before wiring the new
shapes into every runtime family.

Before raising it:

- define per-storage route rules for the new combinations;
- probe the new derived requests and fix any overclaims the compile/runtime
  matrices expose;
- update supported counts, baselines, and the docs.

Do not simply remove the bound; depth 4 produces a much larger surface.

### 4. Remaining declared gaps

The 43 declared gaps are real exceptions, not missing tests:

- 35 concrete: top-level handle access, cyclical pointer access, consuming-scope
  lvalue borrows.
- 8 interface: consuming-scope interface registration.

Reducing them requires new scenario families, not more generated cases.

### 5. Cleanup

- `behavior.py` still contains scenario-level C++ strings; move them through
  `CppType` / registration plans. The interface and annotation recipes are
  hand-built shape lists, not derived from the ledger.
- `legacy/binding.py` and `legacy/request.py` are test/matrix-only. The
  architecture test now enforces that no packaged module imports the layer.
  Migrating `test/matrix` is still the end state.
- `type_matrix.cpp` is the compile-time bottleneck. If it grows, split it into
  multiple translation units.
- `report/paths.py` outcome and per-family tables still describe only the
  copyable domain; the trait split lives in the coverage table.

## Commands

Configure:

```bash
cmake -S . -B build -DDINGO_DEVELOPMENT_MODE=ON
```

Model pytest:

```bash
./.venv/bin/python -m pytest -q tools/model/tests test/matrix/generator_test.py
```

Coverage report:

```bash
./.venv/bin/python -m tools.model paths \
    --out build/paths.md --json build/paths.json
```

Other generators through the dispatcher:

```bash
python -m tools.model matrix --out ...
python -m tools.model trait-matrix --out ...
python -m tools.model resolve --out-dir ... --manifest ...
python -m tools.model binding-matrix --out ...
python -m tools.model keyed --out ...
python -m tools.model interface-runtime --out ...
python -m tools.model annotated --out ...
python -m tools.model const --out ...
```

Quality:

```bash
cmake --build build -t cells-check
cmake --build build -t resolve-check
ctest --test-dir build --output-on-failure -j8
cmake --build build -t format
cmake --build build -t check
```

Baseline update after an intended model change:

```bash
python -m tools.model.report.baseline update \
  --matrix build/test/type-matrix.json \
  --behavior build/test/behavior-outcomes.jsonl \
  --baseline test/baselines/model.json \
  --changes test/baselines/model-changes.json \
  --reason "describe the intended model change"
```

## Git state

The layered package refactor is committed
(`Split the model generator into layered packages`). The leaf-trait slice is the
next commit: `grammar/traits.py`, `emit/traits.py`,
`emit/templates/trait_matrix.cpp.j2`, the associated test, and the CMake wiring.

See also:

- `docs/architecture/type-path-coverage.md`
- `tools/model/README.md`
