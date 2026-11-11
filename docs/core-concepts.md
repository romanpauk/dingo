# Core Concepts

This page describes the features that control the object graph: registration
modes, lifetimes, stored forms, arrays, variants, factories and interface
resolution.

The [architecture docs](architecture/README.md) describe the internal model.

## Registration Model

A managed type does not inherit from a framework base class and does not expose
metadata. The registration describes how Dingo handles the type. Runtime calls
or compile-time bindings make the registration.

Common registration policies:

- `scope<...>` selects lifetime and caching behavior
- `storage<...>` selects the stored representation
- `interfaces<...>` exposes the registration through one or more interface types
- `factory<...>` overrides construction when constructor deduction is not enough

Dingo deduces a missing policy from the other policies, so most registrations
are short.

Both registration modes use the same policies:

```c++
container<> runtime_container;
runtime_container.register_type<scope<shared>, storage<config>>();

using static_registry =
    bindings<bind<scope<shared>, storage<config>>>;
static_container<static_registry> compile_time_container;
```

Runtime registration changes the configuration of a container object.
Compile-time bindings are part of the container type. A binding can list
`dependencies<...>`. The compiler checks them as part of the static graph. The
static container rejects a cycle, unless every binding in the cycle uses
`scope<shared_cyclical>`.

<!-- { include("../examples/registration/non_intrusive.cpp", scope="////") -->

Example code included from
[../examples/registration/non_intrusive.cpp](../examples/registration/non_intrusive.cpp):

```c++
container<> container;
// Registration of a struct A
container.register_type<scope<unique>, // using unique scope
                        factory<constructor<A>>,
                        // using constructor detection
                        storage<std::unique_ptr<A>>, // stored as
                                                     // unique_ptr<A>
                        interfaces<A>                // resolvable as A
                        >();
// As some policies can be deduced from the others, the above
// registration simplified
container.register_type<scope<unique>, storage<std::unique_ptr<A>>>();
```

<!-- } -->

See:

- [include/dingo/registration/type_registration.h](../include/dingo/registration/type_registration.h)

## Scopes And Storage

Scope and storage work together. The scope controls the lifetime. The storage
controls the form that Dingo keeps and converts during resolution.

### Scopes

- `external`: refers to an object that already exists.
- `unique`: builds a new instance for each resolution. These rules apply:
  - The container gives the instance by value, or moves it into a
    `std::optional`, `std::unique_ptr` or `std::shared_ptr` of its type or of an
    interface.
  - The container moves the instance out of a stored `std::optional` or
    `std::unique_ptr`, also into another of these wrappers. It copies the
    instance out of a stored `std::shared_ptr`.
  - The container gives a reference or a pointer only where it cannot dangle.
    The rule covers the instance, the handle, optional or alternative type that
    stores it, the element of that type, and values that the request makes of
    the instance, such as a `std::shared_ptr` of an interface.
  - A callable that `invoke` calls can take such a reference while the call
    runs.
  - An instance that a `shared` or `shared_cyclical` scope keeps can take such a
    reference. The scope keeps the unique instance for the lifetime of the
    container.
  - A class that goes to the caller cannot take such a reference. `resolve`,
    `construct` and their dependencies give classes to the caller. A static
    container rejects the reference at compile time. A runtime container throws
    `type_not_convertible_exception`. Such a class must take the value.
- `shared`: caches one instance and reuses it.
- `shared_cyclical`: caches instances and allows cyclic graphs.

### Storage Forms

The stored type can be a value, a raw pointer, `std::unique_ptr`,
`std::shared_ptr`, `std::optional`, a raw array, or another form that
`type_traits` supports. The wrappers compose, for example
`std::shared_ptr<std::optional<T>>` or `std::optional<std::unique_ptr<T>>`.
Dingo keeps the usual semantics of each form.

A request can compose wrappers too. If the binding does not publish a wrapper,
Dingo resolves the held value first and makes the wrapper of it. Wrapping keeps
the `unique` and `shared` semantics:

- A `unique` registration of `T` gives a `std::optional<std::shared_ptr<T>>` or
  a `std::shared_ptr<std::unique_ptr<T>>`. Dingo moves the new `T` one time,
  into all the new layers.
