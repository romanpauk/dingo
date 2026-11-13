# Extending Dingo

Dingo is extended by traits, one for each kind of addition:

| Addition                                                    | Trait                                            |
| ----------------------------------------------------------- | ------------------------------------------------ |
| a wrapper or an alternative type                            | one `type_traits` specialization                 |
| a storage scope                                             | a tag type and one `scope_traits` specialization |
| a conversion between two types that no constructor performs | one `type_conversion_traits` specialization      |
| a type that is not constructed as its constructors show     | one `constructor_traits` specialization          |

The container and the resolution pipeline do not hard-code a closed list of
wrapper forms. The library supports standard smart pointers, arrays, variants,
and similar shapes through the same traits that external code uses.

## The Extension Points

### 1. `type_traits`

Specialize `type_traits` in
[include/dingo/type/type_traits.h](../../include/dingo/type/type_traits.h) for a
new wrapper or handle type.

A specialization derives from the semantics of the wrapper and provides its
operations. The standard wrappers are specialized the same way, so a user
wrapper takes part in conversions and storage as `std::unique_ptr` does:

```cpp
template <typename T>
struct type_traits<my_handle<T>> : semantics::shared_owner {
  using value_type = T;
  template <typename U> using rebind_t = my_handle<U>;

  static T *get(const my_handle<T> &handle) { return handle.get(); }
  static T &borrow(const my_handle<T> &handle) { return *handle; }
  static bool empty(const my_handle<T> &handle) { return !handle; }

  template <typename... Args> static my_handle<T> emplace(Args &&...args);
};
```

The semantics are:

- `semantics::observer`: a handle to an object that it does not own, such as a
  raw pointer
- `semantics::sole_owner`: the sole owner of an object, such as
  `std::unique_ptr`; it also declares `share_t<U>`, the sharing handle that can
  take over its object
- `semantics::shared_owner`: a handle that shares ownership, such as
  `std::shared_ptr`
- `semantics::optional_value`: a value that holds an object or nothing, such as
  `std::optional`

The semantics tell whether the wrapper is pointer-like and owns its element.
They also select the storage form, which decides the requests that each scope
publishes for the wrapper.

A wrapper declares `borrow` when its element can be borrowed as an object. An
owning handle without `borrow` holds an array. A default conversion does not
create an owning handle from a borrowed non-owning source. This rule applies to
the leaf handle conversion. Alternative and optional conversions first select or
unwrap their contained conversion. They then take its `borrow` or `consume`
requirement.

An optional wrapper provides `value_type`, `get`, `empty`, `wrap`, and
`empty_value`. Dingo uses them to keep an empty source empty and to wrap the
converted contained value.

An alternative type, such as `std::variant`, derives from
`semantics::alternative_value` and provides `alternatives`,
`selected_type<Source>`, `wrap<Selected>` and `get<Selected>`. Dingo applies the
same rule to the alternative that overload resolution selects.

A handle converts to the handle of a base of its element through its converting
constructor, as `std::shared_ptr` does. Such a wrapper needs no other trait.

`emplace(args...)` returns a wrapper of an element constructed from `args`. Two
optional members describe `emplace`:

- `static constexpr bool emplaces_element = true`: when the element is a wrapper
  too, `emplace` passes its arguments to the element's `emplace`, as the
  standard wrappers do. Dingo then emplaces a wrapper of such wrappers at once
  from the arguments of its innermost element.
- `template <typename... Args> static constexpr bool can_emplace`: whether
  `emplace` takes `Args`. Declare it when `emplace` cannot take every argument
  list. An example is a handle of an array, which initializes its elements from
  the arguments.

`owning_handles<>` lists the owning handle families that a scope makes of a
value, a pointer or an array that it makes for a request. A handle of
`handle_element` names each family. The default list is `std::unique_ptr` and
`std::shared_ptr`. A scope rebinds each family with `rebind_t` to what it makes,
and leaves out a handle that no `type_traits` describes. A listed user family
works the same way, also for a handle of an interface:

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
The built-in `unique`, `shared`, `external` and `shared_cyclical` scopes use the
same kind of specialization. The specialization declares:

