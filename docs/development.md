# Development

This document covers repository development tasks. For consumers, the library is
header-only.

## Development Build

`DINGO_DEVELOPMENT_MODE=ON` enables tests, benchmarks and runnable examples.

```bash
cmake -S . -B build -DDINGO_DEVELOPMENT_MODE=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

These CMake options control individual development features:

- `DINGO_TESTING_ENABLED`
- `DINGO_BENCHMARK_ENABLED`
- `DINGO_EXAMPLES_ENABLED`
- `DINGO_LIT_TESTS_ENABLED`

## Quality Targets

Run a quality target with `cmake --build build -t <target>`. The locked `uv`
environment provides the development tools.

- `format`: runs `sources-format` and `markdown-format`.
- `sources-format`: formats the C++ files under `benchmark`, `examples`,
  `include` and `test`.
- `sources-format-check`: verifies the same C++ files.
- `markdown-format`: formats `README.md` and the Markdown files under `docs` and
  `docker`.
- `markdown-format-check`: verifies the same Markdown files.
- `format-check`: runs `sources-format-check` and `markdown-format-check`.
- `layers-check`: verifies that a header includes only headers of lower layers
  or of its own directory. See
  [Header Layers](architecture/overview.md#header-layers).
- `includes-compile`: compiles one generated translation unit for each public
  header.
- `includes-tidy`: runs clang-tidy on those translation units.
- `includes-check`: checks the includes of those translation units.
- `check`: runs all verification targets that are available on the platform.
- `complexity-report`: prints where the complexity of the headers is. It is a
  report, not part of `check`. See [Complexity Report](#complexity-report).
- `conversion-check`: checks what the conversion layer of each registration
  publishes, and the copies and the moves of its conversions, against the
  [Specification](specification.md#conversion-check).
- `emptiness-check`: checks what each registration whose stored object holds no
  leaf gives the requests that are defined when a layer is empty, against the
  [Specification](specification.md#emptiness-check).
- `storage-check`: checks the copies and the moves of the storage of each
  registration against the [Specification](specification.md#model-probe).
- `resolution-check`: checks the copies and the moves when a leaf is passed on
  in each position, against the [Specification](specification.md#model-probe).
- `model-check-small`, `model-check-medium`, `model-check-large`: run both model
  probes of the [Specification](specification.md). See
  [Model Checks](#model-checks).

With GCC and clang, `check` includes `conversion-check`, `emptiness-check`,
`storage-check`, `resolution-check` and `model-check-small`. `includes-tidy` and
`includes-check` are not available on Windows. The `sources-format*`,
`includes-tidy` and `includes-check` targets need `DINGO_CLANG_TOOLS_ENABLED=ON`
(the default).

## Complexity Report

[`tools/complexity.py`](../tools/complexity.py) ranks the headers to find
candidates for simplification. The report has these sections:

- `functions`: function bodies by cyclomatic complexity, measured by lizard.
- `headers`: for each header, the template declarations, specializations,
  `if constexpr` branches, SFINAE constructs and type definitions. It also
  counts the definitions that are named at most once outside their definition.
- `modules`: non-comment lines for each directory under `include/dingo`.
- `duplicates`: repeated code blocks, found by lizard.
- `compile`: template instantiation time for each template, from clang
  `-ftime-trace`, for the translation units selected with `--profile`.

The first four sections run by default:

```bash
uv run tools/complexity.py --top 30
```

For compile-time profiling, the tool reads `compile_commands.json` from the
build directory and recompiles the selected units with clang:

```bash
uv run tools/complexity.py --sections modules --build-dir build \
  --profile test/container/dependency_chain.cpp \
  --profile test/registration/runtime_registration.cpp
```

The rankings are heuristics to compare revisions. A definition that is named
once is a candidate for inlining, not a defect. The time of an instantiation
excludes the instantiations nested in it.

## Model Checks

The model checks compare the library with its [Specification](specification.md).
They use the [Model Probe](specification.md#model-probe) and the
[Constructor Detection Probe](specification.md#constructor-detection-probe).

```bash
cmake --build build -t model-check-small
```

The sizes probe these covers of the dimensions:

- `model-check-small`: every pair of values.
- `model-check-medium`: every three values.
- `model-check-large`: every combination.

A model check fails on a defect. It reads the diagnostics of GCC and clang.

`model-check-small` writes these files to `build/model-small/`:

- `probe.json`: the outcome of each cell of the model probe.
- `probe.md`: a summary, with a table for each container, leaf, interfaces,
  selection and operation.
- `detection.json`, `detection.md`: the same for the detection probe.

`model-check-medium` and `model-check-large` write to `build/model-medium/` and
`build/model-large/`. A target runs again only when its inputs change: the
headers, the specification, the probe headers, the locked environment, or the
compiler and flags.

When you run `spec/probe.py` directly, `--cover pairs|triples|all` selects the
[cover](specification.md#model-probe). `--only <regex>` probes only the
registrations with a name that matches the expression.

The probes limit the memory of the compilers:

- The probes run one compiler for each processor, at most as many as half of the
  available memory holds. `--jobs <n>` sets a different number.
- Each compiler runs in an address space of 1 GiB (`MEMORY_LIMIT` in
  [`spec/generator/driver.py`](../spec/generator/driver.py)).
- A program holds as many probes as fit in three quarters of that limit. The
  peak memory of earlier compiles with the same container and position gives
  this number.
- When the compiler runs out of memory, the probes split the program into
  smaller programs. A probe that does not fit in a program alone is a defect.

## Unit Tests

`dingo_unit_test` tests what the [Specification](specification.md) does not
cover, one test for each behavior. A test is in the directory of what it tests:

- `test/lookup`: collections and lookups.
- `test/container/parent`: parent containers.
- `test/factory`: the kinds of factory and constructor detection.
- `test/resolution`: wrappers, callables and identity across requests.
- `test/runtime`: regressions of the runtime container.

## Code Size Tests

The lit suite contains code-generation probes in
[`test/lit/codegen-static-probes.cpp`](../test/lit/codegen-static-probes.cpp).
They compile representative static and runtime resolution paths. They inspect
symbol sizes with `nm` and selected small static probes with `objdump`.

[`test/lit/check_codegen_probe_sizes.py`](../test/lit/check_codegen_probe_sizes.py)
and
[`test/lit/check_codegen_probe_instructions.py`](../test/lit/check_codegen_probe_instructions.py)
implement the checks. Run them through the lit test:

```bash
ctest --test-dir build --output-on-failure -R dingo_lit_tests
```

## Compile-Time Profiling

To find the compile cost of templates, use
[`time-trace`](https://github.com/romanpauk/time-trace). It wraps a direct Clang
compile, reads the time-trace JSON of Clang, and writes a synthetic `perf.data`.
Inspect that file with the usual `perf` commands.

Use it to find the expensive template instantiations when a change affects
compile time.

## Container Images

[`docker/ubuntu25-toolchains/README.md`](../docker/ubuntu25-toolchains/README.md)
documents the CI toolchain images. Linux images:

- `dingo-toolchains:ubuntu-26.04-gcc-15`: GCC 15 on Ubuntu 26.04.
- `dingo-toolchains:ubuntu-26.04-clang-20`: Clang 20 on Ubuntu 26.04.
- `dingo-toolchains:ubuntu-26.10-gcc-16`: GCC 16 on Ubuntu 26.10.

For a local MSVC workflow, build an image from
`docker/ubuntu25-toolchains/Containerfile` with `TOOLCHAIN=msvc-wine`.
