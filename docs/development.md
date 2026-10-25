# Development

This document covers repository development tasks. The library itself remains
header-only for consumers.

## Development Build

Development builds enable tests, benchmarks, and runnable examples through
`DINGO_DEVELOPMENT_MODE=ON`.

```bash
cmake -S . -B build -DDINGO_DEVELOPMENT_MODE=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Individual development features can be controlled with the matching CMake
options, including:

- `DINGO_TESTING_ENABLED`
- `DINGO_BENCHMARK_ENABLED`
- `DINGO_EXAMPLES_ENABLED`
- `DINGO_LIT_TESTS_ENABLED`

## Quality Targets

Run a quality target with `cmake --build build -t <target>`.

- `format`: runs `sources-format` and `markdown-format`.
- `sources-format`: updates C++ files under `benchmark`, `examples`, `include`,
  and `test`.
- `sources-format-check`: verifies the same C++ files.
- `markdown-format`: updates Markdown files under the repository root, `docs`,
  and `docker`.
- `markdown-format-check`: verifies the same Markdown files.
- `format-check`: runs `sources-format-check` and `markdown-format-check`.
- `layers-check`: verifies that headers include only headers of lower layers or
  of their own directory, as described in
  [Header Layers](architecture/overview.md#header-layers).
- `includes-compile`: compiles one generated translation unit per standalone
  public header.
- `includes-tidy`: runs clang-tidy on those generated translation units on
  non-Windows builds.
- `includes-check`: checks includes on those generated translation units on
  non-Windows builds.
- `test-cost`: reports the per-translation-unit compile time and object size of
  the generated test suites. It is a report, not a gate; see
  [Test Compile Cost](#test-compile-cost).
- `check`: runs all verification targets available on the platform.

The development tools are provided by the locked `uv` environment.

## Generated Matrix Tests

`dingo_matrix_test` is generated from the axis model in
[`test/matrix/README.md`](../test/matrix/README.md). The model combines
features, registration modes, scopes, stored types, exposed types, resolved
types, and container shapes, then filters invalid combinations before emitting
GoogleTest sources.

The hand-written C++ portion is organized by role: `common/` provides matrix
plumbing, `containers/` and `fixtures/` define axis inputs, `policies/` contains
generic row behavior, and `scenarios/` contains complete behavioral and
regression cases. Axis definitions declare the headers needed by each generated
shard so unrelated scenarios are not compiled together. Registration suites
split their cases into bounded compilation units so one suite cannot dominate
the build; the case set and runner stay the same.

## Generated Cell Coverage

The storage and selection models under [`tools/model/`](../tools/model)
enumerate the built-in cells independently of the C++ headers. The generator
writes one translation unit for the full storage scope-by-form product, one for
every selection policy and source status, one route probe for every supported
storage cell, one type matrix that applies every request shape to every storable
cell and writes `type-matrix.md`, one interface route probe for storage exposed
through `dingo::interfaces<...>`, one behavior class per supported scope/form
cell in the applicable runtime and static containers, and type behavior units
that resolve every supported top-level shape through the runtime container and
through `static_container` for every statically bindable cell, checking
identity, ownership transfer, and the lifetime balance. The type matrix reports
both the direct route, the lifted request, and whether the route is safe at the
container boundary: lvalue borrows from consuming scopes serve dependents but
not top-level callers, so those rows are explicit gaps rather than hidden
support. A wrapper the cell does not store is lifted from its value; owning
handles require a consumed value. Every remaining miss carries a structural
reason, and the report summarizes the misses by reason with a count and an
example. Runtime value routes address the source a consuming scope already
materialized in the context, so value requests construct the result with one
move instead of copying a context value and moving it out. The route ledger
classifies every scope/form/category row as supported, unavailable with a
reason, or a tracked gap. `route_recipes.py` derives the operation-kind and
retention sequence from the form's stored wrapper and the source shape the scope
publishes, so the ledger no longer keeps one hand-written sequence per cell. The
route probes check value, object, and rvalue availability against that ledger,
require every published route target to be reachable from both the static and
runtime request matchers, check that the scope's access satisfies every route,
and check the derived operation-kind and source-retention sequences. The
interface probes ask the same model for their value/object/rvalue counts by
exposed form and copyable interface value. It also generates the committed
`include/dingo/storage/scopes.gen.h` header, which owns the built-in
`storage_scope_policy` data. Compile-time assertions compare the generated
expectation with the library; a missing cell, changed exposure, or stale catalog
fails the build.

Behavior classes are data-driven too. `behavior_cells.py` owns a registration
description and an expectation entry per form, derives the scope and container
product from the route ledger, and requires every supported scope/form cell to
be either behavior-tested or listed in `BEHAVIOR_GAPS` with a reason. Adding a
form is one table entry instead of a hand-written test per scope. A behavior
class asserts the request identity relation, construction count, optional copy
and move counts, array element identity, leaf address, and the lifetime balance
`destructions == constructions + copies + moves`.

The generated sources live in the build tree, so consumer builds never run
Python. The `cells-check` target regenerates them and verifies that they are up
to date, and the model's pytest suite checks the catalog against the library
declarations in `include/dingo/storage` and `include/dingo/binding`.

```bash
cmake --build build -t cells-check
uv run --locked pytest -q tools/model
```

## Code Size Tests

The lit suite also contains code-generation probes in
[`test/lit/codegen-static-probes.cpp`](../test/lit/codegen-static-probes.cpp).
They compile representative static and runtime resolution paths, inspect symbol
sizes with `nm`, and inspect selected tiny static probes with `objdump`.

The checks are implemented by
[`test/lit/check_codegen_probe_sizes.py`](../test/lit/check_codegen_probe_sizes.py)
and
[`test/lit/check_codegen_probe_instructions.py`](../test/lit/check_codegen_probe_instructions.py).
Run them through the normal lit test target:

```bash
ctest --test-dir build --output-on-failure -R dingo_lit_tests
```

## Compile-Time Profiling

For template compile-cost investigations, use
[`time-trace`](https://github.com/romanpauk/time-trace). It wraps a direct Clang
compile, reads Clang's time-trace JSON, and writes synthetic `perf.data` that
can be inspected with normal `perf` commands.

This is useful when a registration or resolution change affects compile time and
the expensive template instantiations need to be identified directly.

## Test Compile Cost

`test-cost` reports how much each generated test translation unit costs:

```bash
cmake --build build -t test-cost
```

It reads `build/compile_commands.json`, recompiles every generated test source,
and prints the slowest units with their object sizes, then writes the full
report to `build/cost/test-tus.json`. The deterministic values (translation unit
count and object size) are worth comparing between revisions; wall time is a
trend because it depends on machine load.

The model suites are split by catalog, so their units are small
(`storage_cells`, `route_cells`, and `selection_rules` are around one second
each, and `behavior_cells` is the largest). When one unit grows past a few
seconds, split it by form or cost class before adding more cases. Compact
translation units preserve parallel build time without duplicating the shared
kernel's instantiations.

## Cost Comparisons

[`tools/cost/measure.py`](../tools/cost/measure.py) records deterministic probe
sizes and optional Clang instantiation counts, then compares two records. Use it
when a refactor is supposed to preserve binary size or compile cost:

```bash
uv run --locked python tools/cost/measure.py record \
    --include /path/to/baseline/include \
    --source test/lit/codegen-static-probes.cpp \
    --out build/cost/baseline.json --time-trace
