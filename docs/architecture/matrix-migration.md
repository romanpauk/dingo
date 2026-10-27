# Matrix Migration

The legacy `test/matrix` generator is being retired. Its features are being
moved into the test hierarchy; a feature is removed from the matrix only after
the model hierarchy covers the same behavior.

## Coverage map

| matrix feature / family | model status | target | | --- | --- | --- | |
`resolve_concrete` | covered | dropped | | `resolve_wrapper` | covered | dropped
| | `resolve_unique_wrapper` | covered | dropped | | `resolve_interface` |
covered | dropped | | `resolve_keyed` | covered | dropped | | `annotated` |
covered | dropped | | `variant` | covered | dropped | | `array` | covered |
dropped | | `lifetime_counts` | covered | dropped | | `nested_wrappers` |
partial | `resolution/.../nested` | | `custom_wrappers` | covered (properties +
row) | `classification/wrapper` + `resolution/runtime/global/wrapper` | |
`constructor_detection` | partial (shape slice) |
`classification/constructor/<backend>/<mode>/<shape>` | | `construct` | covered
(row claims + 18 shape classes) | `resolution/runtime/<cell>/construct.cpp` and
`resolution/runtime/global/construct/<shape>` | | `construct_collection` |
covered (vector + map insert) | `resolution/<mode>/global/collection` | |
`local_bindings` | not covered | `resolution/.../local` | | `nested_container` |
not covered | `resolution/.../container` | | `factory_override` | covered
(producer classes) | `resolution/runtime/global/producer/<cell>` | |
`explicit_dependencies` | not covered | `resolution/.../dependencies` | |
`mixed_external_dependency` | not covered | `resolution/.../mixed` | |
`mixed_singular_ambiguity` | not covered | `resolution/.../mixed` | |
`custom_allocator` | not covered | `resolution/.../config/allocator` | |
`custom_rtti` | not covered | `resolution/.../config/rtti` | |
`resolve_collection` | covered (static + mixed) |
`resolution/<mode>/global/collection` | | `resolve_keyed_collection` | covered
(static + mixed) | `resolution/<mode>/global/collection` | | `resolve_indexed` |
not covered | `registration/.../indexed` | | `construct_invoke` / invoke family
| covered (19 shape classes) | `resolution/runtime/global/invoke/<shape>` | |
`scenario` family | not covered | `resolution/.../scenario` | |
`shared_cyclical` family | not covered | `resolution/.../cyclical` | |
dependency-composition families | not covered | `resolution/.../composition` | |
conversion parity | not covered | `classification/conversion` |

## Removed matrix features

`test/matrix/model_coverage.py` lists the features that no longer generate
matrix rows because the model hierarchy covers them:

- `annotated`
- `array`
- `lifetime_counts`
- `resolve_concrete`
- `resolve_interface`
- `resolve_keyed`
- `resolve_unique_wrapper`
- `resolve_wrapper`
- `variant`

## Constructor detection

The model has the first slice:

```text
classification/constructor/<shape>.cpp
```

It covers zero-argument, copy-only, move-only, aggregate, two-value,
dependency-reference/value/pointer/rvalue, annotated-constructor, `std::array`,
nested `std::array`, and move-only array element shapes.

The matrix still owns the backend (`portable`/`msvc`), detection-mode
(`shape`/`signature`), and constructor-argument-conversion axes. Those move into
`classification/constructor/<backend>/<mode>/<shape>` next.

## Removal order

1. Drop the features the model already covers. Done.
2. Rebuild the remaining construction (unique/cyclical) and factory forms, then
   collection, indexed, and container-composition features as model families.
3. Move the invoke, scenario, cyclical, dependency-composition, and conversion
   parity families.
4. Delete `test/matrix`, its axes, fixtures, scenarios, and the `legacy/`
   request/binding algebra once no feature remains.

## Factorization

See [Coverage Model](coverage-model.md). Operator families are factored over
dependency categories; a matrix feature is removed when its operator classes are
covered, not when its row count is reproduced.

## Test structure

The matrix features fall into four structural shapes. Each one maps to a
category under the shared hierarchy:

```text
<phase>/<execution>/<container>/<scope>/<storage>/<category>/<traits>/<binding>/<request>
```

- `container` is omitted for the default container. Other values are `indexed`,
  `indexed_int`, `indexed_string`, `indexed_dsl`, `allocator`, and `rtti`.
- `execution` is `classification`, `registration`, `resolution`, or
  `resolution/static` when the static front-end is the subject.

### Storage-cell behavior

Used by construct, factory, local bindings, nested containers, and the existing
resolve/behavior families:

```text
resolution/runtime/<scope>/<storage>/construct.cpp
resolution/runtime/<scope>/<storage>/invoke.cpp
registration/runtime/<scope>/<storage>/factory/<kind>.cpp
resolution/runtime/<scope>/<storage>/local/<traits>.cpp
```

`construct.cpp` and `invoke.cpp` are claims over the same rows as `resolve.cpp`,
rendered from `resolve-manifest.json`; the claim is the level after the cell in
the case path. Operator classes that are not tied to a cell live under `global`:

```text
resolution/runtime/global/construct/<shape>/<traits>.cpp
```

### Container configuration

