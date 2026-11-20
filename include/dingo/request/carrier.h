//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/request/classification.h>
#include <dingo/semantics/model.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/type_traits.h>

#include <cassert>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

// The carriers of what a conversion reaches: the object, whether the
// request owns it, and the handle it was taken out of. What an operation
// makes for the request is kept by a store, which constructs it in place of
// what makes it and keeps it while the request lives: keep<T>(make) returns
// a T & of T(make()).

namespace dingo::detail::execution {

// The object a derivation reached, of type T, and whether the request owns it;
// and the nearest handle it was taken out of that owns it alone, with how to
// release it, which a step that takes the object over does.
template <typename T, bool Owned> struct carrier {
  using type = T;
  static constexpr bool owned = Owned;
  T *object;
  void *owner = nullptr;
  void (*release)(void *owner) = nullptr;

  void released() const {
    if (release != nullptr) {
      release(owner);
    }
  }
};

// Gives up the element of a sole owner, which the request has taken over.
template <typename T> void release_element(void *owner) {
  (void)type_traits<T>::release(*static_cast<T *>(owner));
}

// Whether a layer is a handle that owns its element alone and can give it
// up.
template <typename T, typename = void>
inline constexpr bool releases_element_v = false;

template <typename T>
inline constexpr bool releases_element_v<
    T, std::void_t<decltype(type_traits<T>::release(std::declval<T &>()))>> =
    true;

template <bool Owned, typename T> carrier<T, Owned> carry(T &object) {
  return {std::addressof(object)};
}

// U with the qualifiers of T.
template <typename T, typename U>
using qualified_like_t = std::conditional_t<
    std::is_const_v<T>,
    std::conditional_t<std::is_volatile_v<T>, const volatile U, const U>,
    std::conditional_t<std::is_volatile_v<T>, volatile U, U>>;

// The element type of a layer of type T, toward Leaf, as the type declares
// it: what a pointer points to; what a wrapper holds, the array a handle of
// an array holds, of the bound its type traits give; the alternative of an
// alternative type that holds the leaf or its interface. An element held in
// place is of the qualifiers of the layer too.
template <typename T, typename Leaf> struct held_element {
  using layer_type = std::remove_cv_t<T>;
  static auto make() {
    if constexpr (std::is_pointer_v<layer_type>) {
      return type_tag<std::remove_pointer_t<layer_type>>{};
    } else if constexpr (std::is_array_v<layer_type>) {
      return type_tag<std::remove_extent_t<T>>{};
    } else if constexpr (is_alternative_type_v<layer_type>) {
      return type_tag<qualified_like_t<
          T,
          typename alternatives_stack<Leaf, alternative_type_alternatives_t<
                                                layer_type>>::alternative>>{};
    } else {
      using element = typename type_traits<layer_type>::value_type;
      if constexpr (is_array_like_type_v<layer_type>) {
        if constexpr (handle_extent_v<layer_type> == 0) {
          return type_tag<element[]>{};
        } else {
          return type_tag<element[handle_extent_v<layer_type>]>{};
        }
      } else if constexpr (
          wrapper_kind_v<layer_identity_t<layer_type>>.shares_element) {
        return type_tag<element>{};
      } else {
        return type_tag<qualified_like_t<T, element>>{};
      }
    }
  }
  template <typename U> struct type_tag {
    using type = U;
  };
  using type = typename decltype(make())::type;
};

template <typename T, typename Leaf>
using held_element_t = typename held_element<T, Leaf>::type;

// The address of the element of a layer, or null when it holds none.
template <typename T, typename Leaf>
std::add_pointer_t<held_element_t<T, Leaf>> element_address(T &layer) {
  using layer_type = std::remove_cv_t<T>;
  using element = held_element_t<T, Leaf>;
  // The layer is only read through; what its element is declares whether
  // the element may be changed.
  auto &plain = const_cast<layer_type &>(layer);
  if constexpr (std::is_pointer_v<layer_type>) {
    return plain;
  } else if constexpr (is_alternative_type_v<layer_type>) {
    return type_traits<layer_type>::template get<
        held_element_t<layer_type, Leaf>>(plain);
  } else if constexpr (std::is_array_v<element>) {
    return reinterpret_cast<std::add_pointer_t<element>>(
        type_traits<layer_type>::get(plain));
  } else {
    return type_traits<layer_type>::get(plain);
  }
}

// Whether the element of an owned layer is owned: a layer that owns it
// alone hands it over with itself.
template <typename T> constexpr bool owns_alone() {
  using layer_type = std::remove_cv_t<T>;
  if constexpr (std::is_pointer_v<layer_type>) {
    return false;
  } else if constexpr (is_alternative_type_v<layer_type>) {
    return true;
  } else {
    return std::is_same_v<typename type_traits<layer_type>::semantics,
                          ::dingo::semantics::sole_owner> ||
           std::is_same_v<typename type_traits<layer_type>::semantics,
                          ::dingo::semantics::optional_value>;
  }
}

// Whether a store keeps what lives as long as the container apart.
template <typename Store, typename = void>
struct keeps_apart : std::false_type {};

template <typename Store>
struct keeps_apart<
    Store, std::void_t<decltype(std::declval<Store &>().template part<true>())>>
    : std::true_type {};

} // namespace dingo::detail::execution