- A new wrapper holds what the request owns. It never holds a copy of what a
  scope keeps.
- A `shared` registration of `std::shared_ptr<T>` gives a
  `std::optional<std::shared_ptr<T>>`. The optional holds a copy of the handle,
  so it shares the one `T`.
- The binding keeps such a wrapper with its instance. A
  `std::optional<std::shared_ptr<T>> &` refers to the same wrapper for the
  lifetime of the container. A `std::shared_ptr<I> &` of an interface behaves in
  the same way.
- A wrapper of what a `unique` scope makes belongs to the request. The request
  takes it only by value.
- A `shared` registration of a plain `T` does not give a `std::shared_ptr<T>` or
  a `std::optional<T>`, because Dingo would have to copy the `T`. Register the
  `std::shared_ptr<T>` instead.

The injectable result depends on the scope and on the stored type:

- A `shared` registration stored as `std::shared_ptr<T>` satisfies `T &`, `T *`
  and `std::shared_ptr<T>` dependencies.
- An `external` registration exposes an object that code outside the container
  owns. The container does not take ownership.
- By design, some exact wrapper forms resolve only as the wrapper itself, not as
  each contained alternative.
- An immovable value can be neither copied nor moved. Dingo constructs it in
  place, and also a `std::optional` of it that a `shared` scope keeps. Resolve
  it by reference or by pointer.
- An `external` registration keeps a copy of the value that it receives. Give an
  immovable value by reference.
- `resolve<T &&>()` moves out of the object, so it is not available for an
  immovable value.

<!-- { include("../examples/storage/scope_external.cpp", scope="////") -->

Example code included from
[../examples/storage/scope_external.cpp](../examples/storage/scope_external.cpp):

```c++
struct A {};

A instance;
container<> container;
// Register existing instance of A, stored as a pointer.
container.register_type<scope<external>, storage<A *>>(&instance);
// Resolution will return an existing instance of A casted to required type.
assert(&container.resolve<A &>() == container.resolve<A *>());
```

<!-- } -->

<!-- { include("../examples/storage/scope_unique.cpp", scope="////") -->

Example code included from
[../examples/storage/scope_unique.cpp](../examples/storage/scope_unique.cpp):

```c++
struct A {};
container<> container;
// Register struct A with unique scope
container.register_type<scope<unique>, storage<A>>();
// Resolution will get a unique instance of A
container.resolve<A>();
```

<!-- } -->

<!-- { include("../examples/storage/scope_shared.cpp", scope="////") -->

Example code included from
[../examples/storage/scope_shared.cpp](../examples/storage/scope_shared.cpp):

```c++
struct A {};
container<> container;
// Register struct A with shared scope
container.register_type<scope<shared>, storage<A>>();
// Resolution will always return the same A instance
assert(container.resolve<A *>() == &container.resolve<A &>());
```

<!-- } -->

<!-- { include("../examples/storage/scope_shared_cyclical.cpp", scope="////") -->

Example code included from
[../examples/storage/scope_shared_cyclical.cpp](../examples/storage/scope_shared_cyclical.cpp):

```c++
// Forward-declare structs that have cyclical dependency
struct A;
struct B;

// Declare struct A, note that its constructor is taking arguments of struct B
struct A {
  A(B &b, std::shared_ptr<B> bptr) : b_(b), bptr_(std::move(bptr)) {}
  B &b_;
  std::shared_ptr<B> bptr_;
};

// Declare struct B, note that its constructor is taking arguments of struct A
struct B {
  B(A &a, A *aptr) : a_(a), aptr_(aptr) {}
  A &a_;
  A *aptr_;
};

container<> container;

// Register struct A with cyclical scope
container.register_type<scope<shared_cyclical>, storage<A>>();
// Register struct B with cyclical scope
container.register_type<scope<shared_cyclical>, storage<std::shared_ptr<B>>>();

// Returns instance of A that has correctly set b_ member to an instance of
// B, and instance of B has correctly set a_ member to an instance of A.
// Conversions are supported with cycles, too.
A &a = container.resolve<A &>();
B &b = container.resolve<B &>();
// Check that the instances are constructed as promised
assert(&a.b_ == &b);
assert(&a.b_ == a.bptr_.get());
assert(&b.a_ == &a);
assert(b.aptr_ == &a);
```

<!-- } -->

