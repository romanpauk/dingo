# Dingo

Dependency Injection Container for C++, with DI, next-gen and an 'o' in a name.

Dingo builds object graphs from ordinary C++ types with runtime or compile-time
registration. Scopes and storage policies control ownership explicitly, and user
types stay free of framework macros or base classes.

Resolution applies supported ownership and interface conversions automatically,
so one registration can satisfy references, pointers, smart pointers, and
interface views when the selected storage and scope allow it.

Tested with:

- C++17, C++20, C++23, C++26
- GCC 13-15
- Clang 18-20
- Visual Studio 2022, 2026

[![Build](https://github.com/romanpauk/dingo/actions/workflows/build.yaml/badge.svg?branch=master)](https://github.com/romanpauk/dingo/actions?query=branch%3Amaster++)

## Runtime Registration

Runtime registration uses `container<>` with registrations added through
`register_type<...>()`, which fits graphs assembled by ordinary control flow.

<!-- { include("examples/container/quick_runtime.cpp", scope="////") -->

Example code included from
[examples/container/quick_runtime.cpp](examples/container/quick_runtime.cpp):

```c++
// User types do not need Dingo-specific base classes or macros.
struct A {
  A() = default;
};
struct B {
  B(A &, std::shared_ptr<A>) {}
};
struct C {
  C(B *, std::unique_ptr<B> &, A &) {}
};

container<> container;
// A and B are shared, so every resolution reuses the same instances.
container.register_type<scope<shared>, storage<std::shared_ptr<A>>>();
container.register_type<scope<shared>, storage<std::unique_ptr<B>>>();

// C is unique, so every resolve<C>() creates a fresh C.
container.register_type<scope<unique>, storage<C>>();

// Constructor arguments are resolved recursively, including ownership
// conversions from the registered storage forms.
C c = container.resolve<C>();

struct D {
  A &a;
  B *b;
};

// Construct an unmanaged object using dependencies from the container.
D d = container.construct<D>();

// Or invoke a callable with resolved arguments.
D e = container.invoke([&](A &a, B *b) { return D{a, b}; });
```

<!-- } -->

## Compile-Time Registration

Compile-time registration uses `bindings<...>` with `container<bindings<...>>`
or `static_container<bindings<...>>`, which fits graphs known in the type
system.

<!-- { include("examples/container/quick_static.cpp", scope="////") -->

Example code included from
[examples/container/quick_static.cpp](examples/container/quick_static.cpp):

```c++
struct A {
  A() = default;
};
struct B {
  B(A &, std::shared_ptr<A>) {}
};
struct C {
  C(B *, std::unique_ptr<B> &, A &) {}
};

// Compile-time bindings declare the same storage and scope as runtime
// registration.
using app_bindings =
    bindings<dingo::bind<scope<shared>, storage<std::shared_ptr<A>>>,
             dingo::bind<scope<shared>, storage<std::unique_ptr<B>>>,
             dingo::bind<scope<unique>, storage<C>>>;

container<app_bindings> container;
// Resolution still uses the same API.
C c = container.resolve<C>();

struct D {
  A &a;
  B *b;
};

// construct() and invoke() work with compile-time registration, too.
D d = container.construct<D>();
D e = container.invoke([&](A &a, B *b) { return D{a, b}; });
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
                     GIT_TAG        v0.2.0)
FetchContent_MakeAvailable(dingo)
target_link_libraries(dingo_fetchcontent_test PRIVATE dingo::dingo)
```

<!-- } -->

When working from a checkout, the top-level CMake project also supports
`add_subdirectory(...)`.

## Documentation

- [Getting Started](docs/getting-started.md): installation, runtime
  registration, compile-time registration, `resolve()`, `construct()`, and
  `invoke()`.
- [Core Concepts](docs/core-concepts.md): registration modes, scopes, storage,
  arrays, variants, factories, interfaces, and multibindings.
- [Advanced Topics](docs/advanced-topics.md): indexed resolution, annotations,
  runtime and compile-time registration modes, nesting, RTTI, allocation, and
  runtime notes.
- [Architecture](docs/architecture/README.md): registration flow, lookup shape,
  extension traits, and conversion rules.
- [Examples](docs/examples.md): a guided index of runnable examples in
  [`examples/`](examples).
- [Development](docs/development.md): local development builds, repo tooling,
  Markdown verification, and CI container notes.
- [Motivation and History](docs/history.md): background on how the library
  evolved.

## Feature Summary

- Non-intrusive registration of ordinary C++ types
- Runtime and compile-time registration modes
- Explicit control over lifetime and stored representation
- Constructor deduction with explicit factory overrides when needed
- Interface bindings and multibindings
- Indexed and annotated resolution
- Parent-child container nesting
- Array support for raw arrays and smart-pointer-backed arrays
- Explicit variant construction plus whole-variant or held-alternative
  resolution
- Custom RTTI and allocator hooks
- Extensive generated matrix tests across features, registration modes, scopes,
  stored types, resolved types, and containers. See the
  [generated matrix test model](test/matrix/README.md).

## Related Projects

For DI library comparisons, also take a look at:

- [qlibs/di](https://github.com/qlibs/di)
- [boost-ex/di](https://github.com/boost-ext/di)
- [google/fruit](https://github.com/google/fruit)
- [gracicot/kangaru](https://github.com/gracicot/kangaru)
- [ybainier/Hypodermic](https://github.com/ybainier/Hypodermic)
