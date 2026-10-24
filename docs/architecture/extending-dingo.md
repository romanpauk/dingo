# Extending Dingo

Dingo's extensibility is trait-driven. The key is to specialize the trait that
matches the problem actually being solved.

Dingo's container and resolution pipeline do not hard-code a closed list of
supported wrapper forms, layer kinds, scopes, or lookup indexes. The standard
smart pointers, `std::optional`, and `std::variant` are layers through the same
`wrapper_traits` a user wrapper specializes, the layer kinds of the library are
declared by the same `layer_kind_traits` a user kind is, the built-in scopes are
specializations of `scope_traits`, and the lookup backends of the library name
their indexes as a user backend does. Adding another wrapper, kind, scope, or
index follows the same path as Dingo's built-in support.

## Wrappers

Specialize `dingo::wrapper_traits` in
[include/dingo/type/type_traits.h](../../include/dingo/type/type_traits.h) to
make a wrapper or handle type a layer of the shape algebra. A specialization
declares:

- `kind`: the layer kind of the wrapper, such as `unique_kind`, `shared_kind`,
  `optional_kind`, `alternative_kind`, or a kind of the user
- `family`: a type naming the family of the wrapper; a route converts a wrapper
  only to another wrapper of its family, such as a sole-owner handle to a
  sharing handle
- `arguments`: the element types, as a `dingo::type_list`
- `rebind`: the wrapper of other element types
- `element`: the element of a wrapper object; for a variadic kind,
  `element<Index>`, the alternative `Index` of it
- `make`: a wrapper object made from the arguments of its element

and, as the forms of its kind need them:

- `adopt`: for a kind of the adopt form, a wrapper object that takes the
  ownership of an element allocated by `new`; a sharing wrapper that
  `shared_cyclical` publishes before its element exists also adopts an address
  with the deleter that frees it
- `share`: for a kind of the share form, the wrapper of the kind the form leads
  to, of the same family, which takes the element of a consumed wrapper
- `index`: for a variadic kind, the alternative a wrapper object holds
- `has_element`: for a nullable kind that holds its element in place, whether a
  wrapper object holds one

A wrapper converts to the wrapper of a base of its element, the route to a
declared interface, by its converting constructor, or by a
`type_conversion_traits` specialization. A sharing handle of the user:

```c++
#include <dingo/type/type_traits.h>

#include <memory>
#include <type_traits>
#include <utility>

// A sharing handle of the user.
template <typename T> class handle {
public:
  handle() = default;
  explicit handle(std::shared_ptr<T> pointer) : pointer_(std::move(pointer)) {}

  // Converts to the handle of a base, the route to a declared interface.
  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  handle(const handle<U> &other) : pointer_(other.pointer_) {}

  T *get() const { return pointer_.get(); }

private:
  template <typename> friend class handle;
  std::shared_ptr<T> pointer_;
};

struct handle_family {};

template <typename T> struct dingo::wrapper_traits<handle<T>> {
  using kind = dingo::shared_kind;
  using family = handle_family;
  using arguments = dingo::type_list<T>;
  template <typename U> using rebind = handle<U>;

  static T &element(const handle<T> &wrapper) { return *wrapper.get(); }

  template <typename... Args> static handle<T> make(Args &&...args) {
    return handle<T>(std::make_shared<T>(std::forward<Args>(args)...));
  }

  static handle<T> adopt(T *element) {
    return handle<T>(std::shared_ptr<T>(element));
  }

  // Used by shared_cyclical, which publishes the handle before its element
  // is constructed.
  template <typename Deleter>
  static handle<T> adopt(T *element, Deleter deleter) {
    return handle<T>(std::shared_ptr<T>(element, std::move(deleter)));
  }
};
```

The handle serves the element, the handle itself, and the handle of a declared
interface, as `std::shared_ptr` does:

```c++
struct service_interface {
  virtual ~service_interface() = default;
};
struct service : service_interface {};

void example() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>,
                          dingo::storage<handle<service>>,
                          dingo::interfaces<service_interface>>();

  service_interface &object = container.resolve<service_interface &>();
  handle<service_interface> copy =
      container.resolve<handle<service_interface>>();
  assert(copy.get() == &object);
}
```

[test/support/custom_wrappers.h](../../test/support/custom_wrappers.h) declares
a sharing, a sole-owner, and an optional test wrapper this way, which the
generated matrix uses.

## Layer Kinds

Specialize `dingo::layer_kind_traits` for a tag type to declare a layer kind,
when no kind of the library describes a wrapper. A specialization declares the
`layer_property` bits of the kind and its forms, each a
`kind_form<route_form, Target>`, where `Target` is the kind a form that leads to
another kind leads to:

- the properties: `owning`, `sharing`, `nullable`, `borrowable`, `in_place`,
  `unsized`, `constructible`, `copyable`, `immovable`, `wrapper`, `variadic`,
  and `extent`
- the forms of a kind: `borrow`, `address`, `decay`, `take`, `wrap`, `share`,
  `adopt`, `release`, `release_whole`, `upcast_copied`, `upcast_consumed`, and
  `retain`; their meanings are listed in the
  [Conversion Model](conversion-model.md#terms-and-route-forms)

The kinds of the library, `unique_kind`, `shared_kind`, `optional_kind`,
`alternative_kind`, `array_kind`, `unsized_array_kind`, `pointer_kind`, and
`reference_kind`, are declared so in
[conversion/layer_kinds.gen.h](../../include/dingo/conversion/layer_kinds.gen.h).
A wrapper of a kind behaves as its properties and forms say, not as its C++ type
does. A sharing handle without the retain form:

```c++
#include <dingo/type/type_traits.h>

// A sharing handle that a binding hands out only as copies: it has no retain
// form, so no request refers to a handle of an interface the binding keeps.
struct pinned_kind {};

template <> struct dingo::layer_kind_traits<pinned_kind> {
  static constexpr std::uint16_t properties =
      dingo::layer_property::owning | dingo::layer_property::sharing |
      dingo::layer_property::nullable | dingo::layer_property::borrowable |
      dingo::layer_property::constructible | dingo::layer_property::copyable |
      dingo::layer_property::wrapper;
  using forms = dingo::type_list<
      dingo::kind_form<dingo::route_form::borrow>,
      dingo::kind_form<dingo::route_form::adopt, dingo::pointer_kind>,
      dingo::kind_form<dingo::route_form::upcast_copied>>;
};

template <typename T> struct dingo::wrapper_traits<pinned_handle<T>> {
  using kind = pinned_kind;
  using family = pinned_family;
  using arguments = dingo::type_list<T>;
  template <typename U> using rebind = pinned_handle<U>;

  static T &element(const pinned_handle<T> &wrapper) { return *wrapper.get(); }

  template <typename... Args> static pinned_handle<T> make(Args &&...args) {
    return pinned_handle<T>(std::make_shared<T>(std::forward<Args>(args)...));
  }

  static pinned_handle<T> adopt(T *element) {
    return pinned_handle<T>(std::shared_ptr<T>(element));
  }
};
```

A request of `pinned_handle<Interface>` by value is served by `upcast_copied`,
while a request of a reference to it is refused, as no form keeps a handle of an
interface: a runtime container throws `type_not_convertible_exception`. The
declaration is checked when it is used: a form a kind cannot have, a form named
twice, a form that leads to another kind without naming it, and a form that
needs a property the kind lacks are each rejected with a diagnostic.

The route probes declare kinds of the user in
[test/routes/user_kinds.py](../../test/routes/user_kinds.py), over the wrappers
of
[test/routes/templates/kinded_wrappers_next.h](../../test/routes/templates/kinded_wrappers_next.h).

## Conversions

Specialize `dingo::type_conversion_traits` in
[include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)
when a wrapper converts to the wrapper of an interface, or a leaf to an
interface, by something other than a converting constructor.

A specialization provides a static `convert`, which takes the source and returns
the target or an object the target is constructed from, and declares
`required_access<From>` as `dingo::borrow` or `dingo::consume` for the reference
`From` to the source. Use `borrow` when the conversion reads the source without
transferring from it, including conversions that allocate an independent copy,
and `consume` when it moves from the source or acquires the ownership it
represents. A borrowed source converts only by a conversion that borrows. A
specialization without `required_access` is rejected with the diagnostic
`conversion_access_undeclared`.

```c++
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_traits.h>

// A handle without a converting constructor.
template <typename T> class plain_handle {
public:
  plain_handle() = default;
  explicit plain_handle(std::shared_ptr<T> pointer)
      : pointer_(std::move(pointer)) {}

  T *get() const { return pointer_.get(); }
  const std::shared_ptr<T> &native() const { return pointer_; }

private:
  std::shared_ptr<T> pointer_;
};

// Converts the handle of an implementation to the handle of its interface,
// borrowing the source: it copies the shared ownership.
template <typename Target, typename Source>
struct dingo::type_conversion_traits<
    plain_handle<Target>, plain_handle<Source>,
    std::enable_if_t<std::is_convertible_v<Source *, Target *> &&
                     !std::is_same_v<Source, Target>>> {
  template <typename From> using required_access = dingo::borrow;

  static plain_handle<Target> convert(const plain_handle<Source> &source) {
    return plain_handle<Target>(source.native());
  }
};
```

A value request copies a leaf by its copy constructor; a conversion of a leaf to
its own type is not a route.

## Scopes

Specialize `dingo::scope_traits` for a tag type to make it a scope. A
specialization holds its `dingo::scope_descriptor`, the fields of the Scope
schema of the spec: the slot kind, the materialized value category, stability,
whether a request consumes the object, the recursion guard, and the request
categories it exposes for each storage form. A user scope may copy the
descriptor of a built-in scope:

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

A scope may also name a slot class of its own, `slot<Binding>`, derived from
`dingo::scope_slot<Binding>`, which keeps the factory of the binding and
constructs the stored object with `construct(where, resolver)`. The slot
provides `get`, the object a request walks from, and `settled`, whether that
object lives as long as the container, so its address may be kept. It keeps the
contract of the slot kind of its descriptor:

```c++
#include <new>

// The storage of the one object of a stored type that a process keeps.
template <typename Type> struct process_object {
  alignas(Type) static inline unsigned char storage[sizeof(Type)];
  static inline Type *object = nullptr;
};

// A slot that keeps one object of a stored type for the whole process, which
// the bindings of it in every container share.
template <typename Binding>
class process_slot : public dingo::scope_slot<Binding> {
public:
  using stored_type = typename Binding::stored_type;
  using dingo::scope_slot<Binding>::scope_slot;

  template <typename Resolver> stored_type &get(Resolver &resolver) {
    using kept = process_object<stored_type>;
    if (kept::object == nullptr) {
      this->construct(kept::storage, resolver);
      kept::object =
          std::launder(reinterpret_cast<stored_type *>(kept::storage));
    }
    return *kept::object;
  }

  // The object lives as long as the process, so its address may be kept.
  template <typename Resolver> bool settled(Resolver &) const { return true; }
};

struct per_process {};

template <> struct dingo::scope_traits<per_process> {
  static constexpr dingo::scope_descriptor descriptor =
      dingo::scope_traits<dingo::shared>::descriptor;
  template <typename Binding> using slot = process_slot<Binding>;
};
```

The routes of a binding are derived from its descriptor as for a built-in scope,
and the cycle rule allows a cycle through the scopes with a two-phase slot. A
static container with a slot class of the user keeps a transaction log that
grows. [test/next/containers/scopes.cpp](../../test/next/containers/scopes.cpp)
tests both scopes in every container.

## Lookup Indexes

A lookup backend, the last argument of
`associative<Key, Interface, Cardinality, Backend>` or of
`base<Cardinality, Backend>` in
[include/dingo/lookup/lookup.h](../../include/dingo/lookup/lookup.h), is a type
that names a member template `index<Value, Candidate>`, an index as
[backend/index.h](../../include/dingo/backend/index.h) describes it. An index is
default constructible and assignable and has:

```c++
void build(const dingo::index_item<Value, Candidate> *items, std::size_t count);
template <typename Visit> void visit(const Value &key, Visit &&visit) const;
```

`build` takes the candidates of a request, each a `dingo::index_item` with its
`key` and `candidate`, in the order of registration; they live until the next
`build`. `visit` calls `visit(candidate)` for each candidate of `key` in that
order until it returns `false`. An index must visit the candidates a linear scan
visits, in the same order, so it never changes which bindings a request names. A
backend may also name `bound`, the size of a dense array of its key values.

```c++
#include <dingo/container.h>
#include <dingo/lookup/lookup.h>

#include <cstddef>

// A lookup backend whose index scans the candidates of a request in the order
// of registration.
struct scan {
  template <typename Value, typename Candidate> class index {
  public:
    void build(const dingo::index_item<Value, Candidate> *items,
               std::size_t count) {
      items_ = items;
      count_ = count;
    }

    template <typename Visit> void visit(const Value &key, Visit &&visit) const {
      for (std::size_t at = 0; at < count_; ++at) {
        if (items_[at].key == key && !visit(items_[at].candidate)) {
          return;
        }
      }
    }

  private:
    const dingo::index_item<Value, Candidate> *items_ = nullptr;
    std::size_t count_ = 0;
  };
};
```

The backends of the library, `ordered`, `unordered`, and `array<N>`, name the
indexes of [lookup/indexes.h](../../include/dingo/lookup/indexes.h): the sorted
key values, a hash table, and a dense array, each of which falls back to an
index the key type can serve. A backend that names no index is rejected with the
diagnostic `lookup_backend_without_index`. See
[Lookup Resolution](../advanced-topics.md#lookup-resolution) for the
declarations that take a backend.

## Other Extension Points

- `dingo::collection_traits` in
  [include/dingo/type/collection_traits.h](../../include/dingo/type/collection_traits.h)
  makes a type a collection: `is_collection`, `resolve_type`, the element type,
  `add`, and optionally `reserve`.
- `dingo::is_auto_constructible` in
  [include/dingo/type/auto_constructible.h](../../include/dingo/type/auto_constructible.h)
  opts an unregistered type into construction when no binding names it.
- `DINGO_CONSTRUCTOR` in
  [include/dingo/registration/constructor.h](../../include/dingo/registration/constructor.h)
  declares the constructor of a user type.
- Container traits select the RTTI, the allocator, and the lookup declarations
  of a container, as described in
  [Advanced Topics](../advanced-topics.md#custom-rtti).
- `dingo::rtti<Provider>` is specialized for a provider of type information of
  the user, as `typeid_provider` and `static_provider` are in
  [include/dingo/rtti/typeid_provider.h](../../include/dingo/rtti/typeid_provider.h)
  and
  [include/dingo/rtti/static_provider.h](../../include/dingo/rtti/static_provider.h).

## Good Source Companions

- [include/dingo/conversion/layer_traits.h](../../include/dingo/conversion/layer_traits.h)
- [include/dingo/conversion/layer_kinds.gen.h](../../include/dingo/conversion/layer_kinds.gen.h)
- [include/dingo/conversion/type_conversion.h](../../include/dingo/conversion/type_conversion.h)
- [include/dingo/conversion/scopes.gen.h](../../include/dingo/conversion/scopes.gen.h)
- [include/dingo/backend/slot.h](../../include/dingo/backend/slot.h)
- [include/dingo/backend/index.h](../../include/dingo/backend/index.h)
- [include/dingo/lookup/indexes.h](../../include/dingo/lookup/indexes.h)
- [test/support/custom_wrappers.h](../../test/support/custom_wrappers.h)
- [test/next/containers/scopes.cpp](../../test/next/containers/scopes.cpp)
- [test/next/containers/lookups.cpp](../../test/next/containers/lookups.cpp)
