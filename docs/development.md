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
- `complexity-report`: prints where the complexity of the headers is; it is a
  report and is not part of `check`. See
  [Complexity Report](#complexity-report).
- `model-check`: resolves every request of every registration in both containers
  and fails on a defect; it is part of `check` with GCC and clang, whose
  diagnostics it reads. See [Model Probe](#model-probe).

The development tools are provided by the locked `uv` environment.

## Complexity Report

[`tools/complexity.py`](../tools/complexity.py) ranks the headers to find
candidates for simplification. Its sections are:

- `functions`: function bodies by cyclomatic complexity, measured by lizard.
- `headers`: template declarations, specializations, `if constexpr` branches,
  SFINAE constructs and type definitions per header, and how many of those
  definitions are named at most once outside their definition.
- `modules`: non-comment lines per directory under `include/dingo`.
- `duplicates`: repeated code blocks, found by lizard.
- `compile`: template instantiation time per template, from clang
  `-ftime-trace`, for the translation units selected with `--profile`.

The first four sections run by default:

```bash
uv run tools/complexity.py --top 30
```

Compile-time profiling reads `compile_commands.json` from the build directory
and recompiles the selected units with clang:

```bash
uv run tools/complexity.py --sections modules --build-dir build \
  --profile test/container/dependency_chain.cpp \
  --profile test/registration/runtime_registration.cpp
```

The rankings are heuristics to compare between revisions. A definition named
once is a candidate for inlining, not a defect, and an instantiation is charged
its own time, excluding the instantiations nested in it.

## Model Probe

[`test/model/probe.py`](../test/model/probe.py) asks the library what every
combination of container, scope, stored form, leaf, binding, operation and
request does. The operations make the request as the caller of `resolve`, as the
dependency of the constructor of a class in the `unique`, `shared` or
`shared_cyclical` scope, as the parameter of an invoked callable, and with
`construct`. Nothing in it decides what should work: each registration is
compiled into a program with [`test/model/probe.h`](../test/model/probe.h),
which registers it, resolves each request twice and records the outcome. A
request the library rejects at compile time is recorded with the message of its
`static_assert`: the compiler names the requests that fail by their lines in the
program, and the others are compiled again without them. The header is
precompiled once.

A resolved request is also checked: its result has to reach the registered leaf,
a scope that keeps its instance has to return the same one by reference, a
unique scope a new one, a kept class may not refer to a leaf that is gone, and
nothing may leak. The probe fails when a request does not compile without a
`static_assert`, crashes or fails these checks, or when the two containers
disagree on a registration both accept.

```bash
cmake --build build -t model-check
```

writes the outcome of every cell to `build/model/baseline.json` and a summary
with a table per container, leaf and binding to `build/model/baseline.md`.
`--only <text>` probes the registrations whose name contains the text.

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

## Container Images

The CI toolchain images are documented under
[`docker/ubuntu25-toolchains/README.md`](../docker/ubuntu25-toolchains/README.md).

Linux image examples:

- `dingo-toolchains:ubuntu-26.04-gcc-15` for GCC 15 builds on Ubuntu 26.04
- `dingo-toolchains:ubuntu-26.04-clang-20` for Clang 20 builds on Ubuntu 26.04
- `dingo-toolchains:ubuntu-26.10-gcc-16` for GCC 16 builds on Ubuntu 26.10

For an MSVC-style local workflow, build a local image from
`docker/ubuntu25-toolchains/Containerfile` with `TOOLCHAIN=msvc-wine`.
