# Ubuntu 25.04 toolchain images

Container images for local development and CI.

`Containerfile` is parameterized: each build installs exactly one selected
toolchain plus common development tools. The recipe accepts `gcc-*`, `clang-*`,
and `msvc` / `msvc-wine` values.

Linux image-backed CI currently builds and exercises:

- `dingo-toolchains:ubuntu-25.04-gcc-15`
- `dingo-toolchains:ubuntu-25.04-clang-20`

For MSVC, the commands below build and run a local example tag:

- `dingo-toolchains:ubuntu-25.04-msvc-wine`

Every image includes the shared build/runtime baseline:

- `cmake`
- `ninja`
- `ccache`
- `git`
- `python3`
- `mdformat`

## GCC image

Build the image:

```bash
podman build \
  -f docker/ubuntu25-toolchains/Containerfile \
  --build-arg TOOLCHAIN=gcc-15 \
  -t dingo-toolchains:ubuntu-25.04-gcc-15 \
  docker/ubuntu25-toolchains
```

Build and test Dingo in one shot:

```bash
podman run --rm \
  -v "$PWD:/workspace:Z" \
  -w /workspace \
  dingo-toolchains:ubuntu-25.04-gcc-15 \
  bash -lc 'set -euxo pipefail; export NINJA_STATUS="[%f/%t %es] "; cmake -S . -B build-gcc15 -G Ninja \
    -DCMAKE_C_COMPILER=gcc-15 \
    -DCMAKE_CXX_COMPILER=g++-15 \
    -DDINGO_DEVELOPMENT_MODE=ON \
    -DDINGO_BENCHMARK_ENABLED=OFF && \
    cmake --build build-gcc15 --verbose -j"$(nproc)" && \
    ctest --test-dir build-gcc15 --output-on-failure'
```

## Clang image

Build the image:

```bash
podman build \
  -f docker/ubuntu25-toolchains/Containerfile \
  --build-arg TOOLCHAIN=clang-20 \
  -t dingo-toolchains:ubuntu-25.04-clang-20 \
  docker/ubuntu25-toolchains
```

Build and test Dingo in one shot:

```bash
podman run --rm \
  -v "$PWD:/workspace:Z" \
  -w /workspace \
  dingo-toolchains:ubuntu-25.04-clang-20 \
  bash -lc 'set -euxo pipefail; export NINJA_STATUS="[%f/%t %es] "; cmake -S . -B build-clang20 -G Ninja \
    -DCMAKE_C_COMPILER=clang-20 \
    -DCMAKE_CXX_COMPILER=clang++-20 \
    -DDINGO_DEVELOPMENT_MODE=ON \
    -DDINGO_BENCHMARK_ENABLED=OFF && \
    cmake --build build-clang20 --verbose -j"$(nproc)" && \
    ctest --test-dir build-clang20 --output-on-failure'
```

## MSVC via local msvc-wine image

The MSVC path stays separate from the Linux images above. It uses the
`msvc-wine` Containerfile mode, and the commands below build and run the local
example tag `dingo-toolchains:ubuntu-25.04-msvc-wine`.

Build the image:

```bash
podman build \
  -f docker/ubuntu25-toolchains/Containerfile \
  --build-arg TOOLCHAIN=msvc-wine \
  -t dingo-toolchains:ubuntu-25.04-msvc-wine \
  docker/ubuntu25-toolchains
```

Build and test Dingo in one shot:

```bash
podman run --rm \
  -v "$PWD:/workspace:Z" \
  -w /workspace \
  dingo-toolchains:ubuntu-25.04-msvc-wine \
  bash -lc 'set -euxo pipefail; export NINJA_STATUS="[%f/%t %es] "; CC=cl CXX=cl cmake -S . -B build-msvc -G Ninja \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_BUILD_TYPE=Release \
    -DDINGO_DEVELOPMENT_MODE=ON \
    -DDINGO_BENCHMARK_ENABLED=OFF && \
    cmake --build build-msvc --verbose -j"$(nproc)" && \
    ctest --test-dir build-msvc --output-on-failure'
```

## GitHub Actions example

If the selected image is already available on the runner:

```yaml
jobs:
  linux:
    runs-on: ubuntu-24.04
    container:
      image: dingo-toolchains:ubuntu-25.04-gcc-15
    steps:
      - uses: actions/checkout@v6
      - run: cmake -S . -B build -G Ninja \
          -DCMAKE_C_COMPILER=gcc-15 \
          -DCMAKE_CXX_COMPILER=g++-15 \
          -DDINGO_DEVELOPMENT_MODE=ON \
          -DDINGO_BENCHMARK_ENABLED=OFF
      - run: cmake --build build -j4
      - run: ctest --test-dir build --output-on-failure
```