A `shared_cyclical` scope allocates storage for its instance. It publishes the
instance before it constructs it. Thus the scope stores only types that it can
construct in that storage:

- a class
- a bounded array
- an alternative type
- a `std::shared_ptr` of a class or of a wrapper of a class, such as
  `std::shared_ptr<std::optional<A>>`

For a wrapper, the scope publishes the wrapper itself, as `std::optional<A> &`
or `std::shared_ptr<std::optional<A>>`. A cycle can close over the wrapper. The
held `A` does not exist until the scope constructs the wrapper. Thus the scope
does not publish `A &`.

See:

- [include/dingo/storage/external.h](../include/dingo/storage/external.h)
- [include/dingo/storage/unique.h](../include/dingo/storage/unique.h)
- [include/dingo/storage/shared.h](../include/dingo/storage/shared.h)
- [include/dingo/storage/shared_cyclical.h](../include/dingo/storage/shared_cyclical.h)

## Arrays

Dingo supports raw C++ arrays, `std::unique_ptr<T[]>`, `std::shared_ptr<T[]>`
and `std::shared_ptr<T[N]>`, also with N-D shapes.

- The type `std::shared_ptr<T[N]>` contains the size. The default factory makes
  it in the same way as a `T[N]`, with value-initialized elements. If the
  elements cannot be value-initialized, register a factory.
- A `std::shared_ptr<T[]>` needs a factory that knows the size.
- Dingo does not support `std::unique_ptr<T[N]>`. Its default deleter would
  release the array as a single object.

Usage:

- Register the exact array shape that the container stores.
- Resolve the exact shape, or a borrowed or owning view that the scope supports.
- Nested arrays do not flatten to `T*`.

Constraints:

- Dingo does not support `resolve<T[N]>()`, because it does not return arrays by
  value.
- `unique` array storage gives owning handles, such as `std::unique_ptr<T[]>`,
  and a `std::shared_ptr<T[N]>` of a bounded array.
- A `unique` `std::unique_ptr<T[]>` does not resolve as a `T *` for the caller.
  That would release the owner that the registration declares and give an owning
  raw pointer. Resolve the `std::unique_ptr<T[]>` instead.
- A callable that `invoke` calls, or a class that a `shared` scope keeps, can
  take that `T *`. The container keeps the owner while the pointer is in use.
- `shared` array storage gives stable borrowed views and shared handles.

<!-- { include("../examples/storage/array.cpp", scope="////") -->

Example code included from
[../examples/storage/array.cpp](../examples/storage/array.cpp):

```c++
using namespace dingo;

struct cell {
  cell() = default;
};

struct row_consumer {
  cell (*rows)[3];

  explicit row_consumer(cell (*init)[3]) : rows(init) {}
};

container<> raw_container;
// Register a raw N-D array in shared scope.
raw_container.register_type<scope<shared>, storage<cell[2][3]>>();

// Resolve row view, exact pointer view and inject the row view.
auto *rows = raw_container.resolve<cell (*)[3]>();
auto &exact = raw_container.resolve<cell (&)[2][3]>();
row_consumer borrowed =
    raw_container
        .construct<row_consumer, constructor<row_consumer(cell (*)[3])>>();

container<> unique_container;
// Register a fixed-size array in unique scope and resolve it as an owning
// dynamic array handle.
unique_container.register_type<scope<unique>, storage<cell[4]>>();
auto owned = unique_container.resolve<std::unique_ptr<cell[]>>();

container<> shared_container;
// Register a shared smart array directly.
shared_container.register_type<scope<shared>, storage<std::shared_ptr<cell[]>>>(
    callable([] { return std::shared_ptr<cell[]>(new cell[4]); }));
auto shared = shared_container.resolve<std::shared_ptr<cell[]>>();
```

<!-- } -->

See:

- [include/dingo/type/type_traits.h](../include/dingo/type/type_traits.h)

