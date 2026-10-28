# Test Corpus Retirement

The previously generated cross-product corpus under `test/` has been retired. It
combined registration modes, scopes, stored types, exposed types, resolved
types, and container shapes, which made it enormous and made "is this covered"
answerable only by counting rows.

This page records what replaced it, so the mapping is not lost with the code.
The model itself is described in [Coverage Model](coverage-model.md).

## Structure

The retired features were regrouped by mechanism rather than by name. Features
that share a mechanism share a family; features that only looked similar keep
separate families; features whose rows the atom probes already execute need no
family at all.

```text
resolution/runtime/<configuration>/<scope>/<storage>/
    resolve.cpp      rows: <traits>/<binding>/<request>
    construct.cpp    the same rows, claim = construct
    invoke.cpp       the same rows, claim = invoke

resolution/runtime/global/construct/<argument>/<shape>.cpp
resolution/runtime/global/invoke/<shape>/<traits>.cpp
resolution/runtime/global/producer/<pair>.cpp
resolution/runtime/global/collection.cpp
resolution/runtime/global/cyclical.cpp
resolution/runtime/global/conversion/<case>.cpp
resolution/runtime/global/dependencies/<case>.cpp
resolution/runtime/global/lookup/<case>.cpp
resolution/runtime/global/wrapper/<case>.cpp
resolution/mixed/global/<case>.cpp
resolution/static/global/collection.cpp
resolution/runtime/<trait>/global/resolve.cpp
classification/constructor/<backend>/<mode>/<shape>.cpp
```

## Feature mapping

| retired feature | replacement | granularity | | --- | --- | --- | |
`resolve_concrete`, `resolve_wrapper`, `resolve_unique_wrapper` | row probes |
one row per form, binding, request, traits | | `nested_wrappers` | row probes
(`shared_ptr<shared_ptr<T>>`, `unique_ptr<unique_ptr<T>>`, …) | already rows | |
`annotated`, `variant`, `array`, `lifetime_counts` | row probes and behavior
families | one row or one behavior class | | `resolve_interface` | row probes
with the interface binding | one row | | `resolve_keyed` | row probes with the
keyed binding | one row | | `construct`, `construct_invoke` |
`global/construct/<argument>/<shape>` | 16 types x 5 shapes | | invoke family |
`global/invoke/<shape>` plus the `invoke` claim column | 19 shapes | |
`factory_override` | `global/producer/<pair>` | 6 pairs x producers x 3 claims |
| `resolve_collection`, `resolve_keyed_collection`, `construct_collection` |
`global/collection` | 5 cases x 2 modes | | `resolve_indexed` | the indexed
configuration (`runtime/indexed/...`) | one row set per key type | |
`custom_wrappers` | `classification/wrapper` plus `global/wrapper` | properties
\+ one row per wrapper | | `local_bindings`, `nested_container` | `global/lookup`
| 5 rules | | `explicit_dependencies` | `global/dependencies` | 2 declarations |
| `mixed_external_dependency`, `mixed_singular_ambiguity` | `mixed/global` | 2
cases | | `custom_allocator`, `custom_rtti` | `runtime/<trait>` configuration |
one case each | | shared-cyclical graphs | `global/cyclical` | 3 modes x storage
representations | | conversion parity | `global/conversion` | 6 cases plus
front-end parity | | constructor detection | `classification/constructor` | 2
backends x 2 modes x 15 shapes | | scenarios | the family owning the mechanism |
one named case per regression |

## What is gone

- the generated corpus under `test/`: its axes, fixtures, scenarios, templates,
  generator, and generator test;
- its coverage manifest, which was the parity gate while features moved;
- the legacy request/binding algebra that only that corpus consumed.

## What replaced the parity gate

The retired generator test proved that a feature was not dropped before its
replacement existed. That job is now split:

- `claim-check` compares `resolve`, `construct`, and `invoke` on every row and
  fails on an undeclared divergence or a missing outcome;
- `generated-check` regenerates every family and fails when a generated file is
  out of date;
- `pytest spec/tests` pins the structure of every family: paths, case paths,
  declared gaps, and the row set each claim file must contain.

## Remaining work

The model is now the only generated corpus, but two threads continue:

- the resolve probe still reports overclaims (`resolve-check`), which is a
  library-versus-model disagreement rather than a missing test;
- new mechanisms get a family and, where they apply to every row, a claim
  column.
