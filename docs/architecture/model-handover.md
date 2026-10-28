# Model Generator Handover

Date: 2026-10-27

This is the handover for the `spec` coverage generator after the per-storage
request-derivation refactor.

## Current status

The tree is green:

- `pytest spec/tests`: 217 passed
- `dingo_model_test`: 279 tests passed (behavior/type behavior, per form)
- `dingo_classification_test`: 111 tests (both detection backends)
- `dingo_resolution_static_asserts_test`: 40 tests
- `dingo_resolution_static_test`: 7 tests
- `dingo_resolution_runtime_test`: 4,774 tests (resolve rows, `construct` and
  `invoke` claims, 84 construct cases, 19 invoke shapes, 51 producer claims, 5
  mixed collection cases, 2 configuration cases, 2 mixed co-residence cases, 4
  cyclical cases, 7 conversion cases, 4 wrapper cases)
- `dingo_registration_test`: 158 tests
- `dingo_registration_runtime_test`: 6 tests
- `generated-check`: passes
- `ctest`: 38/38 passed (the retired table executables are gone)
- `resolve-check`: 274 tests, report written to `build/test/resolve-gaps.md`
  (2,009 calls, 772 overclaims)
- `claim-check`: report written to `build/test/claim-gaps.md`; `construct` 899
  ok / 257 declared / 771 shared / 82 pinned, `invoke` 952 ok / 257 declared /
  800 shared, nothing diverged or missing
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
based families (`behavior_cases`, `annotated`, `keyed`, `const`, and the
interface families) call `resolve` per row. The concrete shape table guards each
probe with `lifted_request_supported`; when the model overclaims a shape the
block compiles out, but `report/coverage.py` still maps the key. Comparing the
supported concrete keys with `build/test/type-table.json` shows 253 of 487
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

The model now lives in packages under `spec`:

```text
grammar/      type grammar, C++ type algebra, storage forms, wrappers
domain/       stored types -> request closure -> binding projections -> cases
semantics/    support rules, capabilities, routes, extended shapes, gaps
emit/         C++ generators, templates, compile binding table
report/       coverage report, outcome baseline
tests/        pytest suite
```

Imports flow downwards only:

```text
grammar -> domain -> semantics -> emit -> report
```

`spec/tests/architecture_test.py` fails on upward imports.

### Grammar

- `grammar/expr.py` — path constructors and `CONSTRUCTORS` registry.
- `grammar/cpp.py` — structured `CppType` and the only C++ spelling renderer.
- `grammar/enumerate.py` — bounded object/request/storage projections.
- `grammar/shapes.py` — wrapper semantics and request shapes.
- `grammar/catalog.py` — scopes, storage forms, `FormConstructor`, `Factory`.
- `grammar/storage.py` — `StoragePath`, `storable`, const helpers.
- `grammar/traits.py` — `LeafTraits`, `COPYABLE`, `MOVE_ONLY`; the leaf
  copy/move dimension used by the trait table.
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
- `semantics/forms.py` — supported scope/form pair product.
- `semantics/gaps.py` — explicit gap registry and `declared_runtime_gap(...)`.
- `semantics/extended_shapes.py` — shapes outside the base shape catalog.

### Emit

- `emit/table.py` — concrete compile-time shape table.
- `emit/resolve.py` — storage-driven runtime probe. One shard per
  `(scope, form)` form registers each storage once per binding and calls
  `resolve` for every supported case. Outcomes are recorded, not asserted.
- `emit/traits.py` — copyable/move-only compile-time table. It builds each
  supported form for both leaves and asserts the library's move-only route set
  is the copyable set minus the routes that copy the leaf. The copy boundary is
  derived by `semantics/routes/derivation.py:leaf_copy_required`.
- `emit/binding_table.py` — compile-time `dingo::bindings<dingo::bind<...>>`
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
- `report/baseline.py` — pinned table/behavior outcomes.

## Generated C++ suites

Current generated tests are grouped by phase:

- classification: `dingo_classification_test` (62 tests: constructor, wrapper,
  selection, storage);
- resolution compile: `dingo_resolution_static_asserts_test` (40 tests: route,
  interface, type/trait matrices);
- registration: `dingo_registration_test` (158 tests, 47 storage files);
- resolution: `dingo_model_test` (1,013 tests), `dingo_resolve_probe` (2,009
  resolve cases, skipped on failure).

The behavior move-only variant writes its outcomes to
`behavior-outcomes-move-only.jsonl`; the baseline check merges it with the
copyable file.

