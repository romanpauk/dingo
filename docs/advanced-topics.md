# Advanced Topics

This page covers the parts of Dingo that usually matter only after the basic
registration and resolution flow is already in place.

## Lookup Resolution

Container traits can declare lookups so that a registration is resolved by
interface, key domain, and cardinality.

Use lookups when:

- multiple implementations share the same interface
- the caller chooses the implementation by name or numeric ID
- the binding should remain separate from unkeyed resolution

A domain is an interface with a kind of key: no key, a tag key `key_type<Tag>`,
or the values of a key type, fixed by `key_type<Key, Value>` or supplied at
runtime. A lookup declaration says whether a domain holds one binding of a key
or many, and which backend indexes the key values of a domain of a key type. A
domain no declaration names holds one binding of a key, and its key values are
indexed by `ordered`. A keyed registration needs no declaration.

A second registration of a key of a domain of one is rejected with
`lookup_already_registered_exception`, regardless of storage type; a static
container rejects a second static binding when the container type is formed.
Declare `collection<I>` when an interface is meant to have multiple unkeyed
implementations:

```c++
struct container_traits : dynamic_container_traits {
  using lookup_definition_type = lookups<collection<IProcessor>>;
};
```

`collection<I>` makes unkeyed collection membership explicit:
`resolve<std::vector<I *>>()` enumerates the registrations in registration
order, while singular `resolve<I &>()` succeeds only when exactly one
registration matches. A collection request of a domain declared one holds the
binding the domain has, and a collection request that names no binding is an
empty collection, in every container.

The declarations are:

- `single<I>`: the bindings of `I` without a key, one of them
- `collection<I>`: the bindings of `I` without a key, any number of them
- `typed<Tag, I, Cardinality = one>`: the bindings of `I` by the tag key
  `key_type<Tag>`
- `associative<Key, I, Cardinality = one, Backend = ordered>`: the bindings of
  `I` by a value of `Key`, fixed or supplied at runtime
- `base<Cardinality = one, Backend = ordered>`: every domain no other
  declaration covers

`I` may also be `interfaces<I...>`, which declares the domains of several
interfaces at once.

Runtime-keyed lookups use `associative<K, I>` for unique bindings and
`associative<K, I, many>` for keyed collections:

```c++
struct container_traits : dynamic_container_traits {
  using lookup_definition_type =
      lookups<associative<std::size_t, IProcessor>,
                associative<std::string, IHandler, many>>;
};
```

The built-in backends are:

- `ordered`: the sorted key values, searched in `O(log N)`, for a key type with
  `<`
- `unordered`: a hash table of the key values, average `O(1)`, for a key type
  with `std::hash`
- `array<N>`: a dense array of the key values `0` up to `N`, excluded, of an
  integral or enumeration key type

A backend falls back to an index the key type can serve: `ordered` to a hash
table, `unordered` to the sorted key values, and either to a linear scan, which
needs only `==`. A registration of a key value outside the bound of `array<N>`
raises `type_index_out_of_range_exception`, a static binding of one is rejected
when the container type is formed, and a request of one finds no binding. Every
index visits the bindings of a key in the order of registration, so a keyed
collection is in registration order whatever the backend.

For example:

```c++
struct container_traits : dynamic_container_traits {
  using lookup_definition_type =
      lookups<associative<std::size_t, IProcessor, many, unordered>,
              associative<MessageId, IHandler, one, array<32>>>;
};
```

A backend indexes key values only: a request without a key, or by a tag key,
visits the bindings of its domain in the order of registration.

### Custom Lookup Backends

A custom backend is used as `associative<MyKey, IService, many, my_backend>`, or
as `base<Cardinality, my_backend>` for the domains no other declaration covers.
A backend is a type that names a member template `index<Value, Candidate>`. The
container builds the index from the candidates of a request, each a
`dingo::index_item<Value, Candidate>` with its `key` and its `candidate`, in the
order of registration, and asks it to visit the candidates of a key. An index
must visit the candidates a linear scan visits, in the same order, so it never
changes which bindings a request names; it only accelerates the search. A
backend that names no index is rejected when it is declared.

