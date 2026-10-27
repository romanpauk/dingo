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
2. Rebuild construction, factory, collection, indexed, and container-composition
   features as model families.
3. Move the invoke, scenario, cyclical, dependency-composition, and conversion
   parity families.
4. Delete `test/matrix`, its axes, fixtures, scenarios, and the `legacy/`
   request/binding algebra once no feature remains.
