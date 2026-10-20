//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The registration vocabulary of the public API and its reduction to a binding
// type of the next implementation.

#include <dingo/core/base/mechanics.h>
#include <dingo/core/engine/execute.h>
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

} // namespace dingo

namespace dingo::core {

// The factory of the next implementation that a public factory declares.
template <typename Factory> struct factory_of;

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
      is_callable_factory<std::decay_t<First>>::value,
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
  static_assert(!std::is_same_v<scope_part, detail::missing_part>,
                "a registration needs a scope<>");
  static_assert(!std::is_same_v<storage_part, detail::missing_part>,
                "a registration needs a storage<>");

  using factory_part =
      typename detail::find_part<::dingo::factory, detail::missing_part,
                                 Parts...>::type;
  using declared_factory = typename std::conditional_t<
      std::is_same_v<factory_part, detail::missing_part>,
      std::enable_if<true, detected_factory>,
      factory_of<typename factory_part::type>>::type;

  using key_part = typename detail::find_key_part<Parts...>::type;

  // The factory and the key a registration takes from its arguments: a
  // callable factory, and a key value, which a key part must not also name.
  template <typename... Args> struct with_arguments {
    using factory =
        typename detail::find_callable_factory<declared_factory, Args...>::type;
    using runtime = typename detail::find_runtime_key<void, Args...>::type;
    static_assert(std::is_void_v<runtime> || std::is_same_v<key_part, no_key>,
                  "a registration takes its key from a key_type<> or a "
                  "key_value, not both");
    using key = std::conditional_t<std::is_void_v<runtime>, key_part, runtime>;
  };

  template <typename... Args>
  using binding_for =
      binding_type<scope_of_v<typename scope_part::type>,
                   typename storage_part::type, typename interfaces_part::type,
                   typename with_arguments<Args...>::factory,
                   typename with_arguments<Args...>::key>;

  using binding = binding_for<>;
};

template <typename Bind> struct bind_registration;

template <typename... Parts> struct bind_registration<::dingo::bind<Parts...>> {
  using type = registration<Parts...>;
};

} // namespace dingo::core