Generated files are under `build/test/generated/model/` and are compiled into
`dingo_model_test`.

## Key invariants

- `tests/grammar_test.py` checks constructor registry consistency.
- `tests/architecture_test.py` checks layer imports.
- `tests/binding_table_test.py` checks compile binding rows are supported.
- `tests/keyed_cases_test.py`, `interface_runtime_test.py`,
  `annotated_cases_test.py`, `const_cases_test.py` check each runtime family.
- `coverage.py` splits untested supported cases into undeclared gaps and
  declared gaps. Do not make a missing case disappear without a reason.

## Work status

### 0. Storage-driven resolve probe (in progress)

`cmake --build build -t resolve-check` builds the hierarchy
`resolution/runtime/<scope>/<storage>/<category>.cpp` (63 storage files, 2,009
tests) and writes `build/test/resolve-gaps.md`. A file is one storage path, a
suite is that storage path, and a test is one `(trait, binding, request)`. The
logical case path is
`<phase>/<container>/<scope>/<storage>/<category>/<traits>/<binding>/<request>`;
it is recorded through `RecordProperty("path", ...)`. A resolve that throws is
recorded and the test is skipped. The probe fixtures are `copyable`/`movable`;
the behavior families keep the lifetime-counting `tracked` fixture. Current
result:

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
  for every supported form and shape (const rvalue shapes are excluded because
  the lift trait overclaims them).
- Every runtime family emits a move-only variant: `behavior_cases`,
  `annotated_cases`, `keyed_cases`, `const_cases`, `interface_runtime`, and the
  shape-level `type_behavior`.
- `emit/binding_table.py` includes move-only registration probes.
- `Shape.with_leaf()` now rebases the `int` base leaf, so the type-behavior
  guards run real leaf-typed requests instead of compiling out.
- Coverage keys include the trait; `build/paths.md` has a `## Leaf traits`
  table. Baselines include the move-only behavior outcomes.

Remaining work:

1. The library's compile-time lift trait still answers for const rvalue requests
   that the runtime container cannot materialize. Rejecting them in the lift
   layer would let the trait table drop its exclusion.
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
family emits the composed rows for both leaf traits. The binding table probes
the composed registration.

### 3. Raise the per-storage request depth

Current `DEPTH = 2` is conservative. Measured with the current model (five
bindings, both leaf traits):

- depth 2: 2,508 request forms, 264,120 cases, 2,041 supported, 170 shapes;
- depth 3: 20,030 request forms, 2,141,450 cases, 8,098 supported, 1,145 shapes.

Almost all depth-3 growth is concrete (3,971 copyable / 3,541 move-only); keyed,
annotated, interface, and annotated-interface counts do not change. The type
table, trait table, and type-behavior suites would grow from 222 to about 1,145
shapes (roughly 5x). `type_table.cpp` is already the compile-time bottleneck, so
consider a compile-only depth-3 probe before wiring the new shapes into every
runtime family.

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
- The retired generator under `test/` and its legacy request/binding algebra are
  deleted; the model families above are the only generated coverage.
- `type_table.cpp` is the compile-time bottleneck. If it grows, split it into
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
./.venv/bin/python -m pytest -q spec/tests
```

Coverage report:

```bash
./.venv/bin/python -m spec paths \
    --out build/paths.md --json build/paths.json
```

Other generators through the dispatcher:

```bash
python -m spec table --out ...
python -m spec trait-table --out ...
python -m spec resolve --out-dir ... --manifest ...
python -m spec binding-table --out ...
python -m spec keyed --out ...
python -m spec interface-runtime --out ...
python -m spec annotated --out ...
python -m spec const --out ...
```

Quality:

```bash
cmake --build build -t generated-check
cmake --build build -t resolve-check
ctest --test-dir build --output-on-failure -j8
cmake --build build -t format
cmake --build build -t check
```

Baseline update after an intended model change:

```bash
python -m spec.report.baseline update \
  --table build/test/type-table.json \
  --behavior build/test/behavior-outcomes.jsonl \
  --baseline test/baselines/model.json \
  --changes test/baselines/model-changes.json \
  --reason "describe the intended model change"
```

## Git state

The layered package refactor is committed
(`Split the model generator into layered packages`). The leaf-trait slice is the
next commit: `grammar/traits.py`, `emit/traits.py`,
`emit/templates/trait_table.cpp.j2`, the associated test, and the CMake wiring.

See also:

- `docs/architecture/type-path-coverage.md`
- `spec/README.md`
