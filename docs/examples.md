# Examples

The repository ships runnable examples under [`examples/`](../examples). This
index is meant for quick entry points: find the smallest program that matches
the feature you care about and start there.

## Start Here

- [examples/quick.cpp](../examples/quick.cpp): shortest path from registration
  to resolution
- [examples/non_intrusive.cpp](../examples/non_intrusive.cpp): registration
  shape and policy deduction without touching user types

## Factories

- [examples/factory_constructor_deduction.cpp](../examples/factory_constructor_deduction.cpp):
  what the default constructor selection does
- [examples/factory_constructor.cpp](../examples/factory_constructor.cpp): how
  to pin construction to a specific overload
- [examples/factory_function.cpp](../examples/factory_function.cpp): static
  member-function-based construction
- [examples/factory_callable.cpp](../examples/factory_callable.cpp): stateful
  callable-based construction with injected arguments

## Scopes, Arrays, And Wrappers

- [examples/scope_external.cpp](../examples/scope_external.cpp): use an existing
  object without moving ownership into the container
- [examples/scope_unique.cpp](../examples/scope_unique.cpp): create a fresh
  instance on every resolution
- [examples/scope_shared.cpp](../examples/scope_shared.cpp): cache and reuse a
  single stored instance
- [examples/scope_shared_cyclical.cpp](../examples/scope_shared_cyclical.cpp):
  resolve cyclic graphs through two-phase construction
- [examples/array.cpp](../examples/array.cpp): register and resolve raw arrays
  plus smart-array forms
- [examples/variant.cpp](../examples/variant.cpp): construct variants and store
  registrations whose storage is itself a variant

## Interfaces, Collections, And Dispatch

- [examples/service_locator.cpp](../examples/service_locator.cpp): resolve a
  concrete type through an interface view
- [examples/multibindings.cpp](../examples/multibindings.cpp): resolve multiple
  implementations through a single interface
- [examples/collection.cpp](../examples/collection.cpp): aggregate a custom
  collection shape with a custom insertion step
- [examples/message_processing.cpp](../examples/message_processing.cpp): route
  messages to indexed handlers

## Construction Helpers

- [examples/construct.cpp](../examples/construct.cpp): build an unmanaged object
  from registered dependencies
- [examples/invoke.cpp](../examples/invoke.cpp): invoke a callable with injected
  arguments supplied by the container

## Lookup And Container Topology

- [examples/index.cpp](../examples/index.cpp): resolve by interface and runtime
  key
- [examples/nesting.cpp](../examples/nesting.cpp): override registrations in a
  parent-child container chain

## Infrastructure

- [examples/allocator.cpp](../examples/allocator.cpp): provide a custom
  allocator for container bookkeeping

## Building The Examples

Examples are enabled through the development build:

```sh
cmake -S . -B build -DDINGO_DEVELOPMENT_MODE=ON -DDINGO_EXAMPLES_ENABLED=ON
cmake --build build
ctest --test-dir build --output-on-failure
```
