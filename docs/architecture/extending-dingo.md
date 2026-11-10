# Extending Dingo

Dingo is extended by traits, one per thing being added:

- a wrapper or an alternative type: one `type_traits` specialization
- a storage scope: a tag type and one `scope_traits` specialization
- a conversion between two types that no constructor performs: one
  `type_conversion_traits` specialization
- a type constructed otherwise than its constructors tell: one
  `constructor_traits` specialization

Dingo's container and resolution pipeline do not hard-code a closed list of
supported wrapper forms. Library support for standard smart pointers, arrays,
variants, and similar shapes is implemented through the same traits and
conversion hooks available to external code. Adding support for another wrapper
or storage shape follows the same path as Dingo's built-in support.

## The Extension Points

### 1. `type_traits`

Specialize `type_traits` in
[include/dingo/type/type_traits.h](../../include/dingo/type/type_traits.h) when
introducing a new wrapper or handle type.

A specialization derives from the semantics of the wrapper and provides its
operations. The standard wrappers are specialized the same way, so a user
wrapper takes part in conversions and storage exactly as `std::unique_ptr` or
`std::optional` does:

```cpp
template <typename T>
struct type_traits<my_handle<T>> : semantics::shared_owner {
  using value_type = T;
  template <typename U> using rebind_t = my_handle<U>;

  static T *get(const my_handle<T> &handle) { return handle.get(); }
  static T &borrow(const my_handle<T> &handle) { return *handle; }
  static bool empty(const my_handle<T> &handle) { return !handle; }

  template <typename... Args> static my_handle<T> make(Args &&...args);
};
```

The semantics are:

- `semantics::observer`: a handle to an object it does not own, such as a raw
  pointer
- `semantics::sole_owner`: the sole owner of an object, such as
  `std::unique_ptr`; it also declares `share_t<U>`, the sharing handle its
  object can be handed to
- `semantics::shared_owner`: a handle sharing ownership, such as
  `std::shared_ptr`
- `semantics::optional_value`: a value that holds an object or none, such as
  `std::optional`

The semantics say whether the wrapper is pointer-like and whether it owns its
element, and they select the storage form, which decides the requests each scope
publishes for the wrapper. A wrapper whose element can be borrowed as an object
declares `borrow`; an owning handle without it holds an array. Dingo does not
create an owning handle from a borrowed non-owning source through a default
conversion. This rule applies to the leaf handle conversion. Alternative and
optional-style conversions first select or unwrap their contained conversion,
then inherit its `borrow` or `consume` requirement.

An optional-style wrapper provides `value_type`, `get`, `empty`, `wrap`, and
`make_empty`. These operations let Dingo preserve an empty source and construct
the target wrapper around the converted contained value.

An alternative type, such as `std::variant`, derives from
`semantics::alternative_value` and provides `alternatives`,
`selected_type<Source>`, `wrap<Selected>` and `get<Selected>`, so Dingo can
apply the same rule to the alternative selected by overload resolution.

A handle converts to the handle of a base of its element by its converting
constructor, as `std::shared_ptr` does; such a wrapper needs no other trait.

A `make` that makes its element of what makes the element's own element, as
those of the standard wrappers do, declares
`static constexpr bool makes_nested = true`, so that a wrapper of such wrappers
is made at once of what makes its innermost element. A `make` that cannot be
called with every argument list, as that of a handle of an array, whose elements
are initialized of the arguments, declares
`template <typename... Args> static constexpr bool makeable`, whether it makes
the wrapper of `Args`.

The owning handles a scope makes of a value, a pointer or an array it makes for
a request are the families `owning_handles<>` lists, each named by a handle of
`handle_element`; the standard `std::unique_ptr` and `std::shared_ptr` are
listed there. A scope rebinds each with `rebind_t` to what it makes, and leaves
out a handle no `type_traits` describes. A handle family of a user's is made the
same way, including a handle of an interface, once it is listed:

```cpp
template <> struct dingo::owning_handles<> {
  using type = dingo::type_list<std::unique_ptr<dingo::handle_element>,
                                std::shared_ptr<dingo::handle_element>,
                                my_handle<dingo::handle_element>>;
};
```

### 2. `scope_traits`