## Variants

Dingo uses variants in two ways:

- `construct<std::variant<A, B>, constructor<A>>()` constructs the variant. It
  selects `A` as the alternative to build.
- `register_type<..., storage<std::variant<A, B>>, factory<...>>()` stores the
  variant in the container. The container then resolves the whole variant or the
  alternative that it holds.

The rules are narrow by design:

- The selected alternative must occur exactly once in the variant type.
- The whole variant stays resolvable.
- An alternative that occurs once is also resolvable from variant storage.
- If the instance holds a different alternative than the request, resolution
  throws `type_not_convertible_exception`.
- Direct resolution of a duplicate alternative type fails at compile time.
- `unique` variant storage resolves the whole variant and the held alternative
  as a value or an rvalue.
- `shared` and `external` variant storage resolve the whole variant and the held
  alternative as a value, a reference or a pointer.

<!-- { include("../examples/container/variant.cpp", scope="////") -->

Example code included from
[../examples/container/variant.cpp](../examples/container/variant.cpp):

```c++
struct A {
  explicit A(int init) : value(init) {}
  int value;
};

struct B {
  explicit B(float init) : value(init) {}
  float value;
};

container<> construct_container;
construct_container.register_type<scope<external>, storage<int>>(7);
construct_container.register_type<scope<external>, storage<float>>(3.5f);

// Construct a variant by selecting which alternative should be built.
[[maybe_unused]] auto detected =
    construct_container.construct<std::variant<A, B>, constructor<A>>();
assert(std::holds_alternative<A>(detected));

[[maybe_unused]] auto explicit_ctor =
    construct_container.construct<std::variant<A, B>, constructor<B(float)>>();
assert(std::holds_alternative<B>(explicit_ctor));

container<> unique_container;
unique_container.register_type<scope<unique>, storage<int>>();
unique_container.register_type<scope<unique>, storage<std::variant<A, B>>,
                               factory<constructor<A>>>();

// Resolve either the whole variant or its currently held alternative.
[[maybe_unused]] auto value = unique_container.resolve<std::variant<A, B>>();
assert(std::holds_alternative<A>(value));

[[maybe_unused]] auto selected = unique_container.resolve<A>();

try {
  unique_container.resolve<B>();
} catch (const type_not_convertible_exception &) {
}

std::variant<A, B> existing(std::in_place_type<A>, 9);
container<> external_container;
external_container
    .register_type<scope<external>, storage<std::variant<A, B> &>>(existing);

[[maybe_unused]] auto &ref = external_container.resolve<std::variant<A, B> &>();
assert(&ref == &existing);
assert(std::holds_alternative<A>(ref));

[[maybe_unused]] auto &held = external_container.resolve<A &>();
assert(&held == &std::get<A>(existing));
```

<!-- } -->

See:

- [test/resolution/request_identity.cpp](../test/resolution/request_identity.cpp)
- [include/dingo/type/type_traits.h](../include/dingo/type/type_traits.h)

## Factories

By default, Dingo selects a usable constructor automatically. An explicit
factory overrides this selection.

Factory kinds:

- automatic constructor deduction
- explicit constructor selection
- static function factory
- stateful callable factory

### Constructor Deduction

A registration without an explicit `factory<...>` uses `constructor<T>` from
[include/dingo/factory/constructor.h](../include/dingo/factory/constructor.h).
`constructor<T>` delegates to `constructor_detection<T>` in
[include/dingo/factory/constructor_detection.h](../include/dingo/factory/constructor_detection.h).

The default path:

- Dingo deduces the constructor of a registered type.
- It uses the constructor with the highest arity that it can prove
  constructible.
- It resolves the detected arguments from the container.

`construct<T>()` uses the same deduction for unmanaged objects.

Constructor detection has two result forms:

- Shape detection reports the kind of the selected constructor (concrete,
  generic or invalid) and its arity. Normal container construction uses shape
  detection.
- Signature detection starts from the same selected shape. It also recovers the
  parameter types as a `type_list`. Construction then uses the recovered list.
  Thus the reported dependencies and the injected dependencies are always the
  same.

