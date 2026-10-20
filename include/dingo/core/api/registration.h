//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The registration vocabulary of the public API and its reduction to a binding
// type of the next implementation.

#include <dingo/core/api/collection.h>
#include <dingo/core/base/mechanics.h>
#include <dingo/core/engine/execute.h>
#include <dingo/core/vocabulary/compile_diagnostics.gen.h>
#include <dingo/core/vocabulary/scope_tags.gen.h> // IWYU pragma: export

#include <type_traits>

namespace dingo {

template <typename Tag> struct scope {
  using type = Tag;
};

template <typename Type> struct storage {
  using type = Type;
};

template <typename... Interfaces> struct interfaces {
  using type = core::type_list<Interfaces...>;
};

// The factory a registration declares: a constructor, a function, or a
// callable object passed at registration.
template <typename Factory> struct factory {
  using type = Factory;
};

template <typename... Parts> struct bind {};

template <typename... Binds> struct bindings {};

// The dependencies of a binding: the argument types of its constructor.
template <typename... Types> struct dependencies {};

} // namespace dingo

namespace dingo::core {

namespace detail {
template <template <typename...> typename Part, typename Type>
struct is_part : std::false_type {};

template <template <typename...> typename Part, typename... Arguments>
struct is_part<Part, Part<Arguments...>> : std::true_type {};

template <template <typename...> typename Part, typename Default,
          typename... Parts>
struct find_part {
  using type = Default;
};

template <template <typename...> typename Part, typename Default,
          typename First, typename... Rest>
struct find_part<Part, Default, First, Rest...> {
  using type =
      std::conditional_t<is_part<Part, First>::value, First,
                         typename find_part<Part, Default, Rest...>::type>;
};

struct missing_part {
  using type = void;
};

template <typename... Parts> struct find_key_part {
  using type = no_key;
};

template <typename First, typename... Rest>
struct find_key_part<First, Rest...> {
  using type =
      typename std::conditional_t<is_key_type<First>::value, key_of<First>,
                                  find_key_part<Rest...>>::type;
};

template <typename Default, typename... Args> struct find_callable_factory {
  using type = Default;
};

template <typename Default, typename First, typename... Rest>
struct find_callable_factory<Default, First, Rest...> {
  using type = typename std::conditional_t<
      is_callable_factory<std::decay_t<First>>::value ||
          is_collection_factory<std::decay_t<First>>::value,
      std::enable_if<true, std::decay_t<First>>,
      find_callable_factory<Default, Rest...>>::type;
};

template <typename Default, typename... Args> struct find_runtime_key {
  using type = Default;
};

template <typename Default, typename First, typename... Rest>
struct find_runtime_key<Default, First, Rest...> {
  using type =
      typename std::conditional_t<is_key_value<std::decay_t<First>>::value,
                                  runtime_key_of<std::decay_t<First>>,
                                  find_runtime_key<Default, Rest...>>::type;
};

// The runtime keys of the key values of a registration of types other than
// `Primary`, each once: the keys of other domains its binding is also found
// by.
template <typename Primary, typename Found, typename... Args>
struct find_key_aliases {
  using type = Found;
};

template <typename Primary, typename... Found, typename First, typename... Rest>
struct find_key_aliases<Primary, type_list<Found...>, First, Rest...> {
  template <typename Key>
  static constexpr bool adds =
      !std::is_same_v<Key, Primary> && (!std::is_same_v<Key, Found> && ...);
  using key = typename runtime_key_of<std::decay_t<First>>::type;
  using type = typename std::conditional_t<
      is_key_value<std::decay_t<First>>::value && adds<key>,
      find_key_aliases<Primary, type_list<Found..., key>, Rest...>,
      find_key_aliases<Primary, type_list<Found...>, Rest...>>::type;
};
} // namespace detail

template <typename... Parts> struct registration;

template <typename Bind> struct bind_registration;

namespace detail {
template <typename Bindings> struct local_bindings_of;

template <typename... Binds>
struct local_bindings_of<::dingo::bindings<Binds...>> {
  using type = type_list<typename bind_registration<Binds>::type::binding...>;
};

template <typename Dependencies> struct declared_dependencies {
  using type = void;
};

template <typename... Types>
struct declared_dependencies<::dingo::dependencies<Types...>> {
  using type = constructor_factory<declared_leaf<Types...>>;
};
} // namespace detail

template <typename... Parts> struct bind_registration<::dingo::bind<Parts...>> {
  using type = registration<Parts...>;
};

namespace detail {
// The interfaces a registration declares, without the stored type: declaring
// only the stored type, its references and qualifiers removed, names the
// binding by its own type, as declaring no interface does.
template <typename Stored, typename Interfaces> struct declared_interfaces {
  using type = Interfaces;
};

template <typename Stored, typename Interface>
struct declared_interfaces<Stored, type_list<Interface>> {
  using type =
      std::conditional_t<std::is_same_v<remove_cvref_t<Stored>, Interface>,
                         type_list<>, type_list<Interface>>;
};
} // namespace detail

// The binding type a registration declares. A registration whose only
// argument is a callable factory makes its object by it.
template <typename... Parts> struct registration {
  using scope_part =
      typename detail::find_part<::dingo::scope, detail::missing_part,
                                 Parts...>::type;
  using storage_part =
      typename detail::find_part<::dingo::storage, detail::missing_part,
                                 Parts...>::type;
  using interfaces_part =
      typename detail::find_part<::dingo::interfaces, ::dingo::interfaces<>,
                                 Parts...>::type;
  static_assert(core::require<!std::is_same_v<scope_part, detail::missing_part>,
                              core::diag::registration_without_scope>());
  static_assert(
      core::require<!std::is_same_v<storage_part, detail::missing_part>,
                    core::diag::registration_without_storage>());

  using factory_part =
      typename detail::find_part<::dingo::factory, detail::missing_part,
                                 Parts...>::type;
  using dependencies_part =
      typename detail::find_part<::dingo::dependencies, detail::missing_part,
                                 Parts...>::type;
  // The factory a factory<> part declares, or the constructor of the types a
  // dependencies<> part declares, or the detected constructor.
  using declared_factory = typename std::conditional_t<
      std::is_same_v<factory_part, detail::missing_part>,
      std::conditional_t<
          std::is_same_v<dependencies_part, detail::missing_part>,
          std::enable_if<true, detected_factory>,
          detail::declared_dependencies<dependencies_part>>,
      factory_of<typename factory_part::type>>::type;
  using locals_part =
      typename detail::find_part<::dingo::bindings, ::dingo::bindings<>,
                                 Parts...>::type;

  using key_part = typename detail::find_key_part<Parts...>::type;

  // The factory and the key a registration takes from its arguments: a
  // callable factory, and a key value, which a key part must not also name.
  template <typename... Args> struct with_arguments {
    using factory =
        typename detail::find_callable_factory<declared_factory, Args...>::type;
    using runtime = typename detail::find_runtime_key<void, Args...>::type;
    static_assert(
        core::require<
            std::is_void_v<runtime> || std::is_same_v<key_part, no_key>,
            core::diag::registration_key_twice, typename storage_part::type>());
    using key = std::conditional_t<std::is_void_v<runtime>, key_part, runtime>;
    using aliases =
        typename detail::find_key_aliases<key, type_list<>, Args...>::type;
  };

  template <typename... Args>
  using binding_for = binding_type<
      typename scope_part::type, typename storage_part::type,
      typename detail::declared_interfaces<
          typename storage_part::type, typename interfaces_part::type>::type,
      typename with_arguments<Args...>::factory,
      typename with_arguments<Args...>::key,
      typename detail::local_bindings_of<locals_part>::type,
      typename with_arguments<Args...>::aliases>;

  using binding = binding_for<>;
};

} // namespace dingo::core
