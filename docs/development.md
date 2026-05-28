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

## Container Images

The CI toolchain images are documented under
[`docker/ubuntu25-toolchains/README.md`](../docker/ubuntu25-toolchains/README.md).

Linux image examples:

- `dingo-toolchains:ubuntu-25.04-gcc-15` for GCC 15 builds on Ubuntu 25.04
- `dingo-toolchains:ubuntu-25.04-clang-20` for Clang 20 builds on Ubuntu 25.04

For an MSVC-style local workflow, build a local image from
`docker/ubuntu25-toolchains/Containerfile` with `TOOLCHAIN=msvc-wine`.