Each detection result exposes `kind`, `arity` and `arguments`. For a concrete
signature result, `arguments` is the recovered parameter list. For a concrete
zero-argument result, it is `type_list<>`. Otherwise it is `void`. An explicit
constructor declaration contains its argument list, so it needs no signature
recovery.

### Constructor-Deduction Limits

Constructor deduction has these limits:

- Detection uses compile-time constructibility checks. It does not model every
  detail of overload resolution.
- The detector prefers the constructible shape with the highest arity. This is
  not always the overload that public code should use.
- Auto-detection rejects constructor templates and other generic catch-all
  constructors. Such types must use `factory<constructor<...>>`.
- Detection fails for a by-value parameter of a type that converts from anything
  that its element can be made of. Examples are `std::optional`, `std::any` and
  a wrapper with such a converting constructor. The compiler cannot rank that
  constructor against the conversion of the detection argument. Take the
  parameter by reference, or name the constructor.
- Detection works for a `std::variant` parameter. Its converting constructor
  needs exactly one alternative to convert to.
- Signature detection cannot tell a `T&&` parameter from a `T` parameter. It
  recovers and requests `T`.
- Resolve ambiguous or unsupported cases explicitly with
  `factory<constructor<...>>`, `factory<function<...>>`, `callable(...)` or
  `callable<Signature>(...)`.
- `construct<T>()` uses constructor deduction directly. `resolve<T>()` of an
  unregistered plain type auto-constructs only aggregates and types that opt in
  through `is_auto_constructible<T>`.
- A type that opts into auto-construction still goes through the ambiguity
  checks.
- Both containers auto-construct in the same way. A static container without a
  parent rejects at compile time a request that it can neither bind nor
  auto-construct.
- If Dingo does not detect the construction of `T`, a static container rejects
  `construct<T>()` at compile time. It knows that `T` is not bound. A runtime
  container knows this only at run time. It resolves a bound `T` and throws
  `type_not_found_exception` for an unbound `T`. Both containers reject at
  compile time a registration of such a `T` without a factory.
- Auto-construction requires a complete `T`.
- `resolve<T&>()` and `resolve<T*>()` accept a forward-declared `T` if `T` is
  registered.
- The construction of another type can depend on a forward-declared `T&` or
  `T*`. Dingo looks up the dependency and does not auto-construct it.
- A wrapper value takes part in the reference-based deduction only if its
  element can be borrowed. Its `type_traits` must declare `borrow`.
- Nested wrapper or alternative conversions can make a constructor probe
  ambiguous. Then automatic detection does not recover the signature. Give the
  exact signature with `factory<constructor<T(Args...)>>`.
- `constructor_detection_traits<T>::max_arity` bounds the constructor arity. The
  default is `DINGO_CONSTRUCTOR_DETECTION_ARGS`, which is 32 unless the build
  defines it.

Constructor deduction is a good default for ordinary classes and aggregates. Use
an explicit factory when:

- the constructor choice is part of the contract of the type
- constructor deduction is ambiguous
- the type has a templated or forwarding constructor
- a named factory function must build the type
- construction depends on extra callable state
- variant construction or storage must select a specific alternative

<!-- { include("../examples/factory/factory_constructor_deduction.cpp", scope="////") -->

Example code included from
[../examples/factory/factory_constructor_deduction.cpp](../examples/factory/factory_constructor_deduction.cpp):

```c++
struct A {
  A(int); // Definition is not required as constructor is not called
  A(double, double) {} // Definition is required as constructor is called
};

container<> container;
container.register_type<scope<external>, storage<double>>(1.1);

// Constructor with a highest arity will be used (factory<> is deduced
// automatically)
container
    .register_type<scope<unique>, storage<A> /*, factory<constructor<A>> */>();
```

<!-- } -->

<!-- { include("../examples/factory/factory_constructor.cpp", scope="////") -->

Example code included from
[../examples/factory/factory_constructor.cpp](../examples/factory/factory_constructor.cpp):

```c++
struct A {
  A(int);      // Definition is not required as constructor is not called
  A(double) {} // Definition is required as constructor is called
};

container<> container;
container.register_type<scope<external>, storage<double>>(1.1);

// Register A with the explicitly selected A(double) constructor.
// Manual disambiguation is required to avoid a compile-time assertion.
container.register_type<scope<unique>, storage<A>,
                        factory<constructor<A(double)>>>();
```

<!-- } -->

