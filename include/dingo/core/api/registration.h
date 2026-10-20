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

  template <typename... Args> struct with_arguments {
    using factory = declared_factory;
  };

  template <typename Arg> struct with_arguments<Arg> {
    using factory =
        std::conditional_t<is_callable_factory<std::decay_t<Arg>>::value,
                           std::decay_t<Arg>, declared_factory>;
  };

  template <typename... Args>
  using binding_for =
      binding_type<scope_of_v<typename scope_part::type>,
                   typename storage_part::type, typename interfaces_part::type,
                   typename with_arguments<Args...>::factory>;

  using binding = binding_for<>;
};

template <typename Bind> struct bind_registration;

template <typename... Parts> struct bind_registration<::dingo::bind<Parts...>> {
  using type = registration<Parts...>;
};

} // namespace dingo::core