A storage scope is a tag type, used as `scope<Tag>`, and one specialization of
`scope_traits<Tag>` in
[include/dingo/storage/scope_traits.h](../../include/dingo/storage/scope_traits.h).
The built-in `unique`, `shared`, `external` and `shared_cyclical` scopes are
specialized the same way. The specialization declares:

- `enabled`, `is_stable`, `consumes`, `guard` and `retains_unresolved_source`:
  whether references stay valid, whether a request consumes the instance, the
  recursion guard, and whether unresolved storage keeps source state
- `exposed(storage_form)`: which request categories (value, lvalue reference,
  rvalue reference, pointer) the scope publishes for each form of stored type;
  `storage_form::reference` is a stored reference, which only a scope given its
  instance can hold
- `storage<Type, StoredType, Factory, Conversions>`: the class that keeps the
  instance; it provides `resolve`, and `is_resolved` and `reset` when it keeps
  the instance between requests
- optionally `requests<Form, Type, U>`: the request lists of a form, when the
  scope publishes more or other requests than the exposed ones
- optionally `allows_cycles`: whether its bindings may take part in a dependency
  cycle

From it Dingo derives the request lists of every stored type and the
materialization of the scope.
[test/registration/custom_scope.cpp](../../test/registration/custom_scope.cpp)
defines a scope outside the library and registers it with both containers.

### 3. `type_conversion_traits`

Specialize `type_conversion_traits` in
[include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)
when two types need a concrete conversion step that is not covered by a direct
converting constructor or pointer cast.

The `convert` function performs the last-mile "build target wrapper from source
wrapper" step and is the availability signal. A conversion is available exactly
when `convert(Source)` is well-formed and its result can construct the target.
Restrict source categories through `convert` overloads or SFINAE.

Every specialization also declares `required_access<Source>` as `borrow` or
`consume`. Use `borrow` when conversion may read the source without transferring
from it, including conversions that allocate an independent copy. Use `consume`
when conversion can move from the source or acquire ownership represented by it.
Route selection applies this declaration in the same way as requirements derived
for built-in conversion operations, so request discovery and execution describe
one conversion.

An applicable specialization that omits `required_access<Source>` is rejected
instead of falling through to another conversion route. Default value
construction from an lvalue is borrowable only when it accepts a const view of
the source. Direct compatible lvalue-reference binding remains borrowable
because it aliases the existing object rather than constructing a value.

### 4. `constructor_traits`

Specialize `constructor_traits` in
[include/dingo/factory/constructor_traits.h](../../include/dingo/factory/constructor_traits.h)
when a type is constructed in a way its constructors do not show. It provides
`construct(args...)` and `construct(ptr, args...)`, and may declare how
constructor detection sees the type: `arguments`, the type list of its one
signature, which detection takes instead of searching for a constructor, and
`initializable<Args...>`, whether it is list-initialized of `Args`, where
probing `T{args...}` would not tell. `std::array` is described so: it is made of
its elements with the braces its nested array requires, which MSVC does not
elide while probing.

## Extension Example

The best example is already in
[test/type/type_traits.cpp](../../test/type/type_traits.cpp).

That test adds:

- `test_shared<T>`
- `test_unique<T>`
- `test_optional<T>`

each described by one `type_traits` specialization with its semantics and
operations; the handles convert to the handles of a base by their converting
constructors.

Their definitions are in
[test/support/custom_wrappers.h](../../test/support/custom_wrappers.h).

That file is worth treating as executable documentation.

## Interface Storage Rebinding

Registration rebinds a storage handle from a concrete leaf type to an interface
leaf type when the handle of the interface can be constructed from it, or a
`type_conversion_traits` specialization converts it; see
[include/dingo/storage/interface_storage_traits.h](../../include/dingo/storage/interface_storage_traits.h).
It is not a trait to specialize: a wrapper takes part through its converting
constructor.

## Good Source Companions

- [include/dingo/type/type_traits.h](../../include/dingo/type/type_traits.h)
- [include/dingo/storage/scope_traits.h](../../include/dingo/storage/scope_traits.h)
- [include/dingo/type/type_conversion_traits.h](../../include/dingo/type/type_conversion_traits.h)
- [test/support/custom_wrappers.h](../../test/support/custom_wrappers.h)
- [test/registration/custom_scope.cpp](../../test/registration/custom_scope.cpp)
