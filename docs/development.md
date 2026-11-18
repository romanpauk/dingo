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

- `markdown-format`: formats [`README.md`](../README.md) and the Markdown files
  under `docs` and `docker`, and makes each file a code span names a link to it,
  where it names one file of the repository.

- `markdown-format-check`: verifies the same Markdown files: their format, the
  targets of their links, and that each file a code span names, by a suffix of a
  file, is one file of the repository and a link to it. The files the build
  makes are named, not linked, and are kept in `GENERATED` of
  [`tools/md.py`](../tools/md.py).

- `format-check`: runs `sources-format-check` and `markdown-format-check`.

- `layers-check`: verifies that a header includes only headers of lower layers
  or of its own directory. See
  [Header Layers](architecture/overview.md#header-layers).

- `names-check`: verifies that the names of the headers and of the specification
  have no connecting words. See [Naming](#naming).

- `includes-compile`: compiles one generated translation unit for each public
  header.

- `includes-tidy`: runs clang-tidy on those translation units.

- `includes-check`: checks the includes of those translation units.

- `check`: runs all verification targets that are available on the platform.

- `complexity-report`: prints where the complexity of the headers is. It is a
  report, not part of `check`. See [Complexity Report](#complexity-report).

- `semantics-check`: compares the semantics of the library, of no C++ type, with
  the specification, for every stored object and every request. See
  [Layered Semantics](architecture/layered-semantics.md). It compares each
  domain in parts, in processes at once. It keeps the answers of the
  specification in `semantics/answers` of the build directory and makes them
  again when a source of `spec/` changes. A change of the library alone only
  compiles and runs the library's side, in seconds.

- `semantics-check-pairs`: the same, and for every pair of layers a type could
  define: 1,669,960 pairs. It is not part of `check`.

- `conversion-check`: checks what the binding of each registration derives for
  every request against the [Specification](specification.md#conversion-check).

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

- `footprint-check`: measures what the library costs the compiler and the code
  it emits, and reports what changed since the run before. See
  [Footprint](#footprint).

With GCC and clang, `check` includes `semantics-check`, `conversion-check`,
`emptiness-check`, `storage-check`, `resolution-check`, `model-check-small` and
`footprint-check`. `includes-tidy` and `includes-check` are not available on
Windows. The `sources-format*`, `includes-tidy` and `includes-check` targets
need `DINGO_CLANG_TOOLS_ENABLED=ON` (the default).

### Kept Results

The checks that compile programs (`conversion-check`, `emptiness-check`,
`storage-check`, `resolution-check`, the model checks, `footprint-check` and
[`spec/execution_check.py`](../spec/execution_check.py)) keep the result of each
program in `programs/` of their work directory, compressed with zstd. As ccache
does, a result is kept under a key of the compiler, the flags, the name of the
program and the source the preprocessor makes of it. A program is compiled again
only when one of them changes: a header changed, added or removed changes the
preprocessed source of the programs that read it, and a change of comments only
changes none. A result is not kept when a header the precompiled header read
changed after it was compiled, as the compiler took that header as it was. The
key holds no path of the checkout, so a cache made in one checkout is valid in
another. A run of every program of a check removes the results it did not use,
and each run prints how many programs it compiled and how many results it used.

A program includes only the container and the scopes of its cells, and holds
cells of one container and one scope, so a change of one scope or one container
compiles only the programs of it again. The headers of the probes are split the
same way: [`test/model/leaves.h`](../test/model/leaves.h) has the leaf types and
no container, [`test/model/probe.h`](../test/model/probe.h) no container and no
scope, and [`test/model/static.h`](../test/model/static.h) what a static
container adds.

## Naming

A name describes what a thing is or does, in the terms the library and the
specification already use. It has no connecting word: of, with, or, and, at, as,
by, to, from, in, into, for, on, if, per, via, then. `names-check` fails on one,
and on a parameter, member or local named by one alone.

| A name of                          | Is                                    | As                                     |
| ---------------------------------- | ------------------------------------- | -------------------------------------- |
| what a type gives of another type  | the noun, with `_t` or `_v`           | `held_element_t`, `leaf_traits_v`      |
| an encoded value and its decoding  | an `encode_` and a `decode_` function | `encode_kind`, `decode_kind`           |
| a function that builds a table     | `make_` and the table                 | `make_finishes`, `make_tables`         |
| an element of a list               | the element                           | `plan_step`, `search_step`             |
| a function that fills a table      | `add_` and what it adds               | `add_runs`, `add_qualified`            |
| a function that returns an address | an `_address` name                    | `pointee_address`, `cell_address`      |
| a predicate                        | the verb and its object               | `borrows`, `converts_unknown_layer`    |
| a parameter, member or local       | what it holds                         | `object`, `context`, `source`, `index` |

A parameter that takes the name of its type, such as `const object &object`,
names the type in its scope as `semantics::object`. A typedef or a member type
cannot be the name of a variable, as `-Wshadow` warns of it.

A name the standard library or the public interface spells with a connecting
word is kept, with why, in [`tools/names.py`](../tools/names.py): `for_each`,
the names of `std::in_place`, and `lookup_key_out_of_range_exception`.

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

When you run [`spec/probe.py`](../spec/probe.py) directly,
`--cover pairs|triples|all` selects the [cover](specification.md#model-probe).
`--only <regex>` probes only the registrations with a name that matches the
expression.

The probes limit the memory of the compilers:

- The probes run one compiler for each processor, at most as many as half of the
  available memory holds. `--jobs <n>` sets a different number.
- Each compiler runs in an address space of 1 GiB (`MEMORY_LIMIT` in
  [`spec/generator/driver.py`](../spec/generator/driver.py)).
- A program holds 64 probes (`GROUP_PROBES`).
- The limit is a gate: the probes do not split a program the compiler runs out
  of memory on. Its probes fail, which the check reports, and the size of the
  programs or the library is then to be changed.

## Unit Tests

`dingo_unit_test` tests what the [Specification](specification.md) does not
cover, one test for each behavior. A test is in the directory of what it tests:

- `test/lookup`: collections and lookups.
- `test/container/parent`: parent containers.
- `test/factory`: the kinds of factory and constructor detection.
- `test/resolution`: wrappers, callables and identity across requests.
- `test/runtime`: regressions of the runtime container.

## Footprint

`footprint-check` ([`spec/footprint.py`](../spec/footprint.py)) compiles
scenario programs of the library with the compiler CMake is configured with, and
measures each:

- the code, the constants, the data and the unwind tables of its object, read
  from its ELF sections, its functions, and the size of each function of no
  mangled name, such as the probes of
  [`test/lit/codegen-static-probes.cpp`](../test/lit/codegen-static-probes.cpp)
  at `-O2`
- the peak memory and the processor time of the compiler, and the instructions
  it ran where `perf` counts them
- where the compiler spent its work, as it reports it in a compile of its own:
  GCC the time and the collected memory of each phase of `-ftime-report`, clang
  the events of `-ftime-trace` by kind and the templates it instantiated by the
  area of the library they are of

The scenarios are the same programs for each container: the runtime container,
the runtime container of an arena's memory, and the static container. They are
the headers of each container alone, an empty container, and one or five
registrations of each shape, resolved by every request of the shape or by none.
The shapes are:

- `shared`: a shared handle, requested by reference, as a handle and as a
  reference to the handle
- `unique`: a unique value, requested by value and as a `std::unique_ptr`
- `interface`: a shared handle of an implementation, requested as its interface
- `string`: a unique value that owns memory
- `external`: an object the program owns; the static container holds none
- `dependency`: a shared object constructed of another
- `indexed`: implementations of one interface under keys a request selects

Their differences give the costs it prints: of each container, and of a first
registration, a further registration and a request type of each shape in each
container.

The measurements are kept results, in `programs/` of its work directory, and the
last of each scenario for each compiler beside them, in `programs/latest/`. A
run reports which measurements changed since the one before, past what changes
between runs of the same program: nothing of a size or a count, a little of
instructions and memory, much of a time. A cache of the results made by one run,
as of the main branch, is the baseline of the next. It fails only when a
scenario does not compile.

To compare with another checkout, such as the main branch, and with each
compiler, run the same measurements under
[nbench](https://github.com/romanpauk/nbench) with
[`benchmark/nbench_conf.py`](../benchmark/nbench_conf.py): each checkout and
compiler is a variant, compared with this checkout under the same compiler.
Every suite measures the same scenarios:

- `footprint` and `release`: the scenarios at `-O0` and at `-O2`; `costs` and
  `release-costs` are their differences
- `callers`: the code another place that resolves adds
- `headers`: the code by the header it is defined in
- `compiler`: where the compiler spent its work, a phase, an event or an area of
  the library per case
- `probes`: the code-generation probes
- `run`: what the containers run, measured by the harness of nbench: the time,
  the instructions, the cycles and the cache misses of constructing an empty
  container, of registering into a new one, and of the first resolution of each
  request type, in a new container, and of those after it

The `run` suite compiles the cases of every scenario
([`footprint.run_source`](../spec/footprint.py)) with the variant's headers,
optimized as the code-generation probes are, into one program per variant. With
them are the transactions of the runtime container,
[`benchmark/transaction.cpp`](../benchmark/transaction.cpp), and resolvers
written by hand of no container, the least a resolution costs,
[`benchmark/reference.cpp`](../benchmark/reference.cpp). A source a variant does
not compile has no case in its program; its error is in `compile.log` of the
variant's work directory.

nbench is the submodule [`tools/nbench`](../tools/nbench), which `uv sync`
installs. A worktree of the main branch in the build directory is a tree of its
own:

```bash
git submodule update --init tools/nbench
uv sync
git worktree add --detach build/master master
uv run nbench run -C benchmark/nbench_conf.py -v this=. -v master=build/master
```

A tree that names the runtime container traits `dynamic_container_traits`, as
the main branch does, compiles the scenarios with that name.

A case of the `run` suite runs until its time per iteration is steady within 2%,
50 ms at most, or a fixed number of batches of new containers. The command
line's limits replace the suite's: `--budget 10s` divides ten seconds among the
runs, `--min-iterations 64` is enough for the instructions, and
`--in-process -r 3` runs three repetitions of a case in one process. The compile
suites keep their results by what each program is, so a run compiles only the
scenarios whose source or headers changed.

The report is in `build/nbench/reports/`, with `explorer.html` of every run.

## Code Size Tests

The lit suite compiles the code-generation probes of
[`test/lit/codegen-static-probes.cpp`](../test/lit/codegen-static-probes.cpp) at
`-O2`. They check that a resolution from the static container is no larger than
the same resolution from the runtime container, by the symbol sizes `nm` gives,
and that the small static probes compile to one instruction and a return, by
`objdump`, with compilers that reliably compile them so. The size of each probe
is tracked by `footprint-check`.

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