Dingo supports templated constructors if the registration gives an explicit
constructor signature:

```c++
struct Generic {
    template <class First, class Second>
    Generic(First, Second) {}
};

container.register_type<scope<external>, storage<int>>(7);
container.register_type<scope<external>, storage<float>>(3.5f);
container.register_type<scope<unique>, storage<Generic>,
                        factory<constructor<Generic(int, float)>>>();
```

<!-- { include("../examples/factory/factory_function.cpp", scope="////") -->

Example code included from
[../examples/factory/factory_function.cpp](../examples/factory/factory_function.cpp):

```c++
// Declare struct A that has an inaccessible constructor
struct A {
  // Provide factory function to construct instance of A
  static A factory() { return A(); }

private:
  A() = default;
};

container<> container;
// Register A that will be instantiated by calling A::factory()
container
    .register_type<scope<unique>, storage<A>, factory<function<&A::factory>>>();
```

<!-- } -->

<!-- { include("../examples/factory/factory_callable.cpp", scope="////") -->

Example code included from
[../examples/factory/factory_callable.cpp](../examples/factory/factory_callable.cpp):

```c++
struct A {
  int value;
};

struct overloaded_factory {
  A operator()(int value) const { return A{value * 2}; }
  A operator()(float value) const { return A{static_cast<int>(value) + 100}; }
};

container<> container;
// Register int that will be passed to the callable below
container.register_type<scope<external>, storage<int>>(2);
// Register A that will be instantiated by calling provided lambda function
// with arguments resolved using the container.
container.register_type<scope<unique>, storage<A>>(callable([](int value) {
  return A{value * 2};
}));
assert(container.resolve<A>().value == 4);
// Explicit signatures also work for overloaded functors.
assert(container.construct<A>(callable<A(int)>(overloaded_factory{})).value ==
       4);
```

<!-- } -->

See:

- [test/factory/constructor_detection.cpp](../test/factory/constructor_detection.cpp)
- [test/factory/constructor_detection_cases.cpp](../test/factory/constructor_detection_cases.cpp)
- [test/factory/factory_kinds.cpp](../test/factory/factory_kinds.cpp)
- [include/dingo/factory/constructor_detection.h](../include/dingo/factory/constructor_detection.h)
- [include/dingo/factory/constructor.h](../include/dingo/factory/constructor.h)
- [include/dingo/factory/function.h](../include/dingo/factory/function.h)
- [include/dingo/factory/callable.h](../include/dingo/factory/callable.h)

## Interfaces And Service-Locator Style Resolution

A registration can expose its type through base classes or other interface
types. The registration fixes the upcast. Thus multiple inheritance behaves
predictably, and resolution does not use `dynamic_cast`. Dingo gives a base by
reference, by pointer or by a handle that holds the registered object. Dingo
never gives a base by value, because a copy of the base would slice the object.

Use interfaces when:

- concrete implementations should stay hidden behind an interface
- one concrete type should be resolvable through several interfaces

<!-- { include("../examples/container/service_locator.cpp", scope="////") -->

Example code included from
[../examples/container/service_locator.cpp](../examples/container/service_locator.cpp):

```c++
// Interface that will be resolved
struct IA {
  virtual ~IA() = default;
};
// Struct implementing the interface
struct A : IA {};

container<> container;

// Register struct A, resolvable as interface IA
container.register_type<scope<shared>, storage<A>, interfaces<IA>>();
// Resolve instance A through interface IA
IA &instance = container.resolve<IA &>();
assert(dynamic_cast<A *>(&instance));
```

<!-- } -->

## Multibindings And Collections

Dingo can register several implementations under one interface. It then resolves
them as a collection.

Use collections for extension points, such as:

- processors
- handlers
- strategy lists

The collection uses a standard aggregation rule or a custom one.

