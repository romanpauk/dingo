# Ubuntu 25.04 toolchains images

Container images for local development and CI.

## Images

The Linux image built from `Containerfile` is:

- `dingo-toolchains:ubuntu-25.04`

The MSVC image built from `Containerfile.msvc-wine` is:

- `dingo-toolchains:ubuntu-25.04-msvc-wine`

For now, both images are for local or private CI use.

## Linux image

## Included compilers

The main image installs the GCC versions Canonical documents for Ubuntu 25.04:

- `g++-11`
- `g++-12`
- `g++-13`
- `g++-14`
- `g++-15`

It also installs the LLVM/Clang versions Canonical documents for Ubuntu 25.04:

- `clang-14`
- `clang-15`
- `clang-17`
- `clang-18`
- `clang-19`
- `clang-20`

Plus common development tools:

- `cmake`
- `ninja`
- `ccache`
- `gdb`
- `lldb`
- `clang-format`
- `clang-tidy`
- `clangd`
- `python3`
- `mdformat`

## Build

```bash
podman build \
  -f docker/ubuntu25-toolchains/Containerfile \
  -t dingo-toolchains:ubuntu-25.04 \
  docker/ubuntu25-toolchains
```

MSVC image:

```bash
podman build \
  -f docker/ubuntu25-toolchains/Containerfile.msvc-wine \
  --build-arg ENABLE_MSVC_WINE=1 \
  -t dingo-toolchains:ubuntu-25.04-msvc-wine \
  docker/ubuntu25-toolchains
```

## Run

```bash
podman run --rm -it \
  -v "$PWD:/workspace:Z" \
  -w /workspace \
  dingo-toolchains:ubuntu-25.04
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
podman run --rm dingo-toolchains:ubuntu-25.04 dingo-print-toolchains
```

MSVC image:

```bash
podman run --rm dingo-toolchains:ubuntu-25.04-msvc-wine bash -lc 'cl /?'
```

## Build in the Linux image

```bash
cmake -S . -B build-gcc12 -G Ninja \
  -DCMAKE_C_COMPILER=gcc-12 \
  -DCMAKE_CXX_COMPILER=g++-12 \
  -DDINGO_DEVELOPMENT_MODE=ON \
  -DDINGO_BENCHMARK_ENABLED=OFF

cmake --build build-gcc12 -j"$(nproc)"
ctest --test-dir build-gcc12 --output-on-failure
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
  dingo-toolchains:ubuntu-25.04 \
  bash -lc 'cmake -S . -B build -G Ninja \
    -DCMAKE_C_COMPILER=gcc-12 \
    -DCMAKE_CXX_COMPILER=g++-12 \
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

If the Linux image is already available on the runner:

```yaml
jobs:
  linux:
    runs-on: ubuntu-24.04
    container:
      image: dingo-toolchains:ubuntu-25.04
    steps:
      - uses: actions/checkout@v6
      - run: cmake -S . -B build -G Ninja \
          -DCMAKE_C_COMPILER=gcc-12 \
          -DCMAKE_CXX_COMPILER=g++-12 \
          -DDINGO_DEVELOPMENT_MODE=ON \
          -DDINGO_BENCHMARK_ENABLED=OFF
      - run: cmake --build build -j4
      - run: ctest --test-dir build --output-on-failure
```
