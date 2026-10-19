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
- `generate`: regenerates the generated files from the spec, see
  [Generated Code](#generated-code).
- `generate-check`: verifies that the generated files match the spec.
- `check`: runs all verification targets available on the platform.

The development tools are provided by the locked `uv` environment.

## Spec Package

[`spec/`](../spec) is the language-agnostic specification described in
[Spec-Driven Design](architecture/spec-driven-design.md#specification). It is a
typed Python package: `spec/schema.py` defines the sections, `spec/content/`
holds one module of data per section, and `spec.load_spec()` returns the
validated spec. `spec/validate.py` checks identifiers and cross-references
between sections. Export the spec as canonical JSON from the repository root:

```bash
python3 -m spec.export --output build/spec.json
```

The spec tests run as `dingo_spec_test` in ctest.

### Public API Check

[`test/spec/public_api_check.py`](../test/spec/public_api_check.py) compiles
every public include path of the spec alone, and one translation unit that uses
every public name and extension point and checks every configuration macro. A
failure names the spec entry. The lit test `public-api.t` runs it.

### Contract Evaluation

[`test/contracts/evaluate.py`](../test/contracts/evaluate.py) reports, per
toolchain of the contract baseline, which contracts of the spec the current
implementation satisfies, which it violates, and which no current probe
observes. It is a report and does not fail on a violation.

```bash
python3 test/contracts/evaluate.py
```

### Route Conformance

The route tools compare what the library does with the route rules of the spec,
`spec/content/routes.py`, as interpreted by the reference implementation in
`spec/reference/routes.py`.

- [`test/routes/extract.py`](../test/routes/extract.py) compiles and runs every
  combination of container, leaf kind, declared interfaces, scope, stored shape,
  and request against the library and records the outcome, the constructions,
  copies, and moves, and the identity of the result. Stored shapes use the
  standard wrappers and user wrappers defined through the wrapper-traits
  extension point. The table is written to `build/routes/observed/` and not
  committed; extracting it takes about eight minutes with 32 jobs.
- [`test/routes/conform.py`](../test/routes/conform.py) compares every case with
  the reference implementation. Every difference must be declared by a behavior
  change of the spec, `spec/content/behavior_changes.py`, and every declared
  change must match a difference. The ctest entry `dingo_routes_conform_test`
  runs it when a table has been extracted.
- [`test/routes/summarize.py`](../test/routes/summarize.py) lists where the
  runtime, static, and mixed containers differ from each other.

Extract the table of the pinned baseline and check it:

```bash
mkdir -p build/routes/baseline
git archive "$(cat test/differential/baseline-commit)" include \
    | tar -x -C build/routes/baseline
CXX=g++ python3 test/routes/extract.py --work-dir build/routes \
    --include-dir build/routes/baseline/include
python3 test/routes/conform.py --summary
```

### Scope Check

[`test/spec/scopes_check.py`](../test/spec/scopes_check.py) generates a program
that prints the storage scope policies of the current implementation and
compares them with the scope descriptors of the spec. The lit test
`spec-scopes.t` runs it.

## Generated Code

[`tools/generator/generate.py`](../tools/generator/generate.py) generates files
from the spec, as described in
[Generated Code Lifecycle](architecture/spec-driven-design.md#generated-code-lifecycle).
Generated files are committed. Each starts with a marker line naming the spec
entries it comes from, and
[`spec/generated.manifest`](../spec/generated.manifest) lists every generated
file with its content hash.

- `generate` writes the files whose content changed and deletes files that are
  no longer generated. It refuses to overwrite or delete a file that is not in
  the manifest or does not carry the marker.
- `generate-check` fails on changed, missing, stale, or hand-edited generated
  files.

Development builds regenerate before compiling anything that uses the library.
Configure with `-DDINGO_GENERATE_ON_BUILD=OFF` to keep the tree as it is; CI
does so, runs `check`, then runs `generate` and fails when the tree is not
clean. Emitters live in `tools/generator/emitters.py` and compute the data,
while the Jinja templates in `tools/generator/templates/` only lay it out. The
generator runs through `uv`, which provides the pinned clang-format that formats
the output.

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

## Contract Baseline

[`test/contracts/probe.py`](../test/contracts/probe.py) measures the contract
dimensions described in
[Spec-Driven Design](architecture/spec-driven-design.md#contracts) on the
current implementation:

- `instantiations`: Dingo class and function template instantiations of
  generated scaling programs with 1, 8, and 32 bindings in static, mixed, and
  runtime containers, counted from Clang's `-ftime-trace` output; Clang only
- `object_size`: `sizeof` of containers holding growing numbers of bindings
- `codegen`: calls, indirect calls, and calls to Dingo functions made by each
  probe function of
  [`test/lit/codegen-static-probes.cpp`](../test/lit/codegen-static-probes.cpp),
  and the number of out-of-line Dingo functions in its object file
- `headers`: preprocessed lines that come from Dingo headers, per public header

The values are deterministic for a toolchain. They are recorded in
[`test/contracts/baseline.json`](../test/contracts/baseline.json), keyed by
compiler family, major version, architecture, and C++ standard. The lit test
`contract-baseline.t` runs `probe.py check`, which fails when a value exceeds
its baseline and reports values that improved. A toolchain without a baseline
prints its measured values and passes.

Record the baseline for a toolchain after an intended change:

```bash
CXX=clang++ python3 test/contracts/probe.py update --work-dir build/contracts
```

List the most instantiated Dingo templates of one scaling program:

```bash
CXX=clang++ python3 test/contracts/probe.py report \
    --work-dir build/contracts --scenario chain.static.32
```

## Differential Tests

The differential tests compare the behavior of the working tree with a pinned
baseline of the library, as described in
[Differential Testing](architecture/spec-driven-design.md#differential-testing).

[`test/differential/run.py`](../test/differential/run.py) extracts the
`include/` directory of the commit in
[`test/differential/baseline-commit`](../test/differential/baseline-commit) and
builds the generated matrix twice from the working-tree test sources: once
against `include/`, and once against the extracted baseline through the
development-only `DINGO_DIFFERENTIAL_INCLUDE_DIR` cache variable. Both builds
enable `DINGO_MATRIX_RECORD`, which makes every matrix assertion record what it
observed: the expression, its result, and printable operand values such as
counts and messages. Each executable writes its records to the file named by
`DINGO_MATRIX_RECORD_FILE`.

[`test/differential/compare.py`](../test/differential/compare.py) compares the
records. Every difference must be declared by a behavior change of the spec,
[`spec/content/behavior_changes.py`](../spec/content/behavior_changes.py), with
a `matrix:<executable>/<Suite>.<Name>` case pattern, and every such change must
match a difference. The constructor-detection matrix family tests internal names
and does not take part.

```bash
python3 test/differential/run.py --build-root build/differential --profile portable
```

The `differential` workflow runs the same command with GCC 14.

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
