# Ubuntu 25.04 toolchain images

Container images for local development and CI.

`Containerfile` is parameterized: each build installs exactly one selected
toolchain plus common development tools. That keeps each image smaller while
still using a single build recipe.

Supported `TOOLCHAIN` values:

- `gcc-11`
- `gcc-12`
- `gcc-13`
- `gcc-14`
- `gcc-15`
- `clang-14`
- `clang-15`
- `clang-17`
- `clang-18`
- `clang-19`
- `clang-20`
- `msvc-wine`

The built image tag is up to you. The examples below use:

- `dingo-toolchains:ubuntu-25.04-gcc-14`
- `dingo-toolchains:ubuntu-25.04-clang-19`
- `dingo-toolchains:ubuntu-25.04-msvc-wine`

Every image includes the shared build/runtime baseline:

- `cmake`
- `ninja`
- `ccache`
- `git`
- `python3`
- `mdformat`

## Build

```bash
podman build \
  -f docker/ubuntu25-toolchains/Containerfile \
  --build-arg TOOLCHAIN=gcc-14 \
  -t dingo-toolchains:ubuntu-25.04-gcc-14 \
  docker/ubuntu25-toolchains
```

Clang image:

```bash
podman build \
  -f docker/ubuntu25-toolchains/Containerfile \
  --build-arg TOOLCHAIN=clang-19 \
  -t dingo-toolchains:ubuntu-25.04-clang-19 \
  docker/ubuntu25-toolchains
```

MSVC image:

```bash
podman build \
  -f docker/ubuntu25-toolchains/Containerfile \
  --build-arg TOOLCHAIN=msvc-wine \
  -t dingo-toolchains:ubuntu-25.04-msvc-wine \
  docker/ubuntu25-toolchains
```

## Run

```bash
podman run --rm -it \
  -v "$PWD:/workspace:Z" \
  -w /workspace \
  dingo-toolchains:ubuntu-25.04-gcc-14
```

Clang image:

```bash
podman run --rm -it \
  -v "$PWD:/workspace:Z" \
  -w /workspace \
  dingo-toolchains:ubuntu-25.04-clang-19
```

MSVC image:

```bash
podman run --rm -it \
  -v "$PWD:/workspace:Z" \
  -w /workspace \
  dingo-toolchains:ubuntu-25.04-msvc-wine
```

## Inspect

```bash
podman run --rm dingo-toolchains:ubuntu-25.04-gcc-14 dingo-print-toolchains
```

Clang image:

```bash
podman run --rm dingo-toolchains:ubuntu-25.04-clang-19 dingo-print-toolchains
```

MSVC image:

```bash
podman run --rm dingo-toolchains:ubuntu-25.04-msvc-wine bash -lc 'cl /?'
```

## Build in the Linux image

```bash
cmake -S . -B build-gcc14 -G Ninja \
  -DCMAKE_C_COMPILER=gcc-14 \
  -DCMAKE_CXX_COMPILER=g++-14 \
  -DDINGO_DEVELOPMENT_MODE=ON \
  -DDINGO_BENCHMARK_ENABLED=OFF

cmake --build build-gcc14 -j"$(nproc)"
ctest --test-dir build-gcc14 --output-on-failure
```

Clang example:

```bash
cmake -S . -B build-clang19 -G Ninja \
  -DCMAKE_C_COMPILER=clang-19 \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DDINGO_DEVELOPMENT_MODE=ON \
  -DDINGO_BENCHMARK_ENABLED=OFF
```

One-shot form:

```bash
podman run --rm \
  -v "$PWD:/workspace:Z" \
  -w /workspace \
  dingo-toolchains:ubuntu-25.04-gcc-14 \
  bash -lc 'cmake -S . -B build -G Ninja \
    -DCMAKE_C_COMPILER=gcc-14 \
    -DCMAKE_CXX_COMPILER=g++-14 \
    -DDINGO_DEVELOPMENT_MODE=ON \
    -DDINGO_BENCHMARK_ENABLED=OFF && \
    cmake --build build -j"$(nproc)" && \
    ctest --test-dir build --output-on-failure'
```

## Build in the MSVC image

```bash
CC=cl CXX=cl cmake -S . -B build-msvc -G Ninja \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_BUILD_TYPE=Release \
  -DDINGO_DEVELOPMENT_MODE=ON \
  -DDINGO_BENCHMARK_ENABLED=OFF

cmake --build build-msvc -j"$(nproc)"
```

One-shot form:

```bash
podman run --rm \
  -v "$PWD:/workspace:Z" \
  -w /workspace \
  dingo-toolchains:ubuntu-25.04-msvc-wine \
  bash -lc 'CC=cl CXX=cl cmake -S . -B build -G Ninja \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_BUILD_TYPE=Release \
    -DDINGO_DEVELOPMENT_MODE=ON \
    -DDINGO_BENCHMARK_ENABLED=OFF && \
    cmake --build build -j"$(nproc)"'
```

## GitHub Actions

If the selected image is already available on the runner:

```yaml
jobs:
  linux:
    runs-on: ubuntu-24.04
    container:
      image: dingo-toolchains:ubuntu-25.04-gcc-14
    steps:
      - uses: actions/checkout@v6
      - run: cmake -S . -B build -G Ninja \
          -DCMAKE_C_COMPILER=gcc-14 \
          -DCMAKE_CXX_COMPILER=g++-14 \
          -DDINGO_DEVELOPMENT_MODE=ON \
          -DDINGO_BENCHMARK_ENABLED=OFF
      - run: cmake --build build -j4
      - run: ctest --test-dir build --output-on-failure
```