```c++
#include <dingo/lookup/lookup.h>

#include <cstddef>
#include <map>
#include <string>
#include <vector>

// A lookup backend that indexes the key values in a std::map.
struct map_backend {
  template <typename Value, typename Candidate> class index {
  public:
    // Takes the candidates in the order of registration.
    void build(const dingo::index_item<Value, Candidate> *items,
               std::size_t count) {
      groups_.clear();
      for (std::size_t at = 0; at < count; ++at) {
        groups_[items[at].key].push_back(items[at].candidate);
      }
    }

    // Visits the candidates of `key` in the order of registration until
    // `visit` returns false.
    template <typename Visit>
    void visit(const Value &key, Visit &&visit) const {
      const auto found = groups_.find(key);
      if (found == groups_.end()) {
        return;
      }
      for (const Candidate &candidate : found->second) {
        if (!visit(candidate)) {
          return;
        }
      }
    }

  private:
    std::map<Value, std::vector<Candidate>> groups_;
  };
};

struct container_traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<std::string, IService, dingo::many, map_backend>>;
};
```

A backend may also name `bound`, the size of a dense array of its key values, as
`array<N>` does. The backends of the library name their indexes the same way;
see [Lookup Indexes](architecture/extending-dingo.md#lookup-indexes).

The local runtime bindings of a registration follow the lookup declarations of
the container.

Typed-key registrations can also use explicit lookups:

```c++
struct container_traits : dynamic_container_traits {
  using lookup_definition_type =
      lookups<typed<Primary, IProcessor, one>>;
};
```

`typed<K, I, one>` gives `register_type<interfaces<I>, key_type<K>>()` singular
identity for that interface and key type. `typed<K, I, many>` makes
`resolve<std::vector<I *>>(key_type<K>{})` enumerate keyed collection members in
registration order. A singular typed-key resolve succeeds only when the matching
`many` lookup contains exactly one registration.

When no explicit typed-key lookup is declared, `key_type<K>` registrations still
form a domain of one: a second registration for the same interface and key type
is rejected, regardless of storage type. Use `typed<K, I, many>` when one typed
key should hold multiple implementations.

Constructor dependencies can also bind to a fixed request key:

```c++
struct Pipeline {
    Pipeline(dingo::dependency<IProcessor&, dingo::key_type<std::size_t, 1>> first,
             dingo::dependency<IProcessor&, dingo::key_type<std::size_t, 2>> second);
};
```

`dependency<IProcessor&, key_type<std::size_t, 1>>` resolves the same object as
`container.resolve<IProcessor&>(std::size_t{1})`; the configured
`associative<std::size_t, IProcessor>` lookup determines the key domain and
cardinality.

The value in `key_type<T, Value>` must be a valid non-type template parameter
and must be usable as `T{Value}`. Class-type values such as
`key_type<MyKey, MyKey{1}>` require C++20 structural non-type template parameter
support.

Every kind of key works in every container:

- `key_type<K>` is a tag key of the domain of `typed<K, I, ...>`.
- `key_type<K, Value>` is a fixed key of the domain of `associative<K, I, ...>`.
  A runtime registration stores it as its value.
- A request by a runtime key value, `resolve<I &>(K{...})`, is compared with the
  fixed keys of the static bindings when it is resolved, in `static_container`
  as in `container<bindings<...>>`.
- A static container decides a request by a fixed key,
  `dependency<I &, key_type<K, Value>>` or
  `resolve<Collection>(key_type<K, Value>{})`, when the request is compiled.
- `container<bindings<...>, Traits>` declares lookups in its traits as the
  runtime container does, and rejects a runtime registration whose key a static
  binding holds.

<!-- { include("../examples/index/static_fixed.cpp", scope="////") -->

Example code included from
[../examples/index/static_fixed.cpp](../examples/index/static_fixed.cpp):

```c++
struct IProcessor {
  virtual ~IProcessor() = default;
  virtual int id() const = 0;
};

template <int Id> struct Processor : IProcessor {
  int id() const override { return Id; }
};

struct Pipeline {
  explicit Pipeline(
      dingo::dependency<IProcessor &, dingo::key_type<std::size_t, 0>>
          first_processor)
      : first(first_processor) {}

  IProcessor &first;
};

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Processor<0>>,
                dingo::interfaces<IProcessor>, dingo::key_type<std::size_t, 0>>,
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Processor<1>>,
                dingo::interfaces<IProcessor>,
                dingo::key_type<std::size_t, 1>>>;

struct container_traits : dingo::static_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, IProcessor>>;
};

dingo::static_container<source, container_traits> container;

auto &first = container.resolve<
    dingo::dependency<IProcessor &, dingo::key_type<std::size_t, 0>>>();
auto pipeline = container.construct<Pipeline>();

return first.id() == 0 && &pipeline.first == &first ? 0 : 1;
```

<!-- } -->

<!-- { include("../examples/index/index.cpp", scope="////") -->

Example code included from
[../examples/index/index.cpp](../examples/index/index.cpp):

```c++
struct IAnimal {
  virtual ~IAnimal() = default;
};

struct Dog : IAnimal {};
struct Cat : IAnimal {};

// Declare traits with a std::string based lookup
struct container_traits : dynamic_container_traits {
  using lookup_definition_type = lookups<associative<std::string, IAnimal>>;
};

container<container_traits> container;
container
    .template register_type<scope<shared>, storage<Dog>, interfaces<IAnimal>>(
        dingo::key_value{std::string("dog")});

container
    .template register_type<scope<shared>, storage<Cat>, interfaces<IAnimal>>(
        dingo::key_value{std::string("cat")});

// Resolve an instance of a dog
auto dog = container.template resolve<IAnimal>(std::string("dog"));
```

<!-- } -->

<!-- { include("../examples/index/message_processing.cpp", scope="////") -->

Example code included from
[../examples/index/message_processing.cpp](../examples/index/message_processing.cpp):

```c++
// Declare Messages and a wrapper that can hold one of the messages,
// resembling protobuf structure
struct MessageA {
  int value;
};
struct MessageB {
  float value;
};

struct MessageWrapper {
  template <typename T> MessageWrapper(T &&message) {
    messages_ = std::forward<T>(message);
  }

  size_t id() const { return messages_.index(); }
  const MessageA &GetA() const { return std::get<MessageA>(messages_); }
  const MessageB &GetB() const { return std::get<MessageB>(messages_); }

private:
  std::variant<std::monostate, MessageA, MessageB> messages_;
};

// Declare message processor hierarchy with dependencies
struct IProcessor {
  virtual ~IProcessor() = default;
  virtual void process(const MessageWrapper &) = 0;
};

struct RepositoryA {};
struct ProcessorA : IProcessor {
  ProcessorA(RepositoryA &) {}
  void process(const MessageWrapper &message) override { message.GetA(); }
};

struct RepositoryB {};
struct ProcessorB : IProcessor {
  ProcessorB(RepositoryB &) {}
  void process(const MessageWrapper &message) override { message.GetB(); }
};

struct Pipeline {
  Pipeline(dingo::dependency<IProcessor &, dingo::key_type<size_t, 1>>
               first_processor,
           dingo::dependency<IProcessor &, dingo::key_type<size_t, 2>>
               second_processor)
      : first(first_processor), second(second_processor) {}

  IProcessor &first;
  IProcessor &second;
};

// Define traits type with a single lookup using size_t as a key
struct container_traits : static_container_traits {
  using lookup_definition_type = lookups<associative<size_t, IProcessor>>;
};
// Runtime lookup storage is dynamic even when this
// example uses static_container_traits for the rest of the container.

container<container_traits> container;

// Register processors into the container, keyed by the type they process
container.register_type<scope<shared>, storage<std::shared_ptr<ProcessorA>>,
                        interfaces<IProcessor>>(dingo::key_value{size_t(1)});
container.register_type<scope<shared>, storage<std::shared_ptr<ProcessorB>>,
                        interfaces<IProcessor>>(dingo::key_value{size_t(2)});

// Register repositories used by the processors
container.register_type<scope<shared>, storage<RepositoryA>>();
container.register_type<scope<shared>, storage<RepositoryB>>();

auto pipeline = container.construct<Pipeline>();

// Invokes the processor for MessageA that is stateful
{
  MessageWrapper msg((MessageA{1}));
  container.template resolve<IProcessor &>(msg.id()).process(msg);
}

// Invokes the processor for MessageB that is stateless
{
  MessageWrapper msg((MessageB{1.1f}));
  container.template resolve<IProcessor &>(msg.id()).process(msg);
}
```

<!-- } -->

See:

- [include/dingo/lookup/lookup.h](../include/dingo/lookup/lookup.h)
- [include/dingo/lookup/indexes.h](../include/dingo/lookup/indexes.h)
- [include/dingo/backend/index.h](../include/dingo/backend/index.h)
- [test/next/containers/lookups.cpp](../test/next/containers/lookups.cpp)

## Annotated Types

Annotations support multiple implementations for the same interface and
disambiguate them with a tag type.

Use annotations when type-based registration is not enough but runtime key
lookups would be the wrong abstraction.

<!-- { include("../examples/registration/annotated.cpp", scope="////") -->

Example code included from
[../examples/registration/annotated.cpp](../examples/registration/annotated.cpp):

```c++
struct primary_tag {};
struct replica_tag {};

struct database {
  int id;
};

struct repository {
  repository(dingo::annotated<database &, primary_tag> primary_db,
             dingo::annotated<database &, replica_tag> replica_db)
      : primary(primary_db), replica(replica_db) {}

  database &primary;
  database &replica;
};

database primary{1};
database replica{2};
container<> container;

container.register_type<scope<external>, storage<database *>,
                        interfaces<annotated<database, primary_tag>>>(&primary);
container.register_type<scope<external>, storage<database *>,
                        interfaces<annotated<database, replica_tag>>>(&replica);
container.register_type<scope<unique>, storage<repository>>();

[[maybe_unused]] auto repo = container.resolve<repository>();
assert(&repo.primary == &primary);
assert(&repo.replica == &replica);
```

<!-- } -->

See:

- [test/matrix/README.md](../test/matrix/README.md)
- [include/dingo/type/annotated.h](../include/dingo/type/annotated.h)

## Custom Wrappers, Wrapper Semantics, And Scopes

A wrapper of the user becomes a wrapper of the conversion rules through
`dingo::wrapper_traits`, which names its wrapper semantics, its family, and its
element types, and tells how to rebind it, reach its element, and make it. The
standard wrappers specialize it the same way, so a user wrapper serves the same
requests in every scope as the standard wrapper of its kind. A wrapper that no
kind of the library describes declares a kind of its own with
`dingo::wrapper_semantics`: its properties and its operations.

A scope is a tag type whose `dingo::scope_traits` hold its
`dingo::scope_descriptor`. A user scope may copy the descriptor of a built-in
scope, and may name a slot class of its own derived from `dingo::scope_slot`:

```c++
#include <dingo/container.h>
#include <dingo/storage/shared.h>

// A scope that keeps one object per container, as shared does.
struct per_container {};

template <> struct dingo::scope_traits<per_container> {
  static constexpr dingo::scope_descriptor descriptor =
      dingo::scope_traits<dingo::shared>::descriptor;
};
```

```c++
dingo::container<> container;
container.register_type<dingo::scope<per_container>, dingo::storage<service>>();
assert(&container.resolve<service &>() == &container.resolve<service &>());
```

[Extending Dingo](architecture/extending-dingo.md) describes every extension
point with complete examples.

See:

- [include/dingo/type/type_traits.h](../include/dingo/type/type_traits.h)
- [include/dingo/type/wrapper_semantics.gen.h](../include/dingo/type/wrapper_semantics.gen.h)
- [include/dingo/conversion/scopes.gen.h](../include/dingo/conversion/scopes.gen.h)
- [test/next/containers/scopes.cpp](../test/next/containers/scopes.cpp)

## Runtime And Compile-Time Registration

Dingo has two registration modes with different tradeoffs.

- Runtime registration uses `container<>` or `runtime_container` and adds
  registrations with `register_type<...>()`.
- Compile-time registration uses `bindings<...>` with `container<bindings<...>>`
  or `static_container<bindings<...>>`.
- `container<bindings<...>>` may also accept runtime registrations, which is
  useful when most of the graph is static but a small boundary remains dynamic.

The modes share the same resolution API and policy declarations. The difference
is where the registration list lives: runtime container state or the container
type.

Runtime registration makes sense when:

- registrations are discovered or selected at runtime
- plugins, tests, or modules need to add registrations independently
- the graph is intentionally open-ended

Compile-time bindings make sense when:

- the participating types are known and stable at compile time
- missing dependencies and unsupported cycles should fail during compilation
- lookups should avoid mutable runtime registration state

See:

- [examples/container/quick_runtime.cpp](../examples/container/quick_runtime.cpp)
- [examples/registration/compile_time_registration.cpp](../examples/registration/compile_time_registration.cpp)
- [architecture/containers.md](architecture/containers.md)
- [include/dingo/container.h](../include/dingo/container.h)
- [include/dingo/runtime_container.h](../include/dingo/runtime_container.h)
- [include/dingo/static_container.h](../include/dingo/static_container.h)

## Template Depth Of Compile-Time Bindings

A compile-time container resolves a binding with code instantiated for that
binding, and that code resolves the binding's dependencies with code
instantiated for them in turn. The compiler instantiates these functions nested
inside each other, so a chain of dependencies, where each type depends on one
other type that depends on one other type, nests one set of instantiations per
link. The depth of the nesting is bounded by `-ftemplate-depth`, 1024 by default
in Clang and 900 in GCC. The depth follows the longest dependency chain, not the
number of bindings: bindings that do not depend on each other, or that all
depend on the same type, do not nest.

The longest chain that compiles at the default depth, measured with C++17 in
`container<bindings<...>>` for a chain whose links each take the previous link;
`static_container` compiles the same 69 and 52 links of the first row:

```text
binding of a link                          Clang 18   GCC 13
shared, constructor detected, by reference       69       52
shared, factory<constructor<T(Args...)>>         72       64
unique, constructor detected, by value           75       52
```

A shared link with a detected constructor costs about 14 levels in Clang and
about 17 in GCC: at twice the default depth, Clang 18 compiles 142 links and GCC
13 compiles 105.
[test/next/containers/dependency_chain.cpp](../test/next/containers/dependency_chain.cpp)
resolves a chain of 64 links on Clang and 48 on GCC, in a static and in a
runtime container.

The declared dependencies of a static container are also checked for cycles when
it is compiled, by a search from every binding whose cost, for a chain, grows
with the cube of its length. With `-ftemplate-depth=4096`, Clang 18 compiles a
chain of up to 86 links with `factory<constructor<T(Args...)>>`, and a longer
one exceeds the default limit of constant evaluation, `-fconstexpr-steps`.

When a longer chain does not compile, raise `-ftemplate-depth`, or register the
chain through `container<>` or `runtime_container`. A runtime registration
instantiates the construction of its binding when it is registered, and a
dependency resolves through the binding the container holds, so the depth of a
runtime chain does not depend on its length: a runtime chain of 200 links
compiles at the default depth with Clang 18 and GCC 13.

## Container Nesting

Containers can form a parent-child hierarchy. Resolution walks from the child
toward the parent chain, which allows local overrides on top of broader shared
configuration. Runtime-only containers use `container<> child(&parent)` for a
parent of their own type, or `container<Traits, Allocator, Parent>` for another.
Mixed static/runtime containers use `container<bindings<...>, Parent>` and
construct the child with `child(&parent)`. Purely static containers use
`static_container<bindings<...>, Parent>` the same way, and have the traits of
their parent. Static bindings in the child can resolve missing dependencies from
the parent's static or runtime graph.

A child shadows its parent for collection requests as for singular ones: a
collection request that names no binding of the child is the collection of its
parent. A binding the parent serves resolves its dependencies in the parent.

Nesting is most useful for:

- per-feature overrides
- test-specific replacements
- local configuration layered on top of a base application container

<!-- { include("../examples/container/nesting.cpp", scope="////") -->

Example code included from
[../examples/container/nesting.cpp](../examples/container/nesting.cpp):

```c++
struct A {
  int value;
};
struct B {
  int value;
};

container<> base_container;
base_container.register_type<scope<external>, storage<int>>(1);
base_container.register_type<scope<unique>, storage<A>>();
// Resolving A will use A{1} to construct A
assert(base_container.resolve<A>().value == 1);

base_container.register_type<scope<unique>, storage<B>>()
    ->register_type<scope<external>, storage<int>>(
        2); // Override value of int for struct B
// Resolving B will use B{2} to construct B
assert(base_container.resolve<B>().value == 2);

// Create a nested container, overriding B (effectively removing override in
// base_container)
container<> nested_container(&base_container);
nested_container.register_type<scope<unique>, storage<B>>();
// Resolving B using nested container will use B{1} as provided by the
// parent container to construct B
assert(nested_container.resolve<B>().value == 1);
```

<!-- } -->

See:

- [test/matrix/README.md](../test/matrix/README.md)
- [test/next/containers/parents.cpp](../test/next/containers/parents.cpp)
- [test/next/containers/static_parents.cpp](../test/next/containers/static_parents.cpp)

## Moving Containers

Runtime-only containers, `container<>` and `runtime_container`, and a mixed
container without static bindings, `container<bindings<>>`, are movable but not
copyable. They keep their state in one allocation made through the container
allocator, and moving transfers it. Registrations, resolved instances, the
registration proxies returned by `register_type<...>()`, and child containers
created with `child(&parent)` therefore stay valid when the container moves.
This supports builder-style setup that registers types and then hands the
container over. A moved-from container can only be destroyed or assigned to.

Containers with compile-time bindings, `static_container<bindings<...>>` and a
`container<bindings<...>>` with static bindings, keep their bindings and
instances in place and are neither movable nor copyable. A `static_container`
does not move even without bindings.

A child refers to the state of its parent, not to the parent object, so a child
of a movable parent follows the parent when it moves and finds the parent's
bindings at its new address.

See:

- [test/next/containers/container_move.cpp](../test/next/containers/container_move.cpp)

## Custom RTTI

The container identifies a type by a tag of its own and names a type in a
message by the compiler's spelling of it, so it needs no RTTI. Container traits
may name an RTTI provider, `rtti_type = dingo::rtti<Provider>`:

- `typeid_provider`, the provider of the language type information
- `static_provider`, the provider that needs no language type information
- a provider of the user, a specialization of `dingo::rtti`

The provider does not change what the container does.

See:

- [include/dingo/rtti/typeid_provider.h](../include/dingo/rtti/typeid_provider.h)
- [include/dingo/rtti/static_provider.h](../include/dingo/rtti/static_provider.h)

## Custom Allocation

The container can take a user-supplied allocator, as the second argument of
`container<Traits, Allocator>` or as the `allocator_type` of its traits.

A runtime container allocates its state and its bindings through the allocator,
with the key values the bindings hold and the objects their slots keep in place,
and frees every binding it allocated, also when a failed request undoes a
registration. The memos of requests, the indexes of their key values, the local
runtime bindings of a registration, and the objects a container keeps for
requests use the global allocator. The allocator need not align what it
allocates: an allocation is padded for the alignment of its object.

This affects container bookkeeping, not the ownership model of every resolved
object. A resolved object may still be externally owned or stored in a standard
smart pointer, depending on its registration.

<!-- { include("../examples/container/allocator.cpp", scope="////") -->

Example code included from
[../examples/container/allocator.cpp](../examples/container/allocator.cpp):

```c++
// Define a container with user-provided allocator type
std::allocator<char> alloc;
container<dingo::dynamic_container_traits, std::allocator<char>>
    container(alloc);
```

<!-- } -->

See:

- [include/dingo/memory/allocation.h](../include/dingo/memory/allocation.h)
- [include/dingo/container_traits.h](../include/dingo/container_traits.h)
- [test/next/containers/allocator.cpp](../test/next/containers/allocator.cpp)

## Runtime Notes

Some resolution details are easy to miss:

- a runtime registration that collides with a binding of its key throws when it
  is registered, and a missing dependency of a runtime binding throws during
  resolution, while compile-time bindings diagnose duplicate keys, declared
  graph errors, and ambiguities among static bindings during compilation
- shared resolutions act as a cache for already-built objects: once the slot of
  a stable binding is settled, a runtime container keeps the address of its
  object in the memo of the request, and a later resolution makes no call
- a request is a transaction of its container: when it fails, the container is
  left as it was before the request
- `shared_cyclical` supports runtime and static cycles through two-phase
  construction and should be treated as a constrained escape hatch rather than
  the default model

For a first pass through the project, read [Getting Started](getting-started.md)
and [Core Concepts](core-concepts.md) first.
