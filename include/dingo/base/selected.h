//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <type_traits>
#include <utility>

namespace dingo {
struct none_t;
template <typename T, auto... Values> struct key_type;

namespace detail {

template <typename T, typename Selector> struct selected;
template <typename Tag> struct type_selector {};

// What makes the value of a selection in place: a selection is constructed of
// it, and of nothing else that converts to it, so that constructor detection
// does not see the constructor.
template <typename Make> struct selected_maker {
  Make &make;
};

template <typename T> inline constexpr bool is_selected_maker_v = false;

template <typename Make>
inline constexpr bool is_selected_maker_v<selected_maker<Make>> = true;

// The value a selection holds: T of a selected<T, Selector>, and of a type a
// selection declares to hold it.
template <typename R> struct selection_value {
  using type = void;
};

template <typename T, typename Selector>
struct selection_value<selected<T, Selector>> {
  using type = T;
};

// What a request returns, made of what make returns: in place, when it is a
// selection of the value make returns.
template <typename R, typename Make> R make_result(Make &&make) {
  using made = decltype(make());
  using selected_value = typename selection_value<std::remove_cv_t<R>>::type;
  if constexpr (!std::is_reference_v<R> && !std::is_reference_v<made> &&
                std::is_same_v<std::remove_cv_t<selected_value>,
                               std::remove_cv_t<made>> &&
                (std::is_move_constructible_v<selected_value> ||
                 std::is_copy_constructible_v<selected_value>)) {
    return R(selected_maker<std::remove_reference_t<Make>>{make});
  } else {
    return std::forward<Make>(make)();
  }
}
template <typename Key, auto Value> struct value_selector {};

template <typename T, typename Selector,
          bool StoresValue =
              !std::is_reference_v<T> && (std::is_move_constructible_v<T> ||
                                          std::is_copy_constructible_v<T>)>
struct selected_base;

template <typename T, typename Selector>
struct selected_base<T, Selector, true> {
  template <typename U = T,
            std::enable_if_t<std::is_copy_constructible_v<U>, int> = 0>
  explicit selected_base(const T &value) : value_(value) {}
  template <typename U = T,
            std::enable_if_t<std::is_move_constructible_v<U>, int> = 0>
  explicit selected_base(T &&value) : value_(std::move(value)) {}
  // The value the maker makes, made in place.
  template <typename Maker,
            std::enable_if_t<is_selected_maker_v<std::decay_t<Maker>>, int> = 0>
  explicit selected_base(Maker &&maker) : value_(maker.make()) {}

  operator T() {
    if constexpr (std::is_move_constructible_v<T>) {
      return std::move(value_);
    } else {
      return value_;
    }
  }

private:
  T value_;
};

template <typename T, typename Selector>
struct selected_base<T &, Selector, false> {
  explicit selected_base(T &value) : value_(value) {}

  operator T &() { return value_; }

private:
  T &value_;
};

// An rvalue reference: made of the rvalue the request resolves to, and given
// back as one.
template <typename T, typename Selector>
struct selected_base<T &&, Selector, false> {
  explicit selected_base(T &&value) : value_(value) {}

  operator T &&() { return std::move(value_); }
  operator T &() { return value_; }

private:
  T &value_;
};

template <typename T, typename Selector>
struct selected_base<T, Selector, false> : selected_base<T &, Selector, false> {
  explicit selected_base(T &value)
      : selected_base<T &, Selector, false>(value) {}

  // A value that can be neither moved nor copied is not kept, and a reference
  // to the temporary it is made of would outlive it.
  template <typename U = T>
  explicit selected_base(std::add_rvalue_reference_t<U>)
      : selected_base<T &, Selector, false>(dangling()) {
    static_assert(!std::is_same_v<U, U>,
                  "an annotated or keyed value that can be neither moved nor "
                  "copied cannot be passed by value; request it by reference");
  }

private:
  static T &dangling();
};

template <typename T, typename Selector>
struct selected : selected_base<T, Selector> {
  using selected_base<T, Selector>::selected_base;
};

template <typename T> struct selected_traits {
  using type = T;
  using selector_type = key_type<none_t>;
  static constexpr bool is_selected = false;
};

template <typename T, typename Selector>
struct selected_traits<selected<T, Selector>> {
  using type = T;
  using selector_type = Selector;
  static constexpr bool is_selected = true;
};

template <typename T, typename Selector>
struct selected_traits<selected<T, Selector> &> {
  using type = T &;
  using selector_type = Selector;
  static constexpr bool is_selected = true;
};

template <typename T, typename Selector>
struct selected_traits<selected<T, Selector> &&> {
  using type = T &&;
  using selector_type = Selector;
  static constexpr bool is_selected = true;
};

template <typename T, typename Selector>
struct selected_traits<selected<T, Selector> *> {
  using type = T *;
  using selector_type = Selector;
  static constexpr bool is_selected = true;
};

template <typename T> using selected_type_t = typename selected_traits<T>::type;

template <typename T>
using selected_selector_t = typename selected_traits<T>::selector_type;

template <typename T>
struct is_selected : std::bool_constant<selected_traits<T>::is_selected> {};

template <typename T>
inline constexpr bool is_selected_v = is_selected<T>::value;

template <typename Selector> struct is_type_selector : std::false_type {};

template <typename Tag>
struct is_type_selector<type_selector<Tag>> : std::true_type {
  using type = Tag;
};

template <typename Selector>
inline constexpr bool is_type_selector_v =
    is_type_selector<std::decay_t<Selector>>::value;

template <typename Selector>
using type_selector_type_t =
    typename is_type_selector<std::decay_t<Selector>>::type;

template <typename Selector> struct is_value_selector : std::false_type {};

template <typename Key, auto Value>
struct is_value_selector<value_selector<Key, Value>> : std::true_type {
  using type = Key;
  static Key make() { return Key{Value}; }
};

template <typename Selector>
inline constexpr bool is_value_selector_v =
    is_value_selector<std::decay_t<Selector>>::value;

template <typename Selector>
using value_selector_type_t =
    typename is_value_selector<std::decay_t<Selector>>::type;
} // namespace detail

} // namespace dingo
