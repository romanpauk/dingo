//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The wrappers of the kind probes: a standard type in a wrapper of a layer
// kind of the user, `Kind`, whose layer_kind_traits user_kinds.py declares.
// Each provides every operation its kind may have a form of, so a route the
// declaration of the kind does not allow is not served because of the
// declaration, never because of the wrapper.

#include "probe_types.h"

#include <dingo/type/type_traits.h>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace dingo {
namespace probe_detail {
// The kind the share form of `Forms` leads to, or void.
template <typename Forms> struct share_target {
  using type = void;
};

template <typename Target, typename... Rest>
struct share_target<type_list<kind_form<route_form::share, Target>, Rest...>> {
  using type = Target;
};

template <typename First, typename... Rest>
struct share_target<type_list<First, Rest...>>
    : share_target<type_list<Rest...>> {};

template <typename Kind>
using share_target_t =
    typename share_target<typename layer_kind_traits<Kind>::forms>::type;
} // namespace probe_detail
} // namespace dingo

template <typename Kind, typename T> class kinded_unique;
template <typename Kind, typename T> class kinded_shared;

template <typename Kind, typename T> class kinded_unique {
public:
  kinded_unique() = default;
  explicit kinded_unique(std::unique_ptr<T> pointer)
      : pointer_(std::move(pointer)) {}

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  kinded_unique(kinded_unique<Kind, U> &&other)
      : pointer_(std::move(other).native()) {}

  T *get() const { return pointer_.get(); }
  std::unique_ptr<T> &&native() && { return std::move(pointer_); }

private:
  std::unique_ptr<T> pointer_;
};

template <typename Kind, typename T> class kinded_shared {
public:
  kinded_shared() = default;
  explicit kinded_shared(std::shared_ptr<T> pointer)
      : pointer_(std::move(pointer)) {}

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  kinded_shared(const kinded_shared<Kind, U> &other)
      : pointer_(other.native()) {}

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  kinded_shared(kinded_shared<Kind, U> &&other)
      : pointer_(std::move(other).native()) {}

  T *get() const { return pointer_.get(); }
  const std::shared_ptr<T> &native() const & { return pointer_; }
  std::shared_ptr<T> &&native() && { return std::move(pointer_); }

private:
  std::shared_ptr<T> pointer_;
};

template <typename Kind, typename T> class kinded_optional {
public:
  kinded_optional() = default;
  template <typename... Args>
  explicit kinded_optional(std::in_place_t, Args &&...args)
      : value_(std::in_place, std::forward<Args>(args)...) {}

  // The element, which the probes reach, or null.
  const T *get() const { return value_ ? std::addressof(*value_) : nullptr; }
  std::optional<T> &native() & { return value_; }
  const std::optional<T> &native() const & { return value_; }
  std::optional<T> &&native() && { return std::move(value_); }

private:
  std::optional<T> value_;
};

template <typename Kind, typename... Types> class kinded_variant {
public:
  template <std::size_t Index, typename... Args>
  explicit kinded_variant(std::in_place_index_t<Index> index, Args &&...args)
      : value_(index, std::forward<Args>(args)...) {}

  // The first alternative, which the probes reach, or null.
  const auto *get() const { return std::get_if<0>(&value_); }
  std::variant<Types...> &native() & { return value_; }
  const std::variant<Types...> &native() const & { return value_; }
  std::variant<Types...> &&native() && { return std::move(value_); }

private:
  std::variant<Types...> value_;
};

template <typename Kind, typename T, std::size_t Extent> struct kinded_array {
  // The first element, which the probes reach.
  const T *get() const { return elements.data(); }

  std::array<T, Extent> elements;
};

namespace dingo {

struct probe_kinded_family {};

template <typename Kind, typename T>
struct wrapper_traits<kinded_unique<Kind, T>> {
  using kind = Kind;
  using family = probe_kinded_family;
  using arguments = type_list<T>;
  template <typename U> using rebind = kinded_unique<Kind, U>;

  static T &element(const kinded_unique<Kind, T> &value) {
    return *value.get();
  }

  template <typename... Args>
  static kinded_unique<Kind, T> make(Args &&...args) {
    return kinded_unique<Kind, T>(
        std::make_unique<T>(std::forward<Args>(args)...));
  }

