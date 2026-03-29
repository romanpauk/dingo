# Examples

The repository ships runnable examples under [`examples/`](../examples). This
page groups them by topic so you can jump to the smallest relevant program
instead of scanning the full API surface.

## Start Here

- [examples/quick.cpp](../examples/quick.cpp): shortest end-to-end example with
  registration, resolution, `construct()`, and `invoke()`
- [examples/non_intrusive.cpp](../examples/non_intrusive.cpp): registration
  policies without intrusive type changes

## Factories

- [examples/factory_constructor.cpp](../examples/factory_constructor.cpp):
  automatic constructor deduction
- [examples/factory_constructor_concrete.cpp](../examples/factory_constructor_concrete.cpp):
  explicit constructor selection when deduction is ambiguous
- [examples/factory_static_function.cpp](../examples/factory_static_function.cpp):
  static factory-function registration
- [examples/factory_callable.cpp](../examples/factory_callable.cpp): stateful
  callable-based construction

## Scopes And Lifetime

- [examples/scope_external.cpp](../examples/scope_external.cpp): use an existing
  instance
- [examples/scope_unique.cpp](../examples/scope_unique.cpp): create a fresh
  instance for each resolution
- [examples/scope_shared.cpp](../examples/scope_shared.cpp): cache and reuse a
  shared instance
- [examples/scope_shared_cyclical.cpp](../examples/scope_shared_cyclical.cpp):
  build cyclic graphs with two-phase construction

## Interfaces, Collections, And Dispatch

- [examples/service_locator.cpp](../examples/service_locator.cpp): resolve a
  concrete type through an interface
- [examples/multibindings.cpp](../examples/multibindings.cpp): resolve multiple
  implementations through one interface
- [examples/collection.cpp](../examples/collection.cpp): aggregate a custom
  collection shape with a custom insertion rule
- [examples/message_processing.cpp](../examples/message_processing.cpp): route
  messages by indexed interface resolution

## Construction Helpers

- [examples/construct.cpp](../examples/construct.cpp): build an unmanaged object
  with injected dependencies
- [examples/invoke.cpp](../examples/invoke.cpp): invoke a callable with injected
  arguments

## Lookup And Container Topology

- [examples/index.cpp](../examples/index.cpp): resolve by interface and runtime
  key
- [examples/nesting.cpp](../examples/nesting.cpp): override registrations in a
  parent-child container hierarchy

## Infrastructure

- [examples/allocator.cpp](../examples/allocator.cpp): provide a custom
  allocator for container internals

## Building The Examples

Examples are enabled through the development build:

```sh
cmake -S . -B build -DDINGO_DEVELOPMENT_MODE=ON -DDINGO_EXAMPLES_ENABLED=ON
cmake --build build
ctest --test-dir build --output-on-failure
```
