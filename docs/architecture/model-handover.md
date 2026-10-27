# Model Generator Handover

Date: 2026-10-27

This is the handover for the `tools/model` coverage generator after the
per-storage request-derivation refactor.

## Current status

The tree is green:

- `pytest tools/model/tests test/matrix/generator_test.py`: 271 passed
- `dingo_model_test`: 553 tests passed
- `cells-check`: passes
- `ctest`: 77/77 passed
- `format-check` and full `check`: pass

Current model snapshot:

| metric | value |
| --- | ---: |
| storage paths | 105 |
| derived request paths | 10,032 |
| cases | 105,648 |
| supported | 1,080 |
| compile-tested | 1,080 |
| runtime-tested | 1,037 |
| compile gap | 0 |
| undeclared runtime gap | 0 |
| declared runtime gap | 43 |

Supported by binding:

- concrete: 798
- keyed: 141
- annotated: 99
- interface: 42

Declared runtime gaps:

- 35 concrete top-level handle / cyclical / consuming-borrow forms
- 8 consuming-scope interface forms

The declared gaps are listed with reasons in
`build/paths.md` (`### Declared runtime gaps`).

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
- `grammar/bindings.py` — `Binding` and binding projections.

### Domain

`domain/requests.py` is the current derivation pipeline:

```text
seed = {T, stored expression}
  -> bounded constructor closure (DEPTH = 2)
  -> concrete / annotated / keyed / interface projections
```

`domain/cases.py` builds cases from each `StoragePath` and its projections.
This replaced the old global `request_forms() x storage` product.

Important: `DEPTH = 2` is the current coverage bound. It is conservative.
Experiments showed:

- depth 2: ~105k cases, 1,080 supported
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
- `semantics/gaps.py` — explicit gap registry and
  `declared_runtime_gap(...)`.
- `semantics/extended_shapes.py` — shapes outside the base shape catalog.

### Emit

- `emit/matrix.py` — concrete compile-time shape matrix.
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
- `report/coverage.py` — supported vs tested vs declared gaps.
- `report/baseline.py` — pinned matrix/behavior outcomes.

## Generated C++ suites

Current generated test counts:

| suite | tests |
| --- | ---: |
| `keyed_cells` | 141 |
| `annotated_cells` | 99 |
| `behavior_cells` | 93 |
| `const_cells` | 52 |
| `type_behavior` | 47 |
| `storage_cells` | 41 |
| `interface_runtime` | 34 |
| `route_cells` | 33 |
| `interface_cells` | 7 |
| `annotation_cells` | 3 |
| `type_matrix`, `selection_rules`, `property_cells` | 1 each |

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

## Open work, in priority order

### 1. Copyable / move-only leaf traits

Not implemented yet. The current generated C++ uses a single copyable
`tracked` fixture. The next agent should add:

```python
@dataclass(frozen=True)
class LeafTraits:
    name: str          # copyable_type / move_only_type
    copyable: bool
    movable: bool
```

Then:

1. `Case` gains `traits`; coverage keys become
   `(scope, storage, binding, request, traits)`.
2. Support rules distinguish copy-out, copy-from-const, move, borrow, and
   pointer requests.
3. Every generated runtime family defines both fixtures and emits both rows.
4. Factories become trait-specific, e.g.
   `make_shared_copyable_type` and `make_shared_move_only_type`.
5. Baselines and docs are regenerated.

This roughly doubles the domain and generated C++ volume. Decide whether to
duplicate all runtime families or add a trait compile matrix first.

### 2. Annotated interfaces

Annotated wrappers now work (`annotated<optional<T>, tag>`,
`annotated<shared_ptr<T>, tag>`, const variants). Annotated **interfaces** do
not, because `Binding` is single-kind. The library supports:

```cpp
interfaces<annotated<interface_type, tag>>
```

and resolves `annotated<shared_ptr<interface_type>, tag>`. Required:

- compositional `Binding` (annotation + interface list);
- request grammar generating `annotated(interface(...))`;
- `Binding.request_type()` mapping that wraps the interface target in
  `annotated`;
- registration interfaces `annotated<interface_type, tag>`;
- runtime and binding-matrix rows for the composition.

### 3. Raise the per-storage request depth

Current `DEPTH = 2` is conservative. Before raising it:

- define per-storage route rules for the new combinations;
- probe the new derived requests;
- update supported counts and baselines.

Do not simply remove the bound; depth 4 produces ~29k supported paths and a
very large C++ surface.

### 4. Remaining declared gaps

The 43 declared gaps are real exceptions, not missing tests:

- 35 concrete: top-level handle access, cyclical pointer access,
  consuming-scope lvalue borrows.
- 8 interface: consuming-scope interface registration.

Reducing them requires new scenario families, not more generated cases.

### 5. Cleanup

- `behavior.py` still contains scenario-level C++ strings; move them through
  `CppType` / registration plans.
- `semantics/support.py` has unused `ANNOTATED_INNER_OPS` and
  `_leaf_interface`; remove them.
- `legacy/binding.py` and `legacy/request.py` are test/matrix-only. Migrate
  `test/matrix` or keep them clearly isolated.
- `type_matrix.cpp` is the compile-time bottleneck. If it grows, split it into
  multiple translation units.

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
python -m tools.model binding-matrix --out ...
python -m tools.model keyed --out ...
python -m tools.model interface-runtime --out ...
python -m tools.model annotated --out ...
python -m tools.model const --out ...
```

Quality:

```bash
cmake --build build -t cells-check
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

The refactor is uncommitted. Old flat `tools/model/*.py` files appear as
deletions, and new package files are untracked. Stage/commit with rename
detection before continuing. Remove `tools/model/**/__pycache__` before
committing.

See also:

- `docs/architecture/type-path-coverage.md`
- `tools/model/README.md`
