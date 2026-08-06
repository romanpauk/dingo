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

## C++ Formatting

C++ sources are formatted with `clang-format` using the repository
`.clang-format` file. The required formatter is pinned as `clang-format` 21 in
the locked `uv` development environment.

Configure CMake with the pinned executable if the system formatter is missing or
has a different major version:

```bash
cmake -S . -B build -DDINGO_DEVELOPMENT_MODE=ON \
  -DDINGO_CLANG_FORMAT_EXE="$(uv run --locked python -c 'import shutil; print(shutil.which("clang-format"))')"
```

Development builds provide targets for updating and checking formatting:

```bash
cmake --build build -t format
cmake --build build -t check-format
```

The older explicit target names remain available as aliases:
`clang-format-update` and `clang-format-verify`.

The aggregate `check` target runs the repository checks used by CI:

```bash
cmake --build build -t check
```

## C++ Static Analysis

Development builds also provide clang-tidy and Include What You Use (IWYU)
targets:

```bash
cmake --build build -t check-tidy
cmake --build build -t check-headers
cmake --build build -t check-iwyu
```

On non-Windows builds, `check` depends on `check-tidy`; Windows skips clang-tidy
and IWYU, and still runs header self-compilation, formatting, and Markdown
verification. `check-tidy` runs clang-tidy 21 against the example translation
units, which gives the analyzer a concrete compile database for this header-only
library without pulling tests or benchmarks into the lint gate. Configure
non-Windows lint builds with `DINGO_EXAMPLES_ENABLED=ON` so those translation
units exist.

`check-headers` compiles one generated translation unit per standalone public
`.h` header, so every entry-point header must compile without relying on include
order. The `.hpp` files under `factory/detail` are composition fragments that
are intentionally included inside an existing namespace and are not standalone
entry points. `check-iwyu` repeats those isolated compilations with IWYU and
reports direct-include recommendations while still failing compilation errors.
IWYU is provided by the pinned `clang-tool-chain` package in `uv.lock`; its
downloaded toolchain is kept inside the build directory.

## Python Tooling

Python helper tooling for development is declared in `pyproject.toml` and locked
in `uv.lock`. `uv` is the required entry point for repo-owned Python tooling.

For the Markdown CMake targets (`md-update` / `md-verify`), `uv sync` is not
required first: CMake invokes `uv run --locked ...` directly.

Preparing the environment ahead of time is still optional:

```bash
uv sync
cmake --build build -t md-verify
```

CI uses the same locked `uv` environment.

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

## Compile-Time Regression Tests

The `check-compile-time` target protects the accumulated benefit of the
compile-time optimizations. It compiles focused translation units from
[`test/compile_time`](../test/compile_time) directly with Clang 22 and compares
the resulting metrics with the reviewed budget in
[`test/compile_time/budgets/clang-22.json`](../test/compile_time/budgets/clang-22.json).

Configure and run the target with the real Clang executable rather than a
`ccache` shim:

```bash
cmake -S . -B build-compile-time \
  -DDINGO_COMPILE_TIME_CHECK_ENABLED=ON \
  -DDINGO_COMPILE_TIME_COMPILER=/usr/bin/clang++ \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_CXX_STANDARD=17
cmake --build build-compile-time -t check-compile-time
```

The check requires Clang 22 and the matching target and standard-library
environment. CI supplies them through the pinned
[`docker/compile-time/Containerfile`](../docker/compile-time/Containerfile). The
compiler-specific target is intentionally separate from the portable `check`
target and runs in one dedicated Linux CI job.

The check uses deterministic proxies for compile cost:

- Clang class-instantiation, function-instantiation, and parsed-class counts
- counts of expensive Dingo template families selected by each fixture
- transitive-header and preprocessed-token counts for public-header fixtures
- compile-only structural guards that make bypassed fallback paths ill-formed
- peak resident memory with 5% headroom

All metrics except peak memory must not exceed the committed reference. Frontend
CPU time, wall time, and time-trace durations remain useful for local profiling,
but are deliberately excluded from CI decisions because shared-runner timing is
not reproducible enough.

The collector records the compiler identity, fixture and manifest digests,
reference value, and limit. A fixture edit therefore fails until its budget is
deliberately regenerated. Library implementation changes leave the fixture
digest intact and fail when they increase a protected counter. CI never updates
budgets automatically.

Budget changes require a new reference measurement with the pinned toolchain:

```bash
python3 tools/compile_time_check.py measure \
  --compiler /usr/bin/clang++ \
  --include-dir include \
  --output reference.json \
  --project-root . \
  --work-dir build-compile-time/reference
```

Generate the reviewed file from those measurements:

```bash
python3 tools/compile_time_check.py make-budget \
  --measurement reference.json \
  --reference-commit REFERENCE_COMMIT \
  --output test/compile_time/budgets/clang-22.json \
  --project-root .
```

Review every changed reference value and limit before accepting the new budget.
Lower measurements pass without a budget update; increases require explicit
review and regeneration. The target writes the latest measurements to
`build-compile-time/compile-time/observed.json`; CI uploads that file even when
the check passes.

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

- `dingo-toolchains:ubuntu-25.04-gcc-15` for GCC 15 builds on Ubuntu 25.04
- `dingo-toolchains:ubuntu-25.04-clang-20` for Clang 20 builds on Ubuntu 25.04

For an MSVC-style local workflow, build a local image from
`docker/ubuntu25-toolchains/Containerfile` with `TOOLCHAIN=msvc-wine`.
