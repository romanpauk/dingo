# Test Hierarchy

The generated tests share one path hierarchy:

```text
<phase>/<container>/<scope>/<storage>/<category>/<traits>/<binding>/<request>
```

- **phase** — what is being tested: `classification`, `registration`, or
  `resolution`;
- **container** — how it is tested: `compile`, `runtime`, or `static`; the level
  is omitted when the phase is compile-time by definition;
- **scope** — `shared`, `unique`, `external`, `shared_cyclical`, or empty;
- **storage** — a storage path (`plain`, `shared_handle`, …) or empty;
- **category** — the test family (`constructor`, `route`, `binding`, `resolve`,
  `behavior`, `annotated`, `keyed`, `const`, `interface`, …);
- **traits** — `copyable` or `movable`;
- **binding** — `concrete`, `annotated`, `keyed`, `interface`,
  `annotated_interface`, or empty;
- **request** — the structural request slug (`t`, `t_ref`, `optional_t`, …).

The root of the tree is the capability form, not the claim. A claim file is a
sibling of `resolve.cpp` inside the form, and the claim is the first segment of
the case path after the form:

```text
resolution/runtime/shared/plain/resolve.cpp      # rows: resolve
resolution/runtime/shared/plain/construct.cpp    # same rows: construct
resolution/runtime/shared/plain/invoke.cpp       # same rows: invoke
```

Each of those files is rendered from `resolve-manifest.json`, so the row sets
are identical by construction. A claim that is not tied to a form (constructor
shapes, collections, indexed lookup) lives under `global` instead:

```text
resolution/runtime/global/construct/lvalue_reference/copyable.cpp
resolution/static/global/collection/interface.cpp
resolution/runtime/indexed/global/size_t/resolve.cpp
```

The case path is the identity used by manifests, baselines, and reports. A
generated file groups cases up to the category:

```text
resolution/runtime/shared/plain/resolve.cpp
resolution/runtime/global/construct/lvalue_reference/copyable.cpp
classification/constructor/zero_argument.cpp
registration/static_asserts/shared/plain/binding.cpp
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

`compile` means a compile-time probe (`static_assert` or type instantiation),
not a container type. `classification` is compile-time by definition, so it has
no `compile` level: `classification/constructor/...`, not
`classification/constructor/...`. `registration/static_asserts/...` means the
binding composition is checked at compile time; a future
`registration/runtime/...` would exercise duplicate-registration and key/tag
behavior. `resolution/static_asserts/...` is compile-time route publication;
`resolution/runtime/...` and `resolution/static/...` are the runtime and static
container front-ends.

### classification

What the library recognizes before anything is registered. Constructor
detection, wrapper properties, selection rules, storage facts, and route
publication live here.

| category | contents | | --- | --- | | `constructor` | constructor detection
backends, modes, and shapes: zero-argument aggregates, dependency constructors,
array shapes, move-only elements, copy-only types | | `wrapper` | wrapper
properties (`is_owning`, `is_sharing`, `is_nullable`, `can_copy_out`,
`can_take`, …) | | `selection` | selection policy and source status rules | |
`storage` | storage-frame exposure and form facts | | `route` | compile-time
route publication for a form and shape |

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

| family | target phase / container / category | | --- | --- | | `resolve_cases`
| `resolution/runtime/<scope>/<storage>/resolve` | | `behavior_cases` |
`resolution/runtime/<scope>/<storage>/behavior` | | `type_behavior` |
`resolution/runtime|static/<scope>/<storage>/route` | | `annotated_cases` |
`resolution/runtime/<scope>/<storage>/annotated` | | `keyed_cases` |
`resolution/runtime/<scope>/<storage>/keyed` | | `const_cases` |
`resolution/runtime/<scope>/<storage>/const` | | `interface_runtime` |
`resolution/runtime/<scope>/<storage>/interface` | | `binding_matrix` |
`registration/static_asserts/<scope>/<storage>/binding` | | `storage policies` |
`classification/<scope>/<form>/storage` | | `route_probes` |
`classification/<scope>/<form>/route` | | `type_matrix` / `trait_matrix` |
`classification/<scope>/<form>/route` | | `wrapper_properties` |
`classification/global/wrapper` | | `selection_rules` |
`classification/global/selection` | | constructor detection matrix |
`classification/global/constructor/<backend>/<mode>/<shape>` |

## Migration order

1. Done: the resolve probe lives under `resolution/runtime/…` and its path is
   the outcome and manifest identity.
2. Done: `classification/constructor/…` is the first classification slice.
3. Done: `binding_matrix` lives under `registration/static_asserts/…`, one file
   per storage.
4. Move `storage policies`, `selection_rules`, `wrapper_properties`, and the
   route matrices to `classification/…`.
5. Move the runtime behavior families to the matching `resolution/…` categories.
6. Retire the standalone translation units once their categories cover the same
   cases and the coverage report is driven by the hierarchy.
