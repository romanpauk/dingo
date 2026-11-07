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
- `model-check-small`, `model-check-medium`, `model-check-large`: run both model
  probes, the [Model Probe](#model-probe) and the
  [Constructor Detection Probe](#constructor-detection-probe), on a cover of
  their dimensions: small every pair of their values, in about three minutes;
  medium every three, in about three times that; large every combination, in
  about half an hour. They fail on a defect and read the diagnostics of GCC and
  clang; `model-check-small` is part of `check` with them.

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

## Specification

[`spec/`](../spec) says what the library should do, without asking it, and holds
the probes that compare the library with it:
[`vocabulary.py`](../spec/vocabulary.py) names the concepts of the library, an
enum each, and [`semantics.py`](../spec/semantics.py) says, for a registration
and a request of it, whether the library accepts the registration, which
interfaces it binds, whether the request resolves, and how it fails when it does
not. The C++ headers the probes include are in [`test/model/`](../test/model).

A type is a stack of layers around an element, `std::shared_ptr<L>` the layer
`shared_ptr` around `L`, and each layer, leaf and scope is declared by its
properties: what it owns, whether a copy of it shares what it holds, whether it
can be copied, moved or made of a value. A request resolves when what it asks
for can be derived from what the scope stores, by steps such as taking the
element of a handle, copying, converting a handle, or moving a value into a new
layer, and when what it refers to lives long enough: what is returned to the
caller only refers to what the container or the caller keeps. A request that
does not resolve the static container rejects as it compiles, except a request
for the alternative an alternative type does not hold; the runtime container
throws `type_not_convertible`, except for what the types of a request rule out.

What the library does not do yet, although the semantics allow it, is a named
limitation: a filter over the steps and references of the semantics, kept apart
from them. The [Model Probe](#model-probe) compares the library with the
specification.

## Model Probe

[`spec/probe.py`](../spec/probe.py) asks the library what every combination of
container, scope, stored form, leaf, interfaces declared, selection, operation
and request does, and compares it with what the [Specification](#specification)
expects.

A registration declares no interfaces, so that the library binds those it
derives from the stored type; its abstract interface `leaf_interface`; or both
its leaf and that interface. Its requests select it plainly, by an annotation,
or by a key.

The specification says which interfaces each registration binds, and a survey
program first checks that the library binds exactly those types. The requests
are every shape of every bound interface `I`: `I`, `std::shared_ptr<I>`,
`std::unique_ptr<I>`, `std::optional<I>`, `std::shared_ptr<I[]>` and
`std::unique_ptr<I[]>`, each by value, `const`, as `&`, `const &`, `&&`, `*` and
`const *`. A request that is not valid C++, such as an abstract type held by
value, is recorded as invalid with the reason.

Each request is made through every entry point, `resolve`, `invoke` and
`construct`, in four positions: itself; as the dependency of the constructor of
an unregistered class, which the container constructs; and as the dependency of
a class registered in the `shared` or `shared_cyclical` scope, which keeps it
and what it receives.

Each registration is compiled into a program per position with
[`test/model/probe.h`](../test/model/probe.h), which makes each request,
resolving it twice, and records the outcome. A request the library rejects at
compile time is recorded with the message of its `static_assert`: the compiler
names the requests that fail by their lines in the program, and the others are
compiled again without them. Probes share instantiations, which the compiler
reports once, so the operations of a failed request are then compiled in a
program each. The header is precompiled once, and clang's error limit is lifted.

A resolved request is also checked: its result has to reach the registered leaf,
a scope that keeps its instance has to return the same one by reference, a
unique scope a new one, a kept class may not refer to a leaf that is gone, and
nothing may leak. Each program also checks every request its registration
publishes: one that selects another binding is unreachable, and one that no
probe makes is unprobed.

A cell a limitation keeps from resolving is recorded as limited, with the
limitations. The probe fails when a request does not compile without a
`static_assert`, crashes or fails these checks, when a published request is
unreachable or unprobed, when a cell is not what the specification expects
(unexpected), when a limited cell resolves (a stale limitation), or when a
registration binds other interfaces than the specification says.

```bash
cmake --build build -t model-check-small
```

writes the outcome of every cell to `build/model-small/probe.json`, and a
summary with a table per container, leaf, interfaces, selection and operation
next to it, as `probe.md`; `model-check-medium` and `model-check-large` write
theirs to `build/model-medium/` and `build/model-large/`. A size probes a
covering array of the registrations the specification says the library accepts:
every combination of values of two (small) or three (medium) of scope, stored
form, leaf, interfaces declared and selection that such a registration holds is
probed in both containers, with 46 or 151 of the 1584 registrations (large). A
registration the library does not accept resolves nothing, so it covers nothing.
Run directly, `probe.py --cover pairs|triples|all` chooses it, and
`--only <regex>` probes the registrations whose name matches the expression.

### Generator

The probe and the detection probe below share
[`spec/generator/`](../spec/generator):

- `dimensions.py`: an enum per dimension only the probes have, beside the
  vocabulary of the specification. The value of a member is its name in a cell.
- `model.py`: an interface the library names, a request of the grammar as a
  shape and a category of an interface, with the rules for requests that are not
  valid C++, and a probe of a request.
- `registrations.py` and `detection.py`: the units of each family and their
  cells, without any C++.
- `templates/`: the Jinja templates that spell the units in C++. `macros.j2`
  holds the spellings of the dimensions, `registration.j2` those of a
  registration of the probe.
- `driver.py`: compiles and runs the programs, and tells compile errors apart by
  probe. A template marks the lines of each probe with `// probe N`.
- `survey.py` and `report.py`: the survey that checks the interfaces a
  registration binds, and the baseline, its defects and its disagreements.

## Constructor Detection Probe

[`spec/detection.py`](../spec/detection.py) probes constructor detection. A
parameter is every shape of every supported leaf (copyable, move-only, and an
abstract interface), plain, annotated and keyed, in every category. Each is the
parameter of a class of each kind: a constructor, an explicit constructor, an
aggregate member, and the two parameters of a constructor
([`test/model/detection.h`](../test/model/detection.h)).

For each class it records what shape and signature detection say, against the
constructor the generator wrote, and makes the class through `resolve`,
`construct` and `invoke` of an unregistered class and through a `shared`
registration of it, in both containers over a few registrations of the leaf. It
makes the class with the constructor an explicit factory names, the one shape
detection finds and the one signature detection finds. A detected constructor
has to make what the explicit one makes: through a registration as a
registration does, and through `resolve` or `construct` as `construct` does. A
class `invoke` makes has no explicit counterpart, as it may refer to what the
request makes, which a class returned to the caller may not; it is checked only
for defects.

The known limitations of detection are listed, with their reasons, in
`LIMITATIONS` of
[`spec/generator/detection.py`](../spec/generator/detection.py): each says which
detections report a class wrongly and which detected constructions make it
differently from its explicit constructor. A misdetection one of them describes
is recorded as `limited`, and a construction one of them describes is not
compared with the explicit constructor; a limitation whose misdetection no
longer occurs is a defect, `stale_limitation`.

The model check targets run it on the same cover sizes, over leaf, selection,
kind and provider, and write `detection.json` and `detection.md` next to the
probe's.

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