  static kinded_unique<Kind, T> adopt(T *element) {
    return kinded_unique<Kind, T>(std::unique_ptr<T>(element));
  }

  // The handle of the kind the share form of `Kind` leads to.
  template <typename Target = probe_detail::share_target_t<Kind>,
            typename = std::enable_if_t<!std::is_void_v<Target>>>
  static kinded_shared<Target, T> share(kinded_unique<Kind, T> &&value) {
    return kinded_shared<Target, T>(
        std::shared_ptr<T>(std::move(value).native()));
  }
};

template <typename Kind, typename T>
struct wrapper_traits<kinded_shared<Kind, T>> {
  using kind = Kind;
  using family = probe_kinded_family;
  using arguments = type_list<T>;
  template <typename U> using rebind = kinded_shared<Kind, U>;

  static T &element(const kinded_shared<Kind, T> &value) {
    return *value.get();
  }

  template <typename... Args>
  static kinded_shared<Kind, T> make(Args &&...args) {
    return kinded_shared<Kind, T>(
        std::make_shared<T>(std::forward<Args>(args)...));
  }

  static kinded_shared<Kind, T> adopt(T *element) {
    return kinded_shared<Kind, T>(std::shared_ptr<T>(element));
  }

  template <typename Deleter>
  static kinded_shared<Kind, T> adopt(T *element, Deleter deleter) {
    return kinded_shared<Kind, T>(
        std::shared_ptr<T>(element, std::move(deleter)));
  }
};

template <typename Kind, typename T>
struct wrapper_traits<kinded_optional<Kind, T>> {
  using kind = Kind;
  using family = probe_kinded_family;
  using arguments = type_list<T>;
  template <typename U> using rebind = kinded_optional<Kind, U>;

  static T &element(kinded_optional<Kind, T> &value) { return *value.native(); }
  static const T &element(const kinded_optional<Kind, T> &value) {
    return *value.native();
  }
  static T &&element(kinded_optional<Kind, T> &&value) {
    return std::move(*value.native());
  }

  template <typename... Args>
  static kinded_optional<Kind, T> make(Args &&...args) {
    return kinded_optional<Kind, T>(std::in_place, std::forward<Args>(args)...);
  }
};

template <typename Kind, typename... Types>
struct wrapper_traits<kinded_variant<Kind, Types...>> {
  using kind = Kind;
  using family = probe_kinded_family;
  using arguments = type_list<Types...>;
  template <typename... Us> using rebind = kinded_variant<Kind, Us...>;

  template <std::size_t Index>
  static auto &element(kinded_variant<Kind, Types...> &value) {
    return std::get<Index>(value.native());
  }
  template <std::size_t Index>
  static auto &element(const kinded_variant<Kind, Types...> &value) {
    return std::get<Index>(value.native());
  }
  template <std::size_t Index>
  static auto &&element(kinded_variant<Kind, Types...> &&value) {
    return std::get<Index>(std::move(value).native());
  }

  static std::size_t index(const kinded_variant<Kind, Types...> &value) {
    return value.native().index();
  }

  template <typename... Args>
  static kinded_variant<Kind, Types...> make(Args &&...args) {
    return kinded_variant<Kind, Types...>(std::in_place_index<0>,
                                          std::forward<Args>(args)...);
  }
};

template <typename Kind, typename T, std::size_t Extent>
struct wrapper_traits<kinded_array<Kind, T, Extent>> {
  using kind = Kind;
  using family = probe_kinded_family;
  using arguments = type_list<T>;
  static constexpr std::size_t extent = Extent;
  template <typename U> using rebind = kinded_array<Kind, U, Extent>;

  template <typename Array> static auto first(Array &value) {
    return value.elements.data();
  }

  // The array of `Extent` elements, each made from `element`.
  template <typename Element>
  static kinded_array<Kind, T, Extent> make(Element &&element) {
    return make_each(element, std::make_index_sequence<Extent>());
  }

private:
  template <typename Element, std::size_t... Indices>
  static kinded_array<Kind, T, Extent>
  make_each(Element &element, std::index_sequence<Indices...> /*indices*/) {
    return kinded_array<Kind, T, Extent>{
        {{(void(Indices), static_cast<T>(element))...}}};
  }
};

} // namespace dingo
