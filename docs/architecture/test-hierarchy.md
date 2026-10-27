# Test Hierarchy

The generated tests share one path hierarchy:

```text
<phase>/<container>/<scope>/<storage>/<category>/<traits>/<binding>/<request>
```

- **phase** — `detection`, `registration`, or `resolution`;
- **container** — `compile`, `runtime`, or `static`;
- **scope** — `shared`, `unique`, `external`, `shared_cyclical`, or empty;
- **storage** — a storage path (`plain`, `shared_handle`, …) or empty;
- **category** — the test family (`constructor`, `route`, `binding`, `resolve`,
  `behavior`, `annotated`, `keyed`, `const`, `interface`, …);
- **traits** — `copyable` or `movable`;
- **binding** — `concrete`, `annotated`, `keyed`, `interface`,
  `annotated_interface`, or empty;
- **request** — the structural request slug (`t`, `t_ref`, `optional_t`, …).

The case path is the identity used by manifests, baselines, and reports. A
generated file groups cases up to the category:

```text
resolution/runtime/shared/plain/resolve.cpp
detection/compile/constructor/zero_argument.cpp
registration/compile/shared/plain/binding.cpp
```

The GTest suite is the same prefix with underscores, and the test name is the
path tail:

```cpp
TEST(resolution_runtime_shared_plain_resolve, copyable_concrete_t_ref) {
  RecordProperty(
      "path",
      "resolution/runtime/shared/plain/resolve/copyable/concrete/t_ref");
}
```

## Phases

### detection

What the library recognizes before anything is registered.

| category | contents | | --- | --- | | `constructor` | constructor detection
backends, modes, and shapes: zero-argument aggregates, dependency constructors,
array shapes, move-only elements, copy-only types | | `wrapper` | wrapper
properties (`is_owning`, `is_sharing`, `is_nullable`, `can_copy_out`,
`can_take`, …) | | `selection` | selection policy and source status rules | |
`storage` | storage-frame exposure and form facts | | `route` | compile-time
route publication for a cell and shape |

### registration

What a storage can be registered as.

| category | contents | | --- | --- | | `binding` |
`dingo::bindings<dingo::bind<scope, storage, interfaces, key>>` composition for
concrete, annotated, keyed, interface, and annotated-interface bindings | |
`annotated` | annotated registration and negative tag cases | | `keyed` | keyed
registration and duplicate-key cases | | `interface` | interface registration,
multiple interfaces, and consuming-scope limits |

### resolution

What resolves from a registered storage.

| category | contents | | --- | --- | | `route` | compile-time route targets,
kind, retention, steps, and shape | | `resolve` | storage-driven `resolve`
probes, one test per request, skipped on failure | | `behavior` | identity,
copy/move/construction counts, lifetime balance | | `annotated` | annotated
resolution | | `keyed` | keyed resolution | | `const` | const storage resolution
| | `interface` | interface resolution, including multiple-interface aliasing |

## Current families

| family | target phase / container / category | | --- | --- | | `resolve_cells`
| `resolution/runtime/<scope>/<storage>/resolve` | | `behavior_cells` |
`resolution/runtime/<scope>/<storage>/behavior` | | `type_behavior` |
`resolution/runtime|static/<scope>/<storage>/route` | | `annotated_cells` |
`resolution/runtime/<scope>/<storage>/annotated` | | `keyed_cells` |
`resolution/runtime/<scope>/<storage>/keyed` | | `const_cells` |
`resolution/runtime/<scope>/<storage>/const` | | `interface_runtime` |
`resolution/runtime/<scope>/<storage>/interface` | | `binding_matrix` |
`registration/compile/<scope>/<storage>/binding` | | `storage_cells` |
`detection/compile/<scope>/<form>/storage` | | `route_cells` |
`detection/compile/<scope>/<form>/route` | | `type_matrix` / `trait_matrix` |
`detection/compile/<scope>/<form>/route` | | `property_cells` |
`detection/compile/global/wrapper` | | `selection_rules` |
`detection/compile/global/selection` | | constructor detection matrix |
`detection/compile/global/constructor/<backend>/<mode>/<shape>` |

## Migration order

1. Done: the resolve probe lives under `resolution/runtime/…` and its path is
   the outcome and manifest identity.
2. Done: `detection/compile/constructor/…` is the first detection slice.
3. Done: `binding_matrix` lives under `registration/compile/…`, one file per
   storage.
4. Move `storage_cells`, `selection_rules`, `property_cells`, and the route
   matrices to `detection/compile/…`.
5. Move the runtime behavior families to the matching `resolution/…` categories.
6. Retire the standalone translation units once their categories cover the same
   cases and the coverage report is driven by the hierarchy.
