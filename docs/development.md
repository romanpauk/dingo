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
shard so unrelated scenarios are not compiled together.

## Generated Cell Coverage

The storage and selection models under [`tools/model/`](../tools/model)
enumerate the built-in cells independently of the C++ headers. The generator
writes one translation unit for the full storage scope-by-form product, one for
every selection policy and source status, one route probe for every supported
storage cell, and one behavior class for shared/unique plain bindings in the
runtime and static containers. The route ledger classifies every
scope/form/category row as supported, unavailable with a reason, or a tracked
gap. The route probes check value, object, and rvalue availability against that
ledger, require every published route target to be reachable from both the
static and runtime request matchers, check that the scope's access satisfies
every route, and check the modeled operation-kind and source-retention
sequences. It also generates the committed `include/dingo/storage/scopes.gen.h`
header, which owns the built-in `storage_scope_policy` data. Compile-time
assertions compare the generated expectation with the library; a missing cell,
changed exposure, or stale catalog fails the build.

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

The hard gate is text/data/bss size and, when present, the number of `dingo::`
events in the time trace. Wall time is printed as a trend only, because it is
too noisy to fail a build.

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