<!-- { include("../examples/index/multibindings.cpp", scope="////") -->

Example code included from
[../examples/index/multibindings.cpp](../examples/index/multibindings.cpp):

```c++
struct IProcessor {
  virtual ~IProcessor() = default;
};

template <size_t N> struct Processor : IProcessor {};

struct container_traits : runtime_container_traits {
  using lookup_definition_type = lookups<collection<IProcessor>>;
};

container<container_traits> container;

// Register types under the same interface
container.template register_type<scope<shared>, storage<Processor<0>>,
                                 interfaces<IProcessor>>();
container.template register_type<scope<shared>, storage<Processor<1>>,
                                 interfaces<IProcessor>>();

// Resolve the collection
container.template resolve<std::vector<IProcessor *>>();
```

<!-- } -->

<!-- { include("../examples/registration/collection.cpp", scope="////") -->

Example code included from
[../examples/registration/collection.cpp](../examples/registration/collection.cpp):

```c++
struct ProcessorBase {
  virtual ~ProcessorBase() = default;
  virtual void process(const void *) = 0;
  virtual std::type_index type() = 0;
};

template <typename T, typename ProcessorT> struct Processor : ProcessorBase {
  virtual std::type_index type() override { return typeid(T); }

  void process(const void *transaction) override {
    static_cast<ProcessorT *>(this)->process_impl(
        *reinterpret_cast<const T *>(transaction));
  }
};

struct StringProcessor : Processor<std::string, StringProcessor> {
  void process_impl([[maybe_unused]] const std::string &value) {};
};

struct VectorIntProcessor : Processor<std::vector<int>, VectorIntProcessor> {
  void process_impl([[maybe_unused]] const std::vector<int> &value) {};
};

struct Dispatcher {
  Dispatcher(
      std::map<std::type_index, std::unique_ptr<ProcessorBase>> &&processors)
      : processors_(std::move(processors)) {}

  Dispatcher(const Dispatcher &) = delete;
  Dispatcher &operator=(const Dispatcher &) = delete;
  Dispatcher(Dispatcher &&) = default;
  Dispatcher &operator=(Dispatcher &&) = default;

  template <typename T> void process(const T &value) {
    processors_.at(typeid(T))->process(&value);
  }

private:
  std::map<std::type_index, std::unique_ptr<ProcessorBase>> processors_;
};

struct traits : dingo::runtime_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::collection<ProcessorBase>>;
};

container<traits> container;
container
    .register_type<scope<unique>, storage<std::unique_ptr<StringProcessor>>,
                   interfaces<ProcessorBase>>();
container
    .register_type<scope<unique>, storage<std::unique_ptr<VectorIntProcessor>>,
                   interfaces<ProcessorBase>>();
container.register_type_collection<
    scope<unique>,
    storage<std::map<std::type_index, std::unique_ptr<ProcessorBase>>>>(
    [](auto &collection, auto &&value) {
      collection.emplace(value->type(), std::forward<decltype(value)>(value));
    });

auto dispatcher = container.construct<Dispatcher>();
dispatcher.process(std::string(""));
```

<!-- } -->

See:

- [include/dingo/type/collection_traits.h](../include/dingo/type/collection_traits.h)

## Runtime Model And Error Handling

Runtime registration works across modules. Its wiring errors appear as
exceptions during resolution. Compile-time bindings put the declared graph into
the type system. The compiler then reports missing static dependencies and
unsupported static cycles.

Container changes are atomic. If a registration or a resolution throws, the
container returns to its state at the start of the operation. Thus an operation
either succeeds completely or leaves the container unchanged. The container
rolls back partial registrations, cached resolutions, lookup entries and the
objects that it owns.

In a container hierarchy, each container with runtime state is a separate atomic
unit. A fallback to the parent runs as an operation of the parent:

- If the parent resolution succeeds and a later child construction fails, the
  parent keeps its result.
- If the parent operation fails, the parent rolls back before the exception
  propagates.

This guarantee covers the state that the container manages. Dingo cannot undo
side effects of user constructors, factories, callbacks or destructors that
occur before they throw.

See:

- [Advanced Topics](advanced-topics.md)
