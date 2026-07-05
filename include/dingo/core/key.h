//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>
#include <dingo/core/none.h>
#include <dingo/core/selected.h>
#include <dingo/type/type_traits.h>

#include <type_traits>
#include <utility>

namespace dingo {
template <typename T, auto... Values> struct key_type;
template <typename T, typename Interface = void> struct key_value;

namespace detail {
template <typename T, auto... Values> struct fixed_key_value {
  static_assert(sizeof...(Values) <= 1,
                "dingo::key_type<T, V> accepts at most one value");
};

template <typename T, auto Value>
struct is_fixed_key_value_constructible
    : std::bool_constant<std::is_same_v<std::remove_cv_t<decltype(Value)>, T> ||
                         is_brace_constructible_v<T, Value>> {};

template <typename T, auto Value> constexpr T make_fixed_key_value() {
  if constexpr (std::is_same_v<std::remove_cv_t<decltype(Value)>, T>) {
    return Value;
  } else {
    return T{Value};
  }
}

template <typename T, auto Value> struct fixed_key_value<T, Value> {
  static_assert(is_fixed_key_value_constructible<T, Value>::value,
                "dingo::key_type<T, V> requires V to be usable as T{V}");

  static constexpr T make() { return make_fixed_key_value<T, Value>(); }
};

template <typename T, auto... Values>
struct is_fixed_key_value_usable : std::false_type {};

template <typename T> struct is_fixed_key_value_usable<T> : std::true_type {};

template <typename T, auto Value>
struct is_fixed_key_value_usable<T, Value>
    : is_fixed_key_value_constructible<T, Value> {};

template <typename T, auto... Values>
inline constexpr bool is_fixed_key_value_usable_v =
    is_fixed_key_value_usable<T, Values...>::value;
} // namespace detail

template <typename T, auto... Values> struct key_type {
  static_assert(sizeof...(Values) <= 1,
                "dingo::key_type<T, V> accepts at most one value");
  static_assert(sizeof...(Values) > 1 ||
                    detail::is_fixed_key_value_usable_v<T, Values...>,
                "dingo::key_type<T, V> requires V to be usable as T{V}");

  using type = T;
  template <typename U> using rebind_t = key_type<U>;

  static constexpr bool has_value = sizeof...(Values) == 1;
};

template <typename T, typename Interface> struct key_value {
  using type = T;
  using interface_type = Interface;

  template <typename U> using rebind_t = key_value<U, Interface>;

  template <typename U,
            typename = std::enable_if_t<std::is_constructible_v<T, U &&>>>
  explicit key_value(U &&runtime_value)
      : runtime_value_(std::forward<U>(runtime_value)) {}

  const T &value() const { return runtime_value_; }

private:
  T runtime_value_;
};

template <typename T> key_value(T &&) -> key_value<std::decay_t<T>>;

namespace detail {
template <typename T> struct is_typed_key : std::false_type {};

template <typename T, auto... Values>
struct is_typed_key<key_type<T, Values...>>
    : std::bool_constant<sizeof...(Values) == 0 && !std::is_void_v<T>> {};

template <> struct is_typed_key<key_type<::dingo::none_t>> : std::false_type {};

template <typename T>
inline constexpr bool is_typed_key_v = is_typed_key<std::decay_t<T>>::value;

template <typename T> struct is_key_value : std::false_type {};

template <typename T, auto Value>
struct is_key_value<key_type<T, Value>>
    : std::bool_constant<!std::is_void_v<T>> {};

template <typename T>
inline constexpr bool is_key_value_v = is_key_value<std::decay_t<T>>::value;

template <typename Selector> struct key_value_traits;

template <typename T, auto Value> struct key_value_traits<key_type<T, Value>> {
  using type = T;

  static T make() { return fixed_key_value<T, Value>::make(); }
};

template <typename T, auto Value>
struct is_value_selector<key_type<T, Value>> : std::true_type {
  using type = T;
  static T make() { return fixed_key_value<T, Value>::make(); }
};

template <typename Identity, typename Binding>
struct keyed_binding_identity : Binding {
  using Binding::Binding;
};

template <typename T>
struct is_runtime_registration_key_arg : std::false_type {};

template <typename T, typename Interface>
struct is_runtime_registration_key_arg<::dingo::key_value<T, Interface>>
    : std::bool_constant<!std::is_void_v<T>> {};

template <typename T> struct runtime_registration_key_arg_traits;

template <typename T, typename Interface>
struct runtime_registration_key_arg_traits<::dingo::key_value<T, Interface>> {
  using key_type = T;
  using interface_type = Interface;
};

template <typename T>
inline constexpr bool is_runtime_registration_key_arg_v =
    is_runtime_registration_key_arg<std::decay_t<T>>::value;

template <typename... Args>
struct are_runtime_registration_key_args
    : std::conjunction<is_runtime_registration_key_arg<std::decay_t<Args>>...> {
};

template <typename... Args>
inline constexpr bool are_runtime_registration_key_args_v =
    are_runtime_registration_key_args<Args...>::value;

template <typename...>
inline constexpr bool lookup_key_dependent_false_v = false;

template <typename T> struct lookup_key {
  static_assert(lookup_key_dependent_false_v<T>,
                "dingo::detail::lookup_key<T> requires T to be "
                "dingo::key_type<...> or dingo::key_value<...>");
};

template <typename T> struct is_lookup_key : std::false_type {};

template <typename T> struct is_lookup_key<lookup_key<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_lookup_key_v = is_lookup_key<std::decay_t<T>>::value;

template <> struct lookup_key<::dingo::key_type<::dingo::none_t>> {
  using selector_type = ::dingo::key_type<::dingo::none_t>;
  using definition_type = lookup_key<::dingo::key_type<::dingo::none_t>>;
  using backend_key_type = ::dingo::none_t;

  ::dingo::none_t backend_key() const { return {}; }
};

template <typename T> struct lookup_key<::dingo::key_type<T>> {
  using selector_type = ::dingo::key_type<T>;
  using definition_type = lookup_key<::dingo::key_type<T>>;
  using backend_key_type = ::dingo::none_t;

  ::dingo::none_t backend_key() const { return {}; }
};

template <typename T, auto Value>
struct lookup_key<::dingo::key_type<T, Value>> {
  using selector_type = ::dingo::key_type<T, Value>;
  using definition_type = lookup_key<::dingo::key_value<T>>;
  using backend_key_type = T;

