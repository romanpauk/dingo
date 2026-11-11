# Ubuntu toolchain images

Container images for local development and CI.

Each build of `Containerfile` installs one toolchain and the common development
tools. The `TOOLCHAIN` build argument selects the toolchain. It accepts `gcc-*`,
`clang-*`, `msvc` and `msvc-wine`. The `BASE_IMAGE` build argument selects the
Ubuntu release. The default is `ubuntu:26.04`. A toolchain that Ubuntu 26.04
does not package is built on a newer release.

Linux CI builds and tests with these images:

- `dingo-toolchains:ubuntu-26.04-gcc-15`: C++23 and C++26, with sanitizers.
- `dingo-toolchains:ubuntu-26.04-clang-20`: C++23 and C++26, with sanitizers.
- `dingo-toolchains:ubuntu-26.10-gcc-16`, built with `BASE_IMAGE=ubuntu:26.10`:
  C++23 with sanitizers, C++26 without sanitizers. With sanitizers in C++26
  mode, GCC 16 reports false uninitialized-variable warnings.

For MSVC, the commands below build and run the local tag
`dingo-toolchains:ubuntu-26.04-msvc-wine`.

Every image contains:

- `cmake`
- `ninja`
- `ccache`
- `git`
- `python3`
- `uv`

The checked-in `pyproject.toml` / `uv.lock` define the Python development tools.
Run Markdown verification and the other helper scripts with `uv run ...`. Do not
use a globally installed `mdformat`.

## GCC image

Build the image:

```bash
podman build \
  -f docker/ubuntu25-toolchains/Containerfile \
  --build-arg TOOLCHAIN=gcc-15 \
  -t dingo-toolchains:ubuntu-26.04-gcc-15 \
  docker/ubuntu25-toolchains
```

Build and test Dingo in one command:

```bash
podman run --rm \
  -v "$PWD:/workspace:Z" \
  -w /workspace \
  dingo-toolchains:ubuntu-26.04-gcc-15 \
  bash -lc 'set -euxo pipefail; export NINJA_STATUS="[%f/%t %es] "; cmake -S . -B build-gcc15 -G Ninja \
    -DCMAKE_C_COMPILER=gcc-15 \
    -DCMAKE_CXX_COMPILER=g++-15 \
    -DDINGO_DEVELOPMENT_MODE=ON \
    -DDINGO_BENCHMARK_ENABLED=OFF && \
    cmake --build build-gcc15 --verbose -j"$(nproc)" && \
    ctest --test-dir build-gcc15 --output-on-failure'
```

For GCC 16, select the Ubuntu 26.10 base:

```bash
podman build \
  -f docker/ubuntu25-toolchains/Containerfile \
  --build-arg BASE_IMAGE=ubuntu:26.10 \
  --build-arg TOOLCHAIN=gcc-16 \
  -t dingo-toolchains:ubuntu-26.10-gcc-16 \
  docker/ubuntu25-toolchains
```

## Clang image

Build the image:

```bash
podman build \
  -f docker/ubuntu25-toolchains/Containerfile \
  --build-arg TOOLCHAIN=clang-20 \
  -t dingo-toolchains:ubuntu-26.04-clang-20 \
  docker/ubuntu25-toolchains
```

Build and test Dingo in one command:

```bash
podman run --rm \
  -v "$PWD:/workspace:Z" \
  -w /workspace \
  dingo-toolchains:ubuntu-26.04-clang-20 \
  bash -lc 'set -euxo pipefail; export NINJA_STATUS="[%f/%t %es] "; cmake -S . -B build-clang20 -G Ninja \
    -DCMAKE_C_COMPILER=clang-20 \
    -DCMAKE_CXX_COMPILER=clang++-20 \
    -DDINGO_DEVELOPMENT_MODE=ON \
    -DDINGO_BENCHMARK_ENABLED=OFF && \
    cmake --build build-clang20 --verbose -j"$(nproc)" && \
    ctest --test-dir build-clang20 --output-on-failure'
```

## MSVC via local msvc-wine image

The MSVC image is separate from the Linux images above. It uses
`TOOLCHAIN=msvc-wine`. The commands below build and run the local tag
`dingo-toolchains:ubuntu-26.04-msvc-wine`.

Build the image:

```bash
podman build \
  -f docker/ubuntu25-toolchains/Containerfile \
  --build-arg TOOLCHAIN=msvc-wine \
  -t dingo-toolchains:ubuntu-26.04-msvc-wine \
  docker/ubuntu25-toolchains
```

Build and test Dingo in one command:

```bash
podman run --rm \
  -v "$PWD:/workspace:Z" \
  -w /workspace \
  dingo-toolchains:ubuntu-26.04-msvc-wine \
  bash -lc 'set -euxo pipefail; export NINJA_STATUS="[%f/%t %es] "; CC=cl CXX=cl cmake -S . -B build-msvc -G Ninja \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_BUILD_TYPE=Release \
    -DDINGO_DEVELOPMENT_MODE=ON \
    -DDINGO_BENCHMARK_ENABLED=OFF && \
    cmake --build build-msvc --verbose -j"$(nproc)" && \
    ctest --test-dir build-msvc --output-on-failure'
```

## GitHub Actions example

This example requires the image to be available on the runner:

```yaml
jobs:
  linux:
    runs-on: ubuntu-24.04
    container:
      image: dingo-toolchains:ubuntu-26.04-gcc-15
    steps:
      - uses: actions/checkout@v7.0.0
      - run: cmake -S . -B build -G Ninja \
          -DCMAKE_C_COMPILER=gcc-15 \
          -DCMAKE_CXX_COMPILER=g++-15 \
          -DDINGO_DEVELOPMENT_MODE=ON \
          -DDINGO_BENCHMARK_ENABLED=OFF
      - run: cmake --build build -j4
      - run: ctest --test-dir build --output-on-failure
```
