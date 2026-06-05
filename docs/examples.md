# Examples

The repository ships runnable examples under [`examples/`](../examples). This
index is meant for quick entry points: find the smallest program that matches
the relevant feature and start there.

## Start Here

- [examples/container/quick_runtime.cpp](../examples/container/quick_runtime.cpp):
  shortest path from runtime registration to resolution
- [examples/container/quick_static.cpp](../examples/container/quick_static.cpp):
  shortest path from compile-time bindings to resolution
- [examples/registration/runtime_registration.cpp](../examples/registration/runtime_registration.cpp):
  runtime registration with shared and unique storage plus `resolve()` and
  `construct()`
- [examples/registration/non_intrusive.cpp](../examples/registration/non_intrusive.cpp):
  registration shape and policy deduction without touching user types
- [examples/registration/compile_time_registration.cpp](../examples/registration/compile_time_registration.cpp):
  compile-time bindings for the same storage and resolution shape as the runtime
  example

## Factories

- [examples/factory/factory_constructor_deduction.cpp](../examples/factory/factory_constructor_deduction.cpp):
  what the default constructor selection does
- [examples/factory/factory_constructor.cpp](../examples/factory/factory_constructor.cpp):
  how to pin construction to a specific overload
- [examples/factory/factory_function.cpp](../examples/factory/factory_function.cpp):
  static member-function-based construction
- [examples/factory/factory_callable.cpp](../examples/factory/factory_callable.cpp):
  stateful callable-based construction with injected arguments

## Scopes

- [examples/storage/scope_external.cpp](../examples/storage/scope_external.cpp):
  use an existing object without moving ownership into the container
- [examples/storage/scope_unique.cpp](../examples/storage/scope_unique.cpp):
  create a fresh instance on every resolution
- [examples/storage/scope_shared.cpp](../examples/storage/scope_shared.cpp):
  cache and reuse a single stored instance
- [examples/storage/scope_shared_cyclical.cpp](../examples/storage/scope_shared_cyclical.cpp):
  resolve cyclic graphs through two-phase construction

## Arrays

- [examples/storage/array.cpp](../examples/storage/array.cpp): register and
  resolve raw arrays plus smart-array forms

## Wrappers

- [examples/container/variant.cpp](../examples/container/variant.cpp): construct
  variants and resolve either the whole variant or its held alternative from
  variant storage

## Interfaces, Collections, And Dispatch

- [examples/container/service_locator.cpp](../examples/container/service_locator.cpp):
  resolve a concrete type through an interface view
- [examples/index/multibindings.cpp](../examples/index/multibindings.cpp):
  resolve multiple implementations through a single interface
- [examples/registration/collection.cpp](../examples/registration/collection.cpp):
  aggregate a custom collection shape with a custom insertion step
- [examples/registration/annotated.cpp](../examples/registration/annotated.cpp):
  disambiguate same-type dependencies with annotation tags
- [examples/index/message_processing.cpp](../examples/index/message_processing.cpp):
  dispatch messages to indexed handlers

## Construction Helpers

- [examples/container/construct.cpp](../examples/container/construct.cpp): build
  an unmanaged object from registered dependencies
- [examples/container/invoke.cpp](../examples/container/invoke.cpp): invoke a
  callable with injected arguments supplied by the container

## Lookup And Container Topology

- [examples/index/index.cpp](../examples/index/index.cpp): resolve by interface
  and runtime key
- [examples/container/nesting.cpp](../examples/container/nesting.cpp): override
  registrations in a parent-child container chain

## Infrastructure

- [examples/container/allocator.cpp](../examples/container/allocator.cpp):
  provide a custom allocator for container bookkeeping

## Building The Examples

Examples are enabled through the development build:

```sh
cmake -S . -B build -DDINGO_DEVELOPMENT_MODE=ON -DDINGO_EXAMPLES_ENABLED=ON
cmake --build build
ctest --test-dir build --output-on-failure
```