| Member                                            | Meaning                                                                                                                                                                                                                                    |
| ------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `enabled`                                         | `true`                                                                                                                                                                                                                                     |
| `is_stable`                                       | references and pointers that the scope hands out stay valid                                                                                                                                                                                |
| `consumes`                                        | a request consumes a produced value instead of borrowing the stored instance                                                                                                                                                               |
| `guard`                                           | the kind of recursion guard                                                                                                                                                                                                                |
| `retains_unresolved_source`                       | an unresolved storage can keep source state across a resolution                                                                                                                                                                            |
| `exposed(storage_form)`                           | the request categories (value, lvalue reference, rvalue reference, pointer) that the scope publishes for each form of stored type; `storage_form::reference` is a stored reference, which only a scope that is given its instance can hold |
| `storage<Type, StoredType, Factory, Conversions>` | the class that keeps the instance; it provides `resolve`, and also `is_resolved` and `reset` if it keeps the instance between requests                                                                                                     |
| `requests<Form, Type, U>`                         | optional: the request lists of a form, when the scope publishes more or other requests than the exposed ones                                                                                                                               |
| `allows_cycles`                                   | optional: whether the bindings of the scope can be part of a dependency cycle                                                                                                                                                              |

Dingo derives from it the request lists of every stored type and the
materialization of the scope.
[test/registration/custom_scope.cpp](../../test/registration/custom_scope.cpp)
defines a scope outside the library and registers it with both containers.

### 3. `type_conversion_traits`

Specialize `type_conversion_traits` in
[include/dingo/conversion/paths.h](../../include/dingo/conversion/paths.h) when
two types need a conversion step that a converting constructor or a pointer cast
does not cover.

The `convert` function performs the last step: it builds the target wrapper from
the source wrapper. It is also the availability signal. A conversion is
available exactly when `convert(Source)` is well-formed and its result can
construct the target. Restrict the source categories with `convert` overloads or
SFINAE.

Every specialization also declares `required_access<Source>` as `borrow` or
`consume`:

- `borrow`: the conversion reads the source and does not transfer from it. This
  includes conversions that allocate an independent copy.
- `consume`: the conversion can move from the source or take the ownership that
  the source holds.

Route selection treats this declaration as it treats the requirements of the
built-in operations, so request discovery and execution describe one conversion.

An applicable specialization without `required_access<Source>` fails to compile;
Dingo does not fall back to another route. Default value construction from an
lvalue is a borrow only when it accepts a const view of the source. Direct
binding of a compatible lvalue reference is a borrow, because it aliases the
existing object and constructs no value.

### 4. `constructor_traits`

Specialize `constructor_traits` in
[include/dingo/factory/constructor_traits.h](../../include/dingo/factory/constructor_traits.h)
when a type is constructed in a way that its constructors do not show. The
specialization provides `construct(args...)` and `construct(ptr, args...)`. It
can also declare how constructor detection sees the type:

- `arguments`: the type list of its one signature. Detection uses it and does
  not search for a constructor.
- `initializable<Args...>`: whether the type is list-initialized from `Args`.
  Use it where probing `T{args...}` gives no answer.

`std::array` uses both. It is made from its elements, with the braces that its
nested array requires. MSVC does not elide these braces while probing.

## Extension Example

The main example is in
[test/type/type_traits.cpp](../../test/type/type_traits.cpp).

This test adds:

- `test_shared<T>`
- `test_unique<T>`
- `test_optional<T>`

Each has one `type_traits` specialization with its semantics and operations. The
handles convert to the handles of a base through their converting constructors.
Their definitions are in
[test/support/custom_wrappers.h](../../test/support/custom_wrappers.h), which is
executable documentation.

## Interface Storage Rebinding

Registration rebinds a storage handle from a concrete leaf type to an interface
leaf type when the handle of the interface can be constructed from it, or a
`type_conversion_traits` specialization converts it; see
[include/dingo/storage/interface_storage_traits.h](../../include/dingo/storage/interface_storage_traits.h).
This is not a trait to specialize. A wrapper takes part through its converting
constructor.

## Good Source Companions

- [include/dingo/type/type_traits.h](../../include/dingo/type/type_traits.h)
- [include/dingo/storage/scope_traits.h](../../include/dingo/storage/scope_traits.h)
- [include/dingo/conversion/paths.h](../../include/dingo/conversion/paths.h)
- [test/support/custom_wrappers.h](../../test/support/custom_wrappers.h)
- [test/registration/custom_scope.cpp](../../test/registration/custom_scope.cpp)