uv run --locked python tools/cost/measure.py record \
    --include include \
    --source test/lit/codegen-static-probes.cpp \
    --out build/cost/current.json --time-trace
uv run --locked python tools/cost/measure.py compare \
    build/cost/baseline.json build/cost/current.json
```

The hard gate is text/data/bss size, preprocessed header bytes, and, when
present, the number of `dingo::` events in the time trace. Wall time is printed
as a trend only, because it is too noisy to fail a build.

The model has its own differential baseline in `test/baselines/model.json`. It
pins the matrix outcomes and the actual behavior runtime outcomes, including
construction/copy/move/destruction counts and the result or exception class.
`ctest` runs `tools/model/outcome_baseline.py check` after the model test; an
intended change is recorded with `update --reason "..."`, which rewrites the
baseline and appends the declared differences to
`test/baselines/model-changes.json`.

`cmake --build build -t cost-record` records the current probe under
`build/cost/current.json` with the configured compiler. Use the explicit
`record` commands above when comparing against a baseline include tree.

## Container Images

The CI toolchain images are documented under
[`docker/ubuntu25-toolchains/README.md`](../docker/ubuntu25-toolchains/README.md).

Linux image examples:

- `dingo-toolchains:ubuntu-26.04-gcc-15` for GCC 15 builds on Ubuntu 26.04
- `dingo-toolchains:ubuntu-26.04-clang-20` for Clang 20 builds on Ubuntu 26.04
- `dingo-toolchains:ubuntu-26.10-gcc-16` for GCC 16 builds on Ubuntu 26.10

For an MSVC-style local workflow, build a local image from
`docker/ubuntu25-toolchains/Containerfile` with `TOOLCHAIN=msvc-wine`.
