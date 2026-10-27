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
partial | `resolution/.../nested` | | `custom_wrappers` | partial (properties
only) | `classification/.../wrapper` + `resolution/.../nested` | |
`constructor_detection` | partial (shape slice) |
`classification/constructor/<backend>/<mode>/<shape>` | | `construct` | not
covered | `resolution/.../construct` | | `construct_collection` | not covered |
`resolution/.../construct/collection` | | `local_bindings` | not covered |
`resolution/.../local` | | `nested_container` | not covered |
`resolution/.../container` | | `factory_override` | not covered |
`registration/.../factory` | | `explicit_dependencies` | not covered |
`resolution/.../dependencies` | | `mixed_external_dependency` | not covered |
`resolution/.../mixed` | | `mixed_singular_ambiguity` | not covered |
`resolution/.../mixed` | | `custom_allocator` | not covered |
`resolution/.../config/allocator` | | `custom_rtti` | not covered |
`resolution/.../config/rtti` | | `resolve_collection` | not covered |
`registration/.../collection` | | `resolve_keyed_collection` | not covered |
`registration/.../collection` | | `resolve_indexed` | not covered |
`registration/.../indexed` | | `construct_invoke` / invoke family | not covered
| `resolution/.../invoke` | | `scenario` family | not covered |
`resolution/.../scenario` | | `shared_cyclical` family | not covered |
`resolution/.../cyclical` | | dependency-composition families | not covered |
`resolution/.../composition` | | conversion parity | not covered |
`classification/conversion` |

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
resolution/runtime/<scope>/<storage>/construct/<traits>.cpp
registration/runtime/<scope>/<storage>/factory/<kind>.cpp
resolution/runtime/<scope>/<storage>/local/<traits>.cpp
```

One file per cell and category; one test per case. The test registers the
storage and asserts identity, value, or counts.

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

## How each feature is tested

| feature | structure | assertion | | --- | --- | --- | | `construct` | storage
cell | `construct<Constructed>()`, dependency value | | `construct_collection` |
global / static collection | collection size and element ids | |
`factory_override` | storage cell | registered factory result value | |
`resolve_collection` | static collection | collection size and element values |
| `resolve_keyed_collection` | static collection + key | keyed collection size
and values | | `resolve_indexed` | runtime/indexed cell | two indexed
registrations resolve by key | | `local_bindings` | storage cell | dependency
attached to one registration | | `nested_container` | global nested | child
resolves from parent/local scope | | `explicit_dependencies` | storage cell |
constructor metadata selects the dependency | | `mixed_external_dependency` |
registration/mixed | static anchor + runtime external dependency | |
`custom_allocator` | runtime/allocator | container bookkeeping uses the
allocator | | `custom_rtti` | runtime/rtti | lookup works with the custom
provider | | `invoke` | global invoke | callable result and dependency dispatch
| | scenarios | global scenario | complete regression behavior | |
shared-cyclical | global cyclical | two-node cycle resolves through each edge |
| dependency composition | global composition | recursive composition
resolves/invokes | | conversion parity | classification | conversion matrix
parity | | constructor detection | classification | backend/mode/shape parity |

## Migration and removal

The matrix is removed feature by feature. A feature is removable only when:

1. the model family covers every supported row the matrix generated for it;
2. the rows are pinned by assertions, not just compiled;
3. the feature is listed in `FULLY_MODEL_COVERED_FEATURES` (or the matrix
   generator no longer emits it);
4. the matrix generator test still passes with the feature removed.

Order:

1. Finish `construct` (unique/cyclical) and the remaining factory forms.
2. Add the runtime/mixed/indexed collection forms and the container
   configuration level.
3. Add local bindings and nested containers.
4. Add explicit/mixed dependencies, allocator, and RTTI.
5. Add invoke.
6. Add scenarios, shared-cyclical graphs, and dependency compositions.
7. Add conversion parity.
8. Delete `test/matrix`, its axes, fixtures, scenarios, and the `legacy/`
   algebra.

The matrix generator test and `model_coverage.py` are the parity gate until step
8\. No feature is deleted from the matrix before its model family exists and the
generator test passes.
