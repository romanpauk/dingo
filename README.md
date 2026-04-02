# Dingo

Header-only dependency injection container for C++.

Dingo builds object graphs from ordinary C++ types. Scopes and storage policies
control ownership explicitly, and user types stay free of framework macros or
base classes.

Tested with:

- C++17, C++20, C++23, C++26
- GCC 12-14
- Clang 17-19
- Visual Studio 2022, 2026

[![Build](https://github.com/romanpauk/dingo/actions/workflows/build.yaml/badge.svg?branch=master)](https://github.com/romanpauk/dingo/actions?query=branch%3Amaster++)

## Quick Start

<!-- { include("examples/container/quick.cpp", scope="////") -->

Example code included from
[examples/container/quick.cpp](examples/container/quick.cpp):

```c++
// Class types to be managed by the container. Note that there is no special
// code required for a type to be used by the container and that conversions
// are applied automatically based on an registered type scope.
struct A {
    A() {}
};
struct B {
    B(A&, std::shared_ptr<A>) {}
};
struct C {
    C(B*, std::unique_ptr<B>&, A&) {}
};

container<> container;
// Register struct A with a shared scope, stored as std::shared_ptr<A>
container.register_type<scope<shared>, storage<std::shared_ptr<A>>>();

// Register struct B with a shared scope, stored as std::unique_ptr<B>
container.register_type<scope<shared>, storage<std::unique_ptr<B>>>();

// Register struct C with an unique scope, stored as plain C
container.register_type<scope<unique>, storage<C>>();

// Resolving the struct C will recursively instantiate required dependencies
// (structs A and B) and inject the instances based on their scopes into C.
// As C is in unique scope, each resolve<C> will return new C instance.
// As A and B are in shared scope, each C will get the same instances
// injected.
/*C c =*/container.resolve<C>();

struct D {
    A& a;
    B* b;
};

// Construct an un-managed struct using dependencies from the container
/*D d =*/container.construct<D>();

// Invoke callable
/*D e =*/container.invoke([&](A& a, B* b) { return D{a, b}; });
```

<!-- } -->

## Installation

For CMake-based projects, `FetchContent` is the documented integration path. The
project exports the `dingo::dingo` interface target and has no runtime
dependencies.

<!-- { include("test/fetchcontent/CMakeLists.txt", scope="####") -->

Example code included from
[test/fetchcontent/CMakeLists.txt](test/fetchcontent/CMakeLists.txt):

```CMake
include(FetchContent)
FetchContent_Declare(dingo
                     GIT_REPOSITORY https://github.com/romanpauk/dingo.git
                     GIT_TAG        master)
FetchContent_MakeAvailable(dingo)
target_link_libraries(dingo_fetchcontent_test PRIVATE dingo::dingo)
```

<!-- } -->

If you are working from a checkout, the top-level CMake project also supports
`add_subdirectory(...)`.

## Documentation

- [Getting Started](docs/getting-started.md): installation, first registrations,
  `resolve()`, `construct()`, and `invoke()`.
- [Core Concepts](docs/core-concepts.md): scopes, storage, arrays, variants,
  factories, interfaces, and multibindings.
- [Advanced Topics](docs/advanced-topics.md): indexed resolution, annotations,
  static vs dynamic containers, nesting, RTTI, allocation, and runtime notes.
- [Architecture](docs/architecture/README.md): registration flow, lookup shape,
  extension traits, and conversion rules.
- [Examples](docs/examples.md): a guided index of runnable examples in
  [`examples/`](examples).
- [Motivation and History](docs/history.md): background on how the library
  evolved.

## Feature Summary

- Non-intrusive registration of ordinary C++ types
- Explicit control over lifetime and stored representation
- Array support for raw arrays and smart-pointer-backed arrays
- Explicit variant construction plus whole-variant or held-alternative
  resolution
- Constructor deduction with explicit factory overrides when needed
- Interface bindings and multibindings
- Indexed and annotated resolution
- Static and dynamic container configurations
- Parent-child container nesting
- Custom RTTI and allocator hooks

## Project Notes

Development builds can enable tests, benchmarks, and runnable examples through
`DINGO_DEVELOPMENT_MODE=ON`. The library itself remains header-only.

If you are comparing DI libraries, also take a look at:

- [qlibs/di](https://github.com/qlibs/di)
- [boost-ex/di](https://github.com/boost-ext/di)
- [google/fruit](https://github.com/google/fruit)
- [gracicot/kangaru](https://github.com/gracicot/kangaru)
- [ybainier/Hypodermic](https://github.com/ybainier/Hypodermic)