Used by indexed lookup, allocator, RTTI, and mixed registration:

```text
resolution/runtime/indexed/<scope>/<storage>/resolve/<traits>.cpp
resolution/runtime/allocator/<scope>/<storage>/resolve/<traits>.cpp
resolution/runtime/rtti/<scope>/<storage>/resolve/<traits>.cpp
registration/mixed/<scope>/<storage>/binding/<traits>.cpp
```

The configuration is a path level, not a test-name suffix, so the same cell can
be exercised under multiple container front-ends without collisions.

### Global orchestration

Used by invoke, nested containers, scenarios, shared-cyclical graphs, and
dependency compositions:

```text
resolution/runtime/<container>/global/invoke/<form>.cpp
resolution/runtime/<container>/global/nested/<form>.cpp
resolution/runtime/<container>/global/scenario/<name>.cpp
resolution/runtime/<container>/global/cyclical/<graph>.cpp
resolution/runtime/<container>/global/composition/<profile>.cpp
```

These cases register more than one binding, so there is no single storage path.
The suite is the scenario/form; the test is the case.

### Compile-time classification

Used by constructor detection and conversion parity:

```text
classification/constructor/<backend>/<mode>/<shape>.cpp
classification/conversion/<form>.cpp
```

Static assertions only; no container.

## Remaining features by mechanism

The matrix treats every feature as an independent axis and multiplies it by
storage, scope, request form, and mode. Several features share one mechanism, so
they belong in one family with the mechanism's own degrees of freedom, and some
need no family at all because the row probes already execute them.

| group | features | shared mechanism | family | cases | | --- | --- | --- | ---
| --- | | assembly | `resolve_collection`, `resolve_keyed_collection`,
`construct_collection`, indexed collections | several bindings assembled into
one value | `global/collection/<mode>` | ~8 | | scope | `local_bindings`,
`nested_container` | where a binding is looked up | `global/scope/<case>` | ~5 |
| trait | `custom_allocator`, `custom_rtti`, `resolve_indexed` (DSL) | a
container trait swapped under the same rows | `configuration/<trait>` | ~3 | |
co-residence | `mixed_external_dependency`, `mixed_singular_ambiguity` | static
and runtime bindings in one container | `global/mixed/<case>` | ~3 | |
publication | `shared_cyclical` | two-phase publication of a cycle |
`global/cyclical/<graph>` | ~4 | | selection | `explicit_dependencies` |
constructor metadata chooses among same-typed dependencies | extension of
`global/construct` | ~2 | | composition | `dependency_composition` | the
dependency type is itself composed | mostly the atom request space;
`global/composition/<depth>` | ~3 | | table | `constructor_detection`,
conversion parity | compile-time type tables | `classification/...` | tables |

Features that share nothing must not be merged:

- assembly and publication are unrelated: one collects bindings, the other
  publishes edges before the graph is complete;
- `explicit_dependencies` selects between dependencies of the *same* type, while
  `dependency_composition` changes the dependency's *type*; one extends the
  construct shapes, the other is mostly atom rows;
- constructor detection and conversion parity share the emission mechanism
  (tables of static assertions) but not their axes.

Features that need no family:

| feature | why | | --- | --- | | `nested_wrappers`, `resolve_wrapper`,
`resolve_unique_wrapper` | the row probes already request
`shared_ptr<shared_ptr<T>>`, `unique_ptr<unique_ptr<T>>`, and deeper nestings |
| `construct_invoke` | `global/construct` and `global/invoke` shape families | |
`factory_override` | `global/producer` | | `custom_wrappers` |
`classification/wrapper` for the properties, plus one row with the custom
wrapper | | scenarios | each scenario belongs to the family owning its
mechanism; a regression keeps one named case there |

Granularity follows the mechanism, not the matrix product. A collection case
varies the collection shape and the registration mode; the element storage and
request form are the row probes. A cycle case varies the graph shape; the edge
storage is the row probe. That is roughly 35 runtime cases and two static tables
for the whole remainder, instead of the matrix's hundreds of rows.

## Migration and removal

The matrix is removed feature by feature. A feature is removable only when:

1. the model family covers every supported row the matrix generated for it, and
   `claim-check` shows `resolve`, `construct`, and `invoke` agreeing on every
   row the feature produces;
2. the rows are pinned by assertions, not just compiled;
3. the feature is listed in `FULLY_MODEL_COVERED_FEATURES` (or the matrix
   generator no longer emits it);
4. the matrix generator test still passes with the feature removed.

Order:

1. Done: `construct`, `invoke`, and the producer classes.
2. Collections: runtime, mixed, keyed, and the indexed container trait.
3. Configuration: allocator, RTTI, and the indexed DSL.
4. Mixed co-residence and the singular ambiguity.
5. Cycles: self, two-node, and three-node graphs.
6. Lookup scope: local bindings, child, grandchild, shadowing.
7. `explicit_dependencies` as a construct shape, then composition depth.
8. Classification tables: constructor detection axes and conversion parity.
9. Delete `test/matrix`, its axes, fixtures, scenarios, and the `legacy/`
   algebra.

The matrix generator test and `model_coverage.py` are the parity gate until step
8\. No feature is deleted from the matrix before its model family exists and the
generator test passes.
