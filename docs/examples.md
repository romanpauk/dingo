# Examples

The repository contains runnable examples in [`examples/`](../examples). Use
this index to find the smallest program that shows a feature.

## Start Here

- [examples/container/quick_runtime.cpp](../examples/container/quick_runtime.cpp):
  shortest path from runtime registration to resolution
- [examples/container/quick_static.cpp](../examples/container/quick_static.cpp):
  shortest path from compile-time bindings to resolution
- [examples/registration/runtime_registration.cpp](../examples/registration/runtime_registration.cpp):
  runtime registration with `shared` and `unique` scopes, `resolve()` and
  `construct()`
- [examples/registration/non_intrusive.cpp](../examples/registration/non_intrusive.cpp):
  a full registration and the short form with deduced policies; the user types
  stay unchanged
- [examples/registration/compile_time_registration.cpp](../examples/registration/compile_time_registration.cpp):
  compile-time bindings with the same storage and resolution as the runtime
  example

## Factories

- [examples/factory/factory_constructor_deduction.cpp](../examples/factory/factory_constructor_deduction.cpp):
  the default constructor selection
- [examples/factory/factory_constructor.cpp](../examples/factory/factory_constructor.cpp):
  selection of one constructor overload
- [examples/factory/factory_function.cpp](../examples/factory/factory_function.cpp):
  construction with a static member function
- [examples/factory/factory_callable.cpp](../examples/factory/factory_callable.cpp):
  construction with a stateful callable and injected arguments

## Scopes

- [examples/storage/scope_external.cpp](../examples/storage/scope_external.cpp):
  use an existing object; the container does not take ownership
- [examples/storage/scope_unique.cpp](../examples/storage/scope_unique.cpp):
  create a new instance for each resolution
- [examples/storage/scope_shared.cpp](../examples/storage/scope_shared.cpp):
  cache one stored instance and reuse it
- [examples/storage/scope_shared_cyclical.cpp](../examples/storage/scope_shared_cyclical.cpp):
  resolve cyclic graphs through two-phase construction

## Arrays

- [examples/storage/array.cpp](../examples/storage/array.cpp): register and
  resolve raw arrays and smart-pointer arrays

## Wrappers

- [examples/container/variant.cpp](../examples/container/variant.cpp): construct
  variants; resolve the whole stored variant or its held alternative

## Interfaces, Collections, And Dispatch

- [examples/container/service_locator.cpp](../examples/container/service_locator.cpp):
  resolve a concrete type through an interface lookup
- [examples/index/multibindings.cpp](../examples/index/multibindings.cpp):
  resolve several implementations through one interface
- [examples/registration/collection.cpp](../examples/registration/collection.cpp):
  build a custom collection type with a custom insertion function
- [examples/registration/annotated.cpp](../examples/registration/annotated.cpp):
  disambiguate same-type dependencies with annotation tags
- [examples/index/message_processing.cpp](../examples/index/message_processing.cpp):
  dispatch messages to keyed handlers

## Construction Helpers

- [examples/container/construct.cpp](../examples/container/construct.cpp): build
  an unmanaged object from registered dependencies
- [examples/container/invoke.cpp](../examples/container/invoke.cpp): call a
  callable with arguments that the container resolves

## Lookup And Container Topology

- [examples/index/index.cpp](../examples/index/index.cpp): resolve by interface
  and runtime key
- [examples/container/nesting.cpp](../examples/container/nesting.cpp): override
  registrations in a chain of parent and child containers

## Infrastructure

- [examples/container/allocator.cpp](../examples/container/allocator.cpp): give
  the container a custom allocator for its internal data

## Building The Examples

The development build compiles the examples:

```sh
cmake -S . -B build -DDINGO_DEVELOPMENT_MODE=ON -DDINGO_EXAMPLES_ENABLED=ON
cmake --build build
ctest --test-dir build --output-on-failure
```