  constexpr T backend_key() const { return fixed_key_value<T, Value>::make(); }
};

template <typename T, typename Interface>
struct lookup_key<::dingo::key_value<T, Interface>> {
  using selector_type = ::dingo::key_value<T, Interface>;
  using definition_type = lookup_key<::dingo::key_value<T>>;
  using backend_key_type = T;
  using interface_type = Interface;

  explicit lookup_key(::dingo::key_value<T, Interface> key_value)
      : key_value_(std::move(key_value)) {}

  const T &backend_key() const { return key_value_.value(); }
  const ::dingo::key_value<T, Interface> &key_value() const {
    return key_value_;
  }

private:
  ::dingo::key_value<T, Interface> key_value_;
};

template <typename T>
inline constexpr bool is_static_lookup_key_definition_v =
    std::is_same_v<typename std::decay_t<T>::backend_key_type, ::dingo::none_t>;

using no_lookup_key_t = lookup_key<::dingo::key_type<::dingo::none_t>>;

template <typename Key>
inline constexpr bool is_no_lookup_key_v =
    std::is_same_v<typename std::decay_t<Key>::definition_type,
                   no_lookup_key_t>;

template <typename Left, typename Right>
struct fixed_lookup_keys_match : std::false_type {};

template <typename Key, auto Left, auto Right>
struct fixed_lookup_keys_match<lookup_key<::dingo::key_type<Key, Left>>,
                               lookup_key<::dingo::key_type<Key, Right>>>
    : std::bool_constant<(Key{Left} == Key{Right})> {};

template <typename Left, typename Right> struct lookup_keys_match {
private:
  using left_lookup = std::decay_t<Left>;
  using right_lookup = std::decay_t<Right>;
  using left_selector = typename left_lookup::selector_type;
  using right_selector = typename right_lookup::selector_type;

public:
  static constexpr bool value =
      std::is_same_v<typename left_lookup::definition_type,
                     typename right_lookup::definition_type> &&
      (std::is_same_v<left_selector, right_selector> ||
       fixed_lookup_keys_match<left_lookup, right_lookup>::value);
};

inline no_lookup_key_t no_lookup_key() { return {}; }

inline lookup_key<::dingo::key_type<::dingo::none_t>>
make_lookup_key(::dingo::none_t = {}) {
  return no_lookup_key();
}

template <typename T> lookup_key<T> make_lookup_key(lookup_key<T> key) {
  return key;
}

template <typename T>
lookup_key<::dingo::key_type<T>> make_lookup_key(type_selector<T>) {
  return {};
}

template <typename T, auto... Values>
lookup_key<::dingo::key_type<T, Values...>>
make_lookup_key(type_selector<::dingo::key_type<T, Values...>>) {
  return {};
}

template <typename T, auto Value>
lookup_key<::dingo::key_type<T, Value>>
make_lookup_key(type_selector<value_selector<T, Value>>) {
  return {};
}

template <typename T, auto Value>
lookup_key<::dingo::key_type<T, Value>>
make_lookup_key(value_selector<T, Value>) {
  return {};
}

template <typename Selector,
          std::enable_if_t<is_value_selector_v<Selector>, int> = 0>
lookup_key<::dingo::key_value<value_selector_type_t<Selector>>>
make_lookup_key(Selector) {
  using key_type = value_selector_type_t<Selector>;
  return lookup_key<::dingo::key_value<key_type>>(
      ::dingo::key_value<key_type>(is_value_selector<Selector>::make()));
}

template <typename T, typename Interface>
void make_lookup_key(type_selector<::dingo::key_value<T, Interface>>) = delete;

inline lookup_key<::dingo::key_type<::dingo::none_t>>
make_lookup_key(::dingo::key_type<::dingo::none_t>) {
  return no_lookup_key();
}

template <typename T>
lookup_key<::dingo::key_type<T>> make_lookup_key(::dingo::key_type<T>) {
  return {};
}

template <typename T, auto Value>
lookup_key<::dingo::key_type<T, Value>>
make_lookup_key(::dingo::key_type<T, Value>) {
  return {};
}

template <typename T, typename Interface>
lookup_key<::dingo::key_value<T, Interface>>
make_lookup_key(::dingo::key_value<T, Interface> key_value) {
  return lookup_key<::dingo::key_value<T, Interface>>(std::move(key_value));
}

template <typename T>
std::enable_if_t<!std::is_same_v<std::decay_t<T>, ::dingo::none_t> &&
                     !is_typed_key_v<std::decay_t<T>> &&
                     !is_key_value_v<std::decay_t<T>> &&
                     !is_runtime_registration_key_arg_v<std::decay_t<T>>,
                 lookup_key<::dingo::key_value<std::decay_t<T>>>>
make_lookup_key(T &&key) {
  return lookup_key<::dingo::key_value<std::decay_t<T>>>(
      ::dingo::key_value<std::decay_t<T>>(std::forward<T>(key)));
}

template <typename T> struct make_lookup_key_type {
  using type = decltype(make_lookup_key(std::declval<T>()));
};

template <typename T>
using make_lookup_key_t = typename make_lookup_key_type<std::decay_t<T>>::type;

template <typename Key> decltype(auto) lookup_backend_key(const Key &key) {
  return key.backend_key();
}

template <typename LookupKey, typename = void>
struct is_static_lookup_key : std::false_type {};

template <typename Key, auto... Values>
struct is_static_lookup_key<
    ::dingo::detail::lookup_key<::dingo::key_type<Key, Values...>>>
    : std::true_type {};

template <typename LookupKey>
inline constexpr bool is_static_lookup_key_v =
    is_static_lookup_key<LookupKey>::value;

} // namespace detail

} // namespace dingo
