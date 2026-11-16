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

// The execution of a plan of the semantics (dingo/semantics/model.h) on C++
// objects: one step for each kind of step of the semantics, each by the type
// traits of its layers and by C++ itself; the program a plan is compiled
// into, which both containers run; and the run of a program as the code
// compiles, which applies its steps one after another, then satisfies the
// request of what they reach. What a step makes for the request is kept by a
// store, which constructs it in place of what makes it and keeps it while
// the request lives: keep<T>(make) returns a T & of T(make()).

namespace dingo::detail::execution {

// The object a plan reached, of type T, and whether the request owns it; and
// the nearest handle it was taken out of that owns it alone, with how to
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

// A step of a kind, to the layer Target where it makes one.
template <semantics::step Kind, typename Target = void> struct step;

// The element of a layer.
// What a pointer the request owns points to, released: the request owns it,
// and what keeps it deletes it as the request ends, as an array when the
// pointer is to the first element of one.
template <typename T, bool Array> struct released_pointer {
  T *pointer;
  explicit released_pointer(T *owned) : pointer(owned) {}
  released_pointer(const released_pointer &) = delete;
  released_pointer &operator=(const released_pointer &) = delete;
  ~released_pointer() {
    if constexpr (Array) {
      delete[] pointer;
    } else {
      delete pointer;
    }
  }
};

template <bool Array> struct release_step {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    static_assert(Owned && std::is_pointer_v<T>,
                  "only a pointer the request owns is released");
    using pointee = std::remove_pointer_t<T>;
    T pointer = *source.object;
    store.template keep<released_pointer<pointee, Array>>(
        [&] { return released_pointer<pointee, Array>(pointer); });
    // What it points to is released from what it was taken out of.
    source.released();
    return carrier<pointee, true>{pointer};
  }
};

// The element of a layer. A request of the element of an empty layer
// fails: the store rejects it.
template <> struct step<semantics::step::element> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    using element = held_element_t<T, Leaf>;
    carrier<element, Owned && owns_alone<T>()> reached{
        element_address<T, Leaf>(*source.object), source.owner, source.release};
    if (reached.object == nullptr) {
      store.absent();
    }
    if constexpr (Owned && releases_element_v<T>) {
      // The element of a handle the request owns is handed over with it.
      reached.owner = source.object;
      reached.release = &release_element<T>;
    }
    return reached;
  }
};

// A pointer to the element of a layer, null when it holds none: to its first
// element when it is an array.
template <> struct step<semantics::step::element_pointer> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    using element = held_element_t<T, Leaf>;
    using pointer = std::add_pointer_t<std::remove_extent_t<element>>;
    auto &made = store.template keep<pointer>([&] {
      return reinterpret_cast<pointer>(
          element_address<T, Leaf>(*source.object));
    });
    return carrier<pointer, false>{std::addressof(made)};
  }
};

// The element of a layer that may be empty, which holds a value that may be
// empty itself: that value, or an empty one when the layer is empty.
template <> struct step<semantics::step::element_or_empty> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    using element = held_element_t<T, Leaf>;
    constexpr bool owned = Owned && owns_alone<T>();
    if (auto *held = element_address<T, Leaf>(*source.object)) {
      return carrier<element, owned>{held};
    }
    using value_type = std::remove_cv_t<element>;
    return carrier<element, owned>{
        std::addressof(store.template keep<value_type>(
            [] { return type_traits<value_type>::empty_value(); }))};
  }
};

// A copy of what element_or_empty gives.
template <> struct step<semantics::step::element_or_empty_copy> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    using element = std::remove_cv_t<held_element_t<T, Leaf>>;
    auto *held = element_address<T, Leaf>(*source.object);
    auto &made = store.template keep<element>([held]() -> element {
      if (held) {
        return *held;
      }
      return type_traits<element>::empty_value();
    });
    return carrier<element, true>{std::addressof(made)};
  }
};

// The sharing handle an owned sole owner converts to, taking over its
// element: the share_t of its type traits.
template <> struct step<semantics::step::convert> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    static_assert(Owned, "only an owned value converts");
    using made_type = typename type_traits<T>::template share_t<
        typename type_traits<T>::value_type>;
    auto &made = store.template keep<made_type>(
        [&] { return made_type(std::move(*source.object)); });
    return carrier<made_type, true>{std::addressof(made)};
  }
};

// The handle of an array of unknown bound a handle of an array of a known
// bound converts to, holding the same array: the unknown_bound_t of its type
// traits, moved out of one the request owns or copied of another.
template <> struct step<semantics::step::unknown_bound> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    using type = std::remove_cv_t<T>;
    using made_type = typename type_traits<type>::template unknown_bound_t<
        typename type_traits<type>::value_type>;
    auto &made = store.template keep<made_type>([&] {
      if constexpr (Owned) {
        return made_type(std::move(*source.object));
      } else {
        return made_type(*source.object);
      }
    });
    return carrier<made_type, true>{std::addressof(made)};
  }
};

// The type of a stack with its leaf replaced by Interface.
// The leaf keeps its qualifiers as its interface; a new pointer or handle
// is of none.
template <typename T, typename Leaf, typename Interface> struct upcast_type {
  using layer_type = std::remove_cv_t<T>;
  static auto make() {
    if constexpr (std::is_same_v<layer_type, Leaf>) {
      return type_tag<qualified_like_t<T, Interface>>{};
    } else if constexpr (std::is_pointer_v<layer_type>) {
      return type_tag<typename upcast_type<std::remove_pointer_t<layer_type>,
                                           Leaf, Interface>::type *>{};
    } else {
      return type_tag<typename type_traits<layer_type>::template rebind_t<
          typename upcast_type<typename type_traits<layer_type>::value_type,
                               Leaf, Interface>::type>>{};
    }
  }
  template <typename U> struct type_tag {
    using type = U;
  };
  using type = typename decltype(make())::type;
};

// The leaf as its interface: the base it is, or a pointer, a handle or a
// stack of them of the interface, converted of the stack of the leaf.
template <> struct step<semantics::step::upcast> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    if constexpr (std::is_same_v<std::remove_cv_t<T>, Leaf>) {
      using qualified_interface = qualified_like_t<T, Interface>;
      return carrier<qualified_interface, Owned>{
          static_cast<qualified_interface *>(source.object), source.owner,
          source.release};
    } else if constexpr (std::is_pointer_v<T>) {
      // A pointer of the interface, to the same object: a pointer the request
      // owns stays owned, and still releases what it was taken out of.
      using made_type = typename upcast_type<T, Leaf, Interface>::type;
      auto &made = store.template keep<made_type>(
          [&] { return made_type(*source.object); });
      return carrier<made_type, Owned>{std::addressof(made), source.owner,
                                       source.release};
    } else {
      // A handle, or a stack of layers, of the interface: converted of the
      // one the request owns, which is moved, or copied, as a new object the
      // request owns.
      using made_type = typename upcast_type<T, Leaf, Interface>::type;
      auto &made = store.template keep<made_type>([&]() -> made_type {
        if constexpr (Owned) {
          return made_type(std::move(*source.object));
        } else {
          return made_type(*source.object);
        }
      });
      return carrier<made_type, Owned || std::is_copy_constructible_v<T>>{
          std::addressof(made)};
    }
  }
};

// A copy: a new object the request owns, of no qualifiers.
template <> struct step<semantics::step::copy> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    using made_type = std::remove_cv_t<T>;
    auto &made = store.template keep<made_type>(
        [object = source.object]() -> made_type { return made_type(*object); });
    return carrier<made_type, true>{std::addressof(made)};
  }
};

// A new layer of the layer Target holding what the request owns, moved into
// it.
template <typename Target> struct step<semantics::step::wrap, Target> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    using made_type = wrapped_t<Leaf, Interface, T>;
    auto &made = store.template keep<made_type>(
        [&] { return wrapped<Leaf, Interface>(std::move(*source.object)); });
    return carrier<made_type, true>{std::addressof(made)};
  }

  // A layer of the value, of the leaf when the value is the leaf as its
  // interface: a handle of the leaf, converted to a handle of the interface.
  template <typename Leaf, typename Interface, typename T>
  static auto wrapped(T &&value) {
    using value_type = std::remove_reference_t<T>;
    if constexpr (is_alternative_layer_v<Target>) {
      return type_traits<typename Target::type>::template wrap<
          typename Target::alternative>(std::forward<T>(value));
    } else if constexpr (std::is_same_v<value_type, Interface> &&
                         !std::is_same_v<Leaf, Interface>) {
      using handle = typename type_traits<Target>::template rebind_t<Leaf>;
      using made_type =
          typename type_traits<Target>::template rebind_t<Interface>;
      return made_type(type_traits<handle>::emplace(
          static_cast<Leaf &&>(static_cast<Interface &>(value))));
    } else {
      using made_type =
          typename type_traits<Target>::template rebind_t<value_type>;
      if constexpr (std::is_same_v<typename type_traits<made_type>::semantics,
                                   ::dingo::semantics::optional_value>) {
        return type_traits<made_type>::wrap(std::forward<T>(value));
      } else {
        return type_traits<made_type>::emplace(std::forward<T>(value));
      }
    }
  }

  template <typename Leaf, typename Interface, typename T>
  using wrapped_t = decltype(wrapped<Leaf, Interface>(std::declval<T &&>()));
};

// An array decayed to the pointer to its first element.
template <> struct step<semantics::step::decay> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    using pointer = std::add_pointer_t<std::remove_extent_t<T>>;
    auto &made = store.template keep<pointer>(
        [&] { return reinterpret_cast<pointer>(source.object); });
    if constexpr (Owned) {
      // The pointer of an array the request owns owns it: it is given up by
      // what it was taken out of.
      source.released();
    }
    return carrier<pointer, Owned>{std::addressof(made)};
  }
};

// An array the request owns, handed over as the pointer that owns it.
template <> struct step<semantics::step::hand_over> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    auto &made = store.template keep<T *>([&] { return source.object; });
    // The pointer owns the array now.
    source.released();
    return carrier<T *, Owned>{std::addressof(made)};
  }
};

// A type with qualifiers of its own, as a mask of the qualifiers of the
// semantics.
template <typename T, std::uint8_t Quals>
using qualified_t = std::conditional_t<
    (Quals & semantics::volatile_qual) != 0,
    std::add_volatile_t<std::conditional_t<(Quals & semantics::const_qual) != 0,
                                           std::add_const_t<T>, T>>,
    std::conditional_t<(Quals & semantics::const_qual) != 0,
                       std::add_const_t<T>, T>>;

// The pointer or the handle T of the same element of the qualifiers Quals.
template <typename T, std::uint8_t Quals, typename = void> struct requalified {
  using element = std::remove_cv_t<typename type_traits<T>::value_type>;
  using type =
      typename type_traits<T>::template rebind_t<qualified_t<element, Quals>>;
};

template <typename T, std::uint8_t Quals>
struct requalified<T, Quals, std::enable_if_t<std::is_pointer_v<T>>> {
  using type = qualified_t<std::remove_cv_t<std::remove_pointer_t<T>>, Quals> *;
};

template <typename T, std::uint8_t Quals>
using requalified_t = typename requalified<std::remove_cv_t<T>, Quals>::type;

// The qualifiers of the element of a pointer or a handle.
template <typename T> constexpr std::uint8_t element_quals() {
  using type = std::remove_cv_t<T>;
  using element =
      std::conditional_t<std::is_pointer_v<type>, std::remove_pointer_t<type>,
                         typename type_traits<type>::value_type>;
  return static_cast<std::uint8_t>(
      (std::is_const_v<element> ? semantics::const_qual : 0) |
      (std::is_volatile_v<element> ? semantics::volatile_qual : 0));
}

// A handle of the layer Target that does not own its element, made of a raw
// pointer to it.
// The qualifiers a layer a request makes holds its element of: none given,
// those of what it is made of.
inline constexpr std::uint8_t made_quals = 0xFF;

template <typename T, std::uint8_t Quals>
using held_t = std::conditional_t<Quals == made_quals, T,
                                  qualified_t<std::remove_cv_t<T>, Quals & 3>>;

template <typename Target, std::uint8_t Quals = made_quals>
struct observe_step {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    using pointee = held_t<std::remove_pointer_t<std::remove_cv_t<T>>, Quals>;
    using made_type = typename type_traits<Target>::template rebind_t<pointee>;
    auto &made = store.template keep<made_type>(
        [&] { return made_type(*source.object); });
    return carrier<made_type, true>{std::addressof(made)};
  }
};

// The handle of the layer Target that takes over a raw pointer to Pointee,
// of an array when Array, the pointer then pointing to its first element. A
// handle of an array is rebound to the element of the array, as its type
// traits rebind it; another one to what it holds.
template <typename Target, typename Pointee, bool Array>
struct adopting_handle {
  using type =
      typename type_traits<Target>::template rebind_t<std::conditional_t<
          is_array_like_type_v<Target>,
          std::conditional_t<Array, Pointee, std::remove_extent_t<Pointee>>,
          std::conditional_t<Array, Pointee[], Pointee>>>;
};

template <typename Target, typename Pointee, bool Array>
using adopting_handle_t =
    typename adopting_handle<Target, Pointee, Array>::type;

// The pointer a handle takes over of a raw pointer: the pointer itself, or,
// of a Whole array the handle holds, the pointer to its first element, as a
// handle of an array takes it.
template <bool Whole, typename Pointer> auto adopted_pointer(Pointer pointer) {
  if constexpr (Whole) {
    using element = std::remove_extent_t<std::remove_pointer_t<Pointer>>;
    return pointer ? &(*pointer)[0] : static_cast<element *>(nullptr);
  } else {
    return pointer;
  }
}

// A raw pointer the request owns, taken over by a handle of the layer
// Target: of an array when it points to the first element of one.
template <typename Target, bool Array, std::uint8_t Quals = made_quals>
struct adopt_step {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    using pointee = held_t<std::remove_pointer_t<T>, Quals>;
    using made_type = adopting_handle_t<Target, pointee, Array>;
    // A handle of an array takes over a pointer to the whole array as the
    // pointer to its first element.
    // A type, as MSVC does not take a constant of the function in a lambda.
    using whole = std::bool_constant<!Array && is_array_like_type_v<Target> &&
                                     std::is_array_v<pointee>>;
    auto &made = store.template keep<made_type>([&] {
      return made_type(adopted_pointer<whole::value>(*source.object));
    });
    // The handle owns what the pointer points to now.
    source.released();
    return carrier<made_type, true>{std::addressof(made)};
  }
};

// A pointer or a handle converted to one of an element of the qualifiers
// Quals: a new object, moved out of one the request owns, copied of another.
template <std::uint8_t Quals> struct qualify_step {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    using made_type = requalified_t<T, Quals>;
    auto &made = store.template keep<made_type>([&]() -> made_type {
      if constexpr (Owned) {
        return made_type(std::move(*source.object));
      } else {
        return made_type(*source.object);
      }
    });
    return carrier<made_type, true>{std::addressof(made)};
  }
};

// What a step a request takes makes of a carrier of T, as the code compiles
// without making it: the carrier it reaches, and whether C++ makes it. The
// semantics allow a step of a layer of any type; a type may not make it,
// and a binding makes only what its types do.
template <typename Step, typename Leaf, typename Interface, typename T,
          bool Owned, typename = void>
struct step_result {
  static constexpr bool possible = false;
  using type = void;
};

template <typename T, typename = void>
inline constexpr bool has_value_type_v = false;
template <typename T>
inline constexpr bool
    has_value_type_v<T, std::void_t<typename type_traits<T>::value_type>> =
        true;

template <typename Leaf, typename Interface, typename T, bool Owned>
struct step_result<
    step<semantics::step::copy>, Leaf, Interface, T, Owned,
    std::enable_if_t<std::is_object_v<T> && !std::is_array_v<T>>> {
  using made_type = std::remove_cv_t<T>;
  static constexpr bool possible = std::is_constructible_v<made_type, T &>;
  using type = carrier<made_type, true>;
};

template <typename Leaf, typename Interface, typename T, bool Owned>
struct step_result<step<semantics::step::decay>, Leaf, Interface, T, Owned,
                   std::enable_if_t<std::is_array_v<T>>> {
  static constexpr bool possible = true;
  using type = carrier<std::add_pointer_t<std::remove_extent_t<T>>, Owned>;
};

template <typename Leaf, typename Interface, typename T, bool Owned>
struct step_result<step<semantics::step::hand_over>, Leaf, Interface, T,
                   Owned> {
  static constexpr bool possible = true;
  using type = carrier<T *, Owned>;
};

template <typename Leaf, typename Interface, typename T, bool Owned>
struct step_result<step<semantics::step::convert>, Leaf, Interface, T, Owned,
                   std::void_t<typename type_traits<T>::template share_t<
                       typename type_traits<T>::value_type>>> {
  using made_type = typename type_traits<T>::template share_t<
      typename type_traits<T>::value_type>;
  static constexpr bool possible =
      Owned && std::is_constructible_v<made_type, T &&>;
  using type = carrier<made_type, true>;
};

template <typename Leaf, typename Interface, typename T, bool Owned>
struct step_result<
    step<semantics::step::unknown_bound>, Leaf, Interface, T, Owned,
    std::void_t<
        typename type_traits<std::remove_cv_t<T>>::template unknown_bound_t<
            typename type_traits<std::remove_cv_t<T>>::value_type>>> {
  using made_type =
      typename type_traits<std::remove_cv_t<T>>::template unknown_bound_t<
          typename type_traits<std::remove_cv_t<T>>::value_type>;
  static constexpr bool possible =
      Owned ? std::is_constructible_v<made_type, T &&>
            : std::is_constructible_v<made_type, T &>;
  using type = carrier<made_type, true>;
};

template <typename Target, std::uint8_t Quals, typename Leaf,
          typename Interface, typename T, bool Owned>
struct step_result<
    observe_step<Target, Quals>, Leaf, Interface, T, Owned,
    std::enable_if_t<
        std::is_pointer_v<std::remove_cv_t<T>> &&
        std::is_object_v<std::remove_pointer_t<std::remove_cv_t<T>>>>> {
  using pointee = held_t<std::remove_pointer_t<std::remove_cv_t<T>>, Quals>;
  template <typename U, typename = void> struct made {
    static constexpr bool possible = false;
    using type = void;
  };
  template <typename U>
  struct made<U,
              std::void_t<typename type_traits<Target>::template rebind_t<U>>> {
    using made_type = typename type_traits<Target>::template rebind_t<U>;
    static constexpr bool possible = std::is_constructible_v<made_type, T &>;
    using type = carrier<made_type, true>;
  };
  static constexpr bool possible = made<pointee>::possible;
  using type = typename made<pointee>::type;
};

template <typename Target, bool Array, std::uint8_t Quals, typename Leaf,
          typename Interface, typename T, bool Owned>
struct step_result<adopt_step<Target, Array, Quals>, Leaf, Interface, T, Owned,
                   std::enable_if_t<std::is_pointer_v<T>>> {
  using pointee = held_t<std::remove_pointer_t<T>, Quals>;
  static constexpr bool whole =
      !Array && is_array_like_type_v<Target> && std::is_array_v<pointee>;
  using pointer = std::conditional_t<whole, std::remove_extent_t<pointee> *, T>;
  template <typename U, typename = void> struct made {
    static constexpr bool possible = false;
    using type = void;
  };
  // A handle its type traits do not know, as of an array they do not say
  // how it deletes, is not made.
  template <typename U>
  struct made<U, std::void_t<adopting_handle_t<Target, U, Array>>> {
    using made_type = adopting_handle_t<Target, U, Array>;
    static constexpr bool possible =
        std::conjunction_v<std::bool_constant<type_traits<made_type>::enabled>,
                           std::is_constructible<made_type, pointer>>;
    using type = carrier<made_type, true>;
  };
  static constexpr bool possible = made<pointee>::possible;
  using type = typename made<pointee>::type;
};

template <std::uint8_t Quals, typename Leaf, typename Interface, typename T,
          bool Owned>
struct step_result<qualify_step<Quals>, Leaf, Interface, T, Owned,
                   std::enable_if_t<std::is_pointer_v<std::remove_cv_t<T>> ||
                                    has_value_type_v<std::remove_cv_t<T>>>> {
  template <typename U, typename = void> struct made {
    static constexpr bool possible = false;
    using type = void;
  };
  template <typename U> struct made<U, std::void_t<requalified_t<U, Quals>>> {
    using made_type = requalified_t<U, Quals>;
    static constexpr bool possible =
        Owned ? std::is_constructible_v<made_type, U &&>
              : std::is_constructible_v<made_type, U &>;
    using type = carrier<made_type, true>;
  };
  static constexpr bool possible = made<T>::possible;
  using type = typename made<T>::type;
};

// The store of what lives as long as the container, or as long as the
// request: a store that keeps them apart gives each with at<Stable>().
template <bool Stable, typename Store> decltype(auto) store_of(Store &store) {
  if constexpr (keeps_apart<Store>::value) {
    return store.template part<Stable>();
  } else {
    return (store);
  }
}

// A layer made of what its element is made of, in place: the layer's emplace
// or wrap is given it when the element can be constructed of it, which the
// element then is in place; otherwise the element is made the same way
// first, a handle, which moves without its leaf. The leaf is moved once,
// however many layers it is moved into.
template <typename Leaf, typename Interface, typename Made, typename T>
Made wrap_value(T &&value) {
  using value_type = std::remove_reference_t<T>;
  if constexpr (std::is_same_v<Made, value_type>) {
    return Made(std::forward<T>(value));
  } else if constexpr (is_alternative_type_v<Made>) {
    using element = held_element_t<Made, Leaf>;
    if constexpr (std::is_same_v<element, value_type>) {
      return type_traits<Made>::template wrap<element>(std::forward<T>(value));
    } else {
      return type_traits<Made>::template wrap<element>(
          wrap_value<Leaf, Interface, element>(std::forward<T>(value)));
    }
  } else {
    using element = typename type_traits<Made>::value_type;
    if constexpr (std::is_same_v<element, Interface> &&
                  std::is_same_v<value_type, Interface> &&
                  !std::is_same_v<Leaf, Interface>) {
      // The leaf as its interface: a handle of the leaf, converted.
      using handle = typename type_traits<Made>::template rebind_t<Leaf>;
      return Made(type_traits<handle>::emplace(
          static_cast<Leaf &&>(static_cast<Interface &>(value))));
    } else if constexpr (std::is_constructible_v<element, T &&>) {
      return type_traits<Made>::emplace(std::forward<T>(value));
    } else {
      return type_traits<Made>::emplace(
          wrap_value<Leaf, Interface, element>(std::forward<T>(value)));
    }
  }
}

// The step that makes a layer of a pointer, holding its element of the
// qualifiers Quals: of a layer a request makes of the qualifiers its element
// needs at once.
template <typename Step, std::uint8_t Quals> struct holding_step;
template <typename Target, std::uint8_t Given, std::uint8_t Quals>
struct holding_step<observe_step<Target, Given>, Quals> {
  using type = observe_step<Target, Quals>;
};
template <typename Target, bool Array, std::uint8_t Given, std::uint8_t Quals>
struct holding_step<adopt_step<Target, Array, Given>, Quals> {
  using type = adopt_step<Target, Array, Quals>;
};

// The innermost type of a stack toward Leaf: the leaf, the base of it the
// stack holds, or another type.
template <typename T, typename Leaf> struct innermost {
  template <typename U> struct tag {
    using type = U;
  };
  static auto make() {
    using type = std::remove_cv_t<T>;
    if constexpr (std::is_same_v<type, Leaf>) {
      return tag<Leaf>{};
    } else if constexpr (std::is_pointer_v<type>) {
      return tag<typename innermost<std::remove_pointer_t<type>, Leaf>::type>{};
    } else if constexpr (std::is_array_v<type>) {
      return tag<typename innermost<std::remove_extent_t<type>, Leaf>::type>{};
    } else if constexpr (type_traits<type>::enabled) {
      return tag<typename innermost<typename type_traits<type>::value_type,
                                    Leaf>::type>{};
    } else if constexpr (is_alternative_type_v<type>) {
      using held = typename alternatives_stack<
          Leaf, alternative_type_alternatives_t<type>>::alternative;
      if constexpr (std::is_void_v<held>) {
        return tag<type>{};
      } else {
        return tag<typename innermost<held, Leaf>::type>{};
      }
    } else {
      return tag<type>{};
    }
  }
  using type = typename decltype(make())::type;
};

template <typename T, typename Leaf>
using innermost_t = typename innermost<T, Leaf>::type;

// The interface a request names: its innermost type, of no qualifiers, or
// the innermost raw array, which is an interface of its own; a handle of an
// array names its element.
template <typename T> struct request_interface {
  template <typename U> struct tag {
    using type = U;
  };
  static auto make() {
    using type = std::remove_cv_t<T>;
    if constexpr (std::is_array_v<type>) {
      return tag<std::remove_cv_t<type>>{};
    } else if constexpr (std::is_pointer_v<type>) {
      return tag<
          typename request_interface<std::remove_pointer_t<type>>::type>{};
    } else if constexpr (type_traits<type>::enabled) {
      return tag<typename request_interface<
          typename type_traits<type>::value_type>::type>{};
    } else {
      return tag<type>{};
    }
  }
  using type = typename decltype(make())::type;
};

template <typename T>
using request_interface_t = typename request_interface<T>::type;

// The layers of the owning handles a pointer the request owns may be taken
// over by: those of owning_handles, which a user adds a family to.
template <typename Handles> struct owning_layers;
template <typename... Handles> struct owning_layers<type_list<Handles...>> {
  using type = type_list<wrapper_layer_tag<Handles>...>;
};

template <typename List, typename Other> struct joined;
template <typename... Items, typename... Others>
struct joined<type_list<Items...>, type_list<Others...>> {
  using type = type_list<Items..., Others...>;
};

// The layers of a stack in a universe.
template <typename Universe, typename... Tags>
constexpr void place_layers(semantics::layer *layers, type_list<Tags...>) {
  std::size_t index = 0;
  ((layers[index++] = Universe::template index<Tags>()), ...);
}

constexpr semantics::element semantic_element(stack_element element) {
  return element == stack_element::abstract ? semantics::element::abstract
                                            : semantics::element::leaf;
}

// A plan as both containers run it: a program of instructions, each of the
// plan steps from first to last, which it takes at once, with whether the
// binding takes them or the request; and how the request takes the value of
// what the last of them reaches. The containers take the decisions of how to
// run a plan here alone:
//
// - A run of wraps is one instruction, made at once of the value it wraps,
//   of the steps of one side.
// - A layer a request makes of a value or of a pointer is made of the
//   qualifiers its element needs at once: the conversion to them that follows
//   it is the same instruction.
// - A value a request takes is made where it takes it when the last step
//   makes it: a copy or a run of wraps the request makes is left to it; and a
//   copy of the element of a layer that may be empty, which the binding
//   makes, is made by the request of what element_or_empty gives.

// How a request takes the value of what a program reaches: as the plan
// satisfies it, or by making it of what the program reached, a copy of it or
// the value a run of wraps makes of it.
enum class finish : std::uint8_t { satisfy, copy, run };

// An instruction of a program: the plan steps from first to last, the kind
// it takes, and whether the binding takes it; the wraps of a run of wraps,
// and whether a conversion to a more qualified element ends it.
struct instruction {
  semantics::step kind = semantics::step::copy;
  std::uint8_t first = 0;
  std::uint8_t last = 0;
  bool binding = false;
  std::uint8_t wraps = 0;
  bool qualified = false;
};

struct program {
  instruction entries[semantics::max_plan] = {};
  std::size_t count = 0;
  finish last = finish::satisfy;
  // The wraps of a run the request makes last.
  std::uint8_t depth = 0;
  // The qualifiers of what the program reaches, which a copy is made of.
  std::uint8_t quals = 0;
};

// The program of a found plan of what a binding publishes, whose first
// source_count states are its layer sources: the steps into them are the
// binding's.
constexpr program compile_program(const semantics::plan &plan,
                                  std::size_t source_count) {
  using semantics::step;
  program made;
  const bool value = plan.how == semantics::satisfaction::move ||
                     plan.how == semantics::satisfaction::copy;
  // Whether the binding takes the plan step at an index.
  auto binding_takes = [&](std::size_t index) {
    return plan.states[index + 1] < source_count;
  };
  std::size_t index = 0;
  while (index < plan.count) {
    const step kind = plan.steps[index];
    std::size_t last = index;
    while (kind == step::wrap && last + 1 < plan.count &&
           plan.steps[last + 1] == step::wrap &&
           binding_takes(last + 1) == binding_takes(index)) {
      ++last;
    }
    const auto wraps =
        static_cast<std::uint8_t>(kind == step::wrap ? last - index + 1 : 0);
    bool qualified = false;
    if ((kind == step::wrap || kind == step::adopt || kind == step::observe) &&
        last + 1 < plan.count && plan.steps[last + 1] == step::qualify &&
        !binding_takes(last)) {
      ++last;
      qualified = true;
    }
    const bool binding = binding_takes(last);
    if (last + 1 == plan.count && value) {
      if (binding && kind == step::element_or_empty_copy) {
        made.entries[made.count++] = {step::element_or_empty,
                                      static_cast<std::uint8_t>(index),
                                      static_cast<std::uint8_t>(last),
                                      true,
                                      0,
                                      false};
        made.last = finish::copy;
        made.quals = plan.objects[index].quals[1];
        return made;
      }
      if (!binding && kind == step::copy) {
        made.last = finish::copy;
        made.quals = plan.objects[index].quals[0];
        return made;
      }
      if (!binding && kind == step::wrap && !qualified) {
        made.last = finish::run;
        made.depth = wraps;
        return made;
      }
    }
    made.entries[made.count++] = {kind,
                                  static_cast<std::uint8_t>(index),
                                  static_cast<std::uint8_t>(last),
                                  binding,
                                  wraps,
                                  qualified};
    index = last + 1;
  }
  return made;
}

// The shape of a binding as the semantics know it, of no C++ type: its
// context, the universe of its layers and its stored object, encoded as
// integers. The semantics of a binding are a template of its shape, so that
// a constant expression of them is evaluated once for every shape, not for
// every type of it.
struct shape_code {
  std::uint64_t entries[4 + 2 * semantics::max_kinds] = {};
  std::size_t count = 0;
};

constexpr std::uint64_t bit(bool value, unsigned index) {
  return static_cast<std::uint64_t>(value) << index;
}
constexpr bool test_bit(std::uint64_t code, unsigned index) {
  return ((code >> index) & 1u) != 0;
}

constexpr std::uint64_t encode_kind(const semantics::layer_kind &kind) {
  return static_cast<std::uint64_t>(kind.owns) | bit(kind.shares_element, 2) |
         bit(kind.copyable, 3) | bit(kind.wraps_value, 4) |
         bit(kind.holds_element_only, 5) | bit(kind.relocatable, 6) |
         bit(kind.gives_element, 7) | bit(kind.bounded, 8) |
         bit(kind.may_be_empty, 9) | bit(kind.has_empty_value, 10) |
         bit(kind.adopts, 11) | bit(kind.decays, 12) | bit(kind.adopted, 13) |
         bit(kind.adopted_array, 14) | bit(kind.rebinds, 15) |
         bit(kind.observes, 16) | bit(kind.qualifies, 17) |
         bit(kind.adopts_bounded, 18) |
         (static_cast<std::uint64_t>(kind.converts) << 32);
}

constexpr semantics::layer_kind decode_kind(std::uint64_t code,
                                            std::uint64_t unknown_bound) {
  semantics::layer_kind made;
  made.owns = static_cast<semantics::owner>(code & 3u);
  made.shares_element = test_bit(code, 2);
  made.copyable = test_bit(code, 3);
  made.wraps_value = test_bit(code, 4);
  made.holds_element_only = test_bit(code, 5);
  made.relocatable = test_bit(code, 6);
  made.gives_element = test_bit(code, 7);
  made.bounded = test_bit(code, 8);
  made.may_be_empty = test_bit(code, 9);
  made.has_empty_value = test_bit(code, 10);
  made.adopts = test_bit(code, 11);
  made.decays = test_bit(code, 12);
  made.adopted = test_bit(code, 13);
  made.adopted_array = test_bit(code, 14);
  made.rebinds = test_bit(code, 15);
  made.observes = test_bit(code, 16);
  made.qualifies = test_bit(code, 17);
  made.adopts_bounded = test_bit(code, 18);
  made.converts = static_cast<std::uint32_t>(code >> 32);
  made.converts_unknown_bound = static_cast<std::uint32_t>(unknown_bound);
  return made;
}

// The context of a binding, of no universe.
constexpr std::uint64_t encode_context(const semantics::context &context) {
  return bit(context.leaf.copyable, 0) | bit(context.leaf.movable, 1) |
         bit(context.leaf.default_constructible, 2) |
         bit(context.leaf.factory_made, 3) |
         bit(context.leaf.copies_volatile, 4) | bit(context.scope.makes, 5) |
         bit(context.scope.given, 6) | bit(context.scope.publishes_early, 7) |
         bit(context.abstract, 8) |
         (static_cast<std::uint64_t>(context.limits) << 16);
}

constexpr semantics::context decode_context(std::uint64_t code,
                                            const semantics::universe *layers) {
  return {{test_bit(code, 0), test_bit(code, 1), test_bit(code, 2),
           test_bit(code, 3), test_bit(code, 4)},
          {test_bit(code, 5), test_bit(code, 6), test_bit(code, 7)},
          test_bit(code, 8),
          static_cast<std::uint8_t>(code >> 16),
          layers};
}

// An object, in two integers.
constexpr std::uint64_t encode_object(const semantics::object &object) {
  std::uint64_t made = 0;
  for (std::size_t index = 0; index < semantics::max_layers; ++index) {
    made |= static_cast<std::uint64_t>(object.layers[index]) << (8 * index);
  }
  return made | (static_cast<std::uint64_t>(object.depth) << 48) |
         (static_cast<std::uint64_t>(object.element) << 52) |
         (static_cast<std::uint64_t>(object.origin) << 54) |
         bit(object.instance, 56) | bit(object.unconstructed, 57) |
         bit(object.absent, 58) | bit(object.owns_pointee, 59) |
         bit(object.made_layer, 60) | bit(object.qualified, 61);
}

constexpr std::uint64_t encode_object_detail(const semantics::object &object) {
  std::uint64_t made = 0;
  for (std::size_t index = 0; index <= semantics::max_layers; ++index) {
    made |= static_cast<std::uint64_t>(object.lifetimes[index]) << (2 * index);
    made |= static_cast<std::uint64_t>(object.quals[index] & 15u)
            << (14 + 4 * index);
  }
  return made | (static_cast<std::uint64_t>(object.contained) << 42) |
         (static_cast<std::uint64_t>(object.full & 15u) << 50);
}

constexpr semantics::object decode_object(std::uint64_t code,
                                          std::uint64_t detail) {
  semantics::object made;
  for (std::size_t index = 0; index < semantics::max_layers; ++index) {
    made.layers[index] = static_cast<semantics::layer>(code >> (8 * index));
  }
  made.depth = static_cast<std::uint8_t>((code >> 48) & 15u);
  made.element = static_cast<semantics::element>((code >> 52) & 3u);
  made.origin = static_cast<semantics::origin>((code >> 54) & 3u);
  made.instance = test_bit(code, 56);
  made.unconstructed = test_bit(code, 57);
  made.absent = test_bit(code, 58);
  made.owns_pointee = test_bit(code, 59);
  made.made_layer = test_bit(code, 60);
  made.qualified = test_bit(code, 61);
  for (std::size_t index = 0; index <= semantics::max_layers; ++index) {
    made.lifetimes[index] =
        static_cast<semantics::lifetime>((detail >> (2 * index)) & 3u);
    made.quals[index] =
        static_cast<std::uint8_t>((detail >> (14 + 4 * index)) & 15u);
  }
  made.contained = static_cast<semantics::layer>((detail >> 42) & 255u);
  made.full = static_cast<std::uint8_t>((detail >> 50) & 15u);
  return made;
}

constexpr shape_code encode_shape(const semantics::context &context,
                                  const semantics::object &start) {
  shape_code made;
  made.entries[made.count++] = encode_context(context);
  made.entries[made.count++] = encode_object(start);
  made.entries[made.count++] = encode_object_detail(start);
  made.entries[made.count++] = context.layers->count;
  for (std::size_t index = 0; index < context.layers->count; ++index) {
    made.entries[made.count++] = encode_kind(context.layers->kinds[index]);
    made.entries[made.count++] =
        context.layers->kinds[index].converts_unknown_bound;
  }
  return made;
}

// The semantics of a binding of a shape: what it publishes, and the layer
// sources of it closed under every step a request takes but a wrap, in the
// layers the binding knows, which a request reaches from.
template <std::uint64_t... Code> struct binding_shape {
  static constexpr std::uint64_t code[] = {Code...};
  static constexpr semantics::universe make_universe() {
    semantics::universe made;
    made.count = static_cast<std::size_t>(code[3]);
    for (std::size_t index = 0; index < made.count; ++index) {
      made.kinds[index] = decode_kind(code[4 + 2 * index], code[5 + 2 * index]);
    }
    return made;
  }
  static constexpr semantics::universe universe = make_universe();
  static constexpr semantics::context context =
      decode_context(code[0], &universe);
  static constexpr semantics::object start = decode_object(code[1], code[2]);
  static constexpr semantics::search sources =
      semantics::layer_sources(start, context);
  static constexpr semantics::search closed =
      semantics::closed_layer_sources(sources, context);
};

// The shape of a binding of a plan, for a request of its leaf or of its
// abstract interface. It is made outside the plan, which is complete by the
// time it is: MSVC does not evaluate a function of a class in the
// declarations of the class.
template <typename Plan, bool Abstract> struct plan_shape {
  static constexpr shape_code code =
      encode_shape(Plan::context(Abstract), Plan::start());
  template <std::size_t... I>
  static auto make(std::index_sequence<I...>)
      -> binding_shape<code.entries[I]...>;
  using type = decltype(make(std::make_index_sequence<code.count>{}));
};

// What a binding of a stored object publishes, planned as the code compiles,
// of no request: the layer sources of its stored object, in a universe of the
// layers it takes steps into, those it stores and those of the handles a
// pointer it makes may be taken over by. The stored object is made for the
// request by a scope that makes its instance (Makes), or kept, and it is
// published early by a scope that does (Early).
template <typename Stored, typename Leaf, bool Makes, bool Early>
struct binding_plan {
  using stored_stack = stack_t<Stored, Leaf>;
  using layers = typename joined<
      typename stored_stack::layers,
      typename owning_layers<typename owning_handles<
          typename owning_handles_key<Leaf>::type>::type>::type>::type;
  using universe_type = layer_universe<layers>;

  static constexpr semantics::context context(bool abstract) {
    return {leaf_traits_v<Leaf>(),
            {Makes, false, Early},
            abstract,
            static_cast<std::uint8_t>(
                static_cast<unsigned>(
                    semantics::limitation::pointer_out_of_made_holder) |
                static_cast<unsigned>(
                    semantics::limitation::pointer_into_converted_copy) |
                static_cast<unsigned>(
                    semantics::limitation::qualified_below_element)),
            &universe_type::value};
  }

  static constexpr semantics::object start() {
    semantics::object made;
    place_layers<universe_type>(made.layers, typename stored_stack::layers{});
    made.depth = static_cast<std::uint8_t>(
        type_list_size_v<typename stored_stack::layers>);
    made.element = semantic_element(stored_stack::element);
    stack_quals<Stored, Leaf>::fill(made.quals);
    for (std::size_t index = 0; index <= made.depth; ++index) {
      made.lifetimes[index] =
          Makes ? semantics::lifetime::request : semantics::lifetime::stable;
    }
    made.origin = Makes ? semantics::origin::made : semantics::origin::borrowed;
    made.instance = true;
    // A pointer a scope makes owns what it points to.
    made.owns_pointee =
        Makes && made.depth && made.layers[0] == semantics::pointer_layer;
    return made;
  }

  // The shape of the binding, for a request of the leaf, and for one of its
  // abstract interface.
  template <bool Abstract>
  using shape = typename plan_shape<binding_plan, Abstract>::type;

  // What the binding publishes for a request of the leaf, and for one of its
  // abstract interface, of its shape. Each is whole: a stored object of more
  // layer sources than a search of them holds does not compile.
  template <bool Abstract>
  static constexpr const semantics::search &sources = shape<Abstract>::sources;
  // The layer sources closed under every step a request takes but a wrap,
  // in the layers the binding knows, which a request reaches from.
  template <bool Abstract>
  static constexpr const semantics::search &closed = shape<Abstract>::closed;
  template <bool Abstract> static constexpr bool whole() {
    static_assert(!sources<Abstract>.overflow && !closed<Abstract>.overflow,
                  "the stored object publishes more layer sources than "
                  "semantics::max_states");
    return true;
  }
};

// A request of a stored object, planned as the code compiles: what C++ says
// of the stored type Stored, of the request type Request and of the leaf
// Leaf, classified as the semantics know them, and the plan of the request
// for each lifetime it may need: as long as the container, when what it
// refers to is passed to the caller, or as long as the request. The request
// is planned from what the binding publishes, in a universe of the layers of
// the binding, which those of the request follow.
template <typename Stored, typename Request, typename Leaf, bool Makes,
          bool Early>
struct request_plan {
  using binding = binding_plan<Stored, Leaf, Makes, Early>;
  using category = requested_category<Request>;
  using shape = typename category::shape;
  using stored_stack = typename binding::stored_stack;
  using request_stack = stack_t<shape, Leaf>;
  using universe_type =
      layer_universe<typename joined<typename binding::layers,
                                     typename request_stack::layers>::type>;
  using identities = typename universe_type::identities;
  using leaf_type = Leaf;
  using interface_type = innermost_t<shape, Leaf>;

  static constexpr bool abstract =
      request_stack::element == stack_element::abstract;

  static constexpr bool modeled =
      category::modeled && stored_stack::element != stack_element::other &&
      request_stack::element != stack_element::other;

  static constexpr semantics::context context() {
    semantics::context context = binding::context(abstract);
    context.layers = &universe_type::value;
    return context;
  }

  static constexpr semantics::request goal() {
    semantics::request made;
    place_layers<universe_type>(made.layers, typename request_stack::layers{});
    made.depth = static_cast<std::uint8_t>(
        type_list_size_v<typename request_stack::layers>);
    made.element = semantic_element(request_stack::element);
    stack_quals<shape, Leaf>::fill(made.quals);
    return made;
  }

  static constexpr semantics::plan make(semantics::lifetime needed) {
    if constexpr (!modeled) {
      (void)needed;
      return {};
    } else {
      static_assert(binding::template whole<abstract>());
      constexpr semantics::context closed_context = context();
      const semantics::search found = semantics::reach_closed(
          binding::template closed<abstract>,
          binding::context(abstract).layers->count, closed_context, goal());
      return semantics::answer_plan(
          found, semantics::satisfy(found, goal(), category::value, needed,
                                    closed_context));
    }
  }

  // The plans of a request that needs what it refers to to live as long as
  // the container, and as long as the request.
  static constexpr semantics::plan stable = make(semantics::lifetime::stable);
  static constexpr semantics::plan transient =
      make(semantics::lifetime::request);

  // The programs of the plans.
  static constexpr std::size_t source_count =
      binding::template sources<abstract>.count;
  static constexpr program stable_program =
      compile_program(stable, source_count);
  static constexpr program transient_program =
      compile_program(transient, source_count);
};

// The C++ step of a step of the semantics, from the object Semantic::source()
// to the object Semantic::reached(), to the layers of a universe of identities.
template <typename Identities, typename Semantic> struct typed_step {
  static constexpr semantics::step kind = Semantic::kind;
  static constexpr semantics::object reached = Semantic::reached();

  template <semantics::layer Layer>
  using layer_type = type_list_element_t<Layer - 4, Identities>;

  static auto make() {
    if constexpr (kind == semantics::step::wrap) {
      return step<semantics::step::wrap, layer_type<reached.layers[0]>>{};
    } else if constexpr (kind == semantics::step::release) {
      // The pointer is to the first element of an array when the step
      // before decayed or handed over an array.
      return release_step<Semantic::source().layers[0] ==
                          semantics::array_pointer_layer>{};
    } else if constexpr (kind == semantics::step::adopt) {
      return adopt_step<layer_type<reached.layers[0]>,
                        reached.layers[1] == semantics::array_layer>{};
    } else if constexpr (kind == semantics::step::observe) {
      return observe_step<layer_type<reached.layers[0]>>{};
    } else if constexpr (kind == semantics::step::qualify) {
      return qualify_step<reached.quals[1]>{};
    } else {
      return step<kind>{};
    }
  }
  using step_type = decltype(make());
};

// The step at an index of a plan.
template <const semantics::plan &Plan, std::size_t Index> struct plan_step {
  static constexpr semantics::step kind = Plan.steps[Index];
  static constexpr semantics::object source() { return Plan.objects[Index]; }
  static constexpr semantics::object reached() {
    return Plan.objects[Index + 1];
  }
};

// The step into a state of a search.
template <const semantics::search &Search, std::size_t State>
struct search_step {
  static constexpr semantics::step kind = Search.states[State].via;
  static constexpr semantics::object source() {
    return Search.states[Search.states[State].parent].reached;
  }
  static constexpr semantics::object reached() {
    return Search.states[State].reached;
  }
};

// The C++ step of the step at an index of a plan.
template <const semantics::plan &Plan, typename Identities, std::size_t Index>
using plan_step_t =
    typename typed_step<Identities, plan_step<Plan, Index>>::step_type;

// The type a run of Count wraps of a plan, from the plan step First, makes
// of a value of T.
template <const semantics::plan &Plan, typename Identities, typename Leaf,
          typename Interface, typename T, std::size_t First, std::size_t Count>
struct wrapped_run {
  using type =
      typename wrapped_run<Plan, Identities, Leaf, Interface,
                           typename plan_step_t<Plan, Identities, First>::
                               template wrapped_t<Leaf, Interface, T>,
                           First + 1, Count - 1>::type;
};

template <const semantics::plan &Plan, typename Identities, typename Leaf,
          typename Interface, typename T, std::size_t First>
struct wrapped_run<Plan, Identities, Leaf, Interface, T, First, 0> {
  using type = T;
};

// A program of a plan, run as the code compiles: each instruction applies
// its step to what the one before reached, and the request takes its value.
template <const semantics::plan &Plan, const program &Program,
          typename Identities, typename Leaf, typename Interface>
struct program_run {
  template <typename Request, std::size_t Index = 0, typename Carrier,
            typename Store>
  static Request run(Carrier source, Store &store) {
    if constexpr (Index == Program.count) {
      return finished<Request>(source);
    } else {
      return run<Request, Index + 1>(apply<Index>(source, store), store);
    }
  }

  // What the program reaches, before the request takes its value.
  template <std::size_t Index = 0, typename Carrier, typename Store>
  static auto reach(Carrier source, Store &store) {
    if constexpr (Index == Program.count) {
      return source;
    } else {
      return reach<Index + 1>(apply<Index>(source, store), store);
    }
  }

  template <std::size_t Index, typename T, bool Owned, typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    constexpr instruction Each = Program.entries[Index];
    constexpr semantics::object reached = Plan.objects[Each.last + 1];
    auto &&kept =
        store_of<reached.lifetimes[0] == semantics::lifetime::stable>(store);
    if constexpr (Each.kind == semantics::step::wrap) {
      using made_type = typename layer_wrapper<T, Index>::type;
      auto &made = kept.template keep<made_type>([&] {
        return wrap_value<Leaf, Interface, made_type>(
            std::move(*source.object));
      });
      return carrier<made_type, true>{std::addressof(made)};
    } else if constexpr (Each.qualified) {
      using step_type =
          typename holding_step<plan_step_t<Plan, Identities, Each.first>,
                                reached.quals[1]>::type;
      return step_type::template apply<Leaf, Interface>(source, kept);
    } else if constexpr (Each.kind == Plan.steps[Each.first]) {
      return plan_step_t<Plan, Identities,
                         Each.first>::template apply<Leaf, Interface>(source,
                                                                      kept);
    } else {
      // A step the program takes in place of the plan's.
      return step<Each.kind>::template apply<Leaf, Interface>(source, kept);
    }
  }

  // The type a run of wraps makes of a value of T, of the qualifiers its
  // element needs when a conversion to them ends it.
  template <typename T, std::size_t Index> struct layer_wrapper {
    static constexpr instruction Each = Program.entries[Index];
    using wrapped = typename wrapped_run<Plan, Identities, Leaf, Interface, T,
                                         Each.first, Each.wraps>::type;
    template <typename U> struct tag {
      using type = U;
    };
    static auto make() {
      if constexpr (Each.qualified) {
        return tag<
            requalified_t<wrapped, Plan.objects[Each.last + 1].quals[1]>>{};
      } else {
        return tag<wrapped>{};
      }
    }
    using type = typename decltype(make())::type;
  };

  // The request of what the program reached.
  template <typename Request, typename T, bool Owned>
  static Request finished(carrier<T, Owned> reached) {
    using semantics::satisfaction;
    if constexpr (Program.last == finish::copy) {
      return Request(*reached.object);
    } else if constexpr (Program.last == finish::run) {
      using made_type =
          typename wrapped_run<Plan, Identities, Leaf, Interface, T,
                               Plan.count - Program.depth, Program.depth>::type;
      return static_cast<Request>(
          wrap_value<Leaf, Interface, made_type>(std::move(*reached.object)));
    } else if constexpr (Plan.how == satisfaction::refer) {
      if constexpr (std::is_pointer_v<Request>) {
        return reached.object;
      } else {
        return static_cast<Request>(*reached.object);
      }
    } else if constexpr (Plan.how == satisfaction::point) {
      return *reached.object;
    } else if constexpr (Plan.how == satisfaction::take) {
      return static_cast<Request>(std::move(*reached.object));
    } else if constexpr (Plan.how == satisfaction::move) {
      return Request(std::move(*reached.object));
    } else {
      return Request(*reached.object);
    }
  }
};

// The program of the plan of a request_plan for a lifetime, run as the code
// compiles: as long as the container (Stable), or as long as the request.
template <typename Plan, bool Stable>
struct planned_run
    : program_run<Plan::transient, Plan::transient_program,
                  typename Plan::identities, typename Plan::leaf_type,
                  typename Plan::interface_type> {};

template <typename Plan>
struct planned_run<Plan, true>
    : program_run<Plan::stable, Plan::stable_program, typename Plan::identities,
                  typename Plan::leaf_type, typename Plan::interface_type> {};

// What a plan makes lives as long as the container when it is kept with the
// binding, under a key of its type.
template <typename T> inline constexpr char retained_key = 0;

// What C++ says of a type T toward the leaf Leaf, of the interface
// Interface, as the steps of the semantics apply to it.
template <typename T, typename Leaf, typename Interface> struct layer_types {
  using type = std::remove_cv_t<T>;

  // Whether a value of U can be copied, of any qualifiers.
  template <typename U> static constexpr bool copyable() {
    using value_type = std::remove_cv_t<U>;
    if constexpr (!std::is_object_v<U> || std::is_array_v<U> ||
                  !is_complete_v<value_type>) {
      return false;
    } else if constexpr (std::is_abstract_v<value_type>) {
      return false;
    } else if constexpr (std::is_volatile_v<U>) {
      return std::is_constructible_v<value_type, const volatile value_type &>;
    } else {
      return is_copy_constructible_v<value_type>;
    }
  }

  // Whether a value of U can be moved into a layer made of it.
  template <typename U> static constexpr bool movable() {
    if constexpr (!std::is_object_v<U> || std::is_array_v<U> ||
                  !is_complete_v<U> || std::is_const_v<U> ||
                  std::is_volatile_v<U>) {
      return false;
    } else {
      return !std::is_abstract_v<U> && std::is_move_constructible_v<U>;
    }
  }

  // Whether the type is a layer, which gives its element.
  static constexpr bool layer() {
    if constexpr (std::is_pointer_v<type>) {
      return std::is_object_v<std::remove_pointer_t<type>>;
    } else if constexpr (is_alternative_type_v<type>) {
      return !std::is_void_v<typename alternatives_stack<
          Leaf, alternative_type_alternatives_t<type>>::alternative>;
    } else if constexpr (std::is_array_v<type>) {
      return false;
    } else {
      return type_traits<type>::enabled;
    }
  }

  // The element type of the layer, or void.
  template <bool Layer = layer(), typename = void> struct element {
    using type = void;
  };
  template <typename Unused> struct element<true, Unused> {
    using type = held_element_t<T, Leaf>;
  };

  // The type of the k-th nested element, of a run of layers each made of
  // what it holds.
  template <std::size_t K, typename U = T> struct nested {
    using type =
        typename nested<K - 1, typename layer_types<U, Leaf, Interface>::
                                   template element<>::type>::type;
  };
  template <typename U> struct nested<0, U> {
    using type = U;
  };

  // Whether a layer of U is made of its element value.
  template <typename U> static constexpr bool wraps_value() {
    using layer_type = std::remove_cv_t<U>;
    if constexpr (std::is_pointer_v<layer_type> ||
                  std::is_array_v<layer_type> || !is_complete_v<layer_type>) {
      return false;
    } else if constexpr (is_alternative_type_v<layer_type>) {
      return true;
    } else if constexpr (type_traits<layer_type>::enabled &&
                         !is_array_like_type_v<layer_type>) {
      return wrapper_kind_v<layer_identity_t<layer_type>>.wraps_value;
    } else {
      return false;
    }
  }

  // Whether every layer from this one down to the K-th nested type is made
  // of what it holds, which is a value.
  template <std::size_t K, typename U = T> static constexpr bool runs() {
    if constexpr (K == 0) {
      // The value is moved into what holds it of the qualifiers it needs.
      return movable<std::remove_cv_t<U>>();
    } else if constexpr (!wraps_value<U>()) {
      return false;
    } else {
      return runs<K - 1, typename layer_types<
                             U, Leaf, Interface>::template element<>::type>();
    }
  }
};

// Whether a handle of an array is made of a value, as an array of it its
// type_traits emplaces.
template <typename Target, typename Value> constexpr bool emplaces_array() {
  if constexpr (type_traits<Target>::enabled && is_array_like_type_v<Target>) {
    return can_emplace_v<Target, Value &&>;
  } else {
    return false;
  }
}

// An object of type Target made of a value, Value: constructed of it, a
// handle of an array of it, or a wrapper made of it at once.
template <typename Target, typename Value> constexpr bool makes_target() {
  if constexpr (std::is_constructible_v<Target, Value &&>) {
    return true;
  } else if constexpr (emplaces_array<Target, Value>()) {
    return true;
  } else if constexpr (std::is_pointer_v<Target> || std::is_array_v<Target> ||
                       !type_traits<Target>::enabled) {
    return false;
  } else {
    constexpr std::size_t depth =
        type_list_size_v<typename stack_t<Target, Value>::layers>;
    return depth != 0 &&
           layer_types<Target, Value, Value>::template runs<depth>();
  }
}

// An object of type Target made of what a storage or a factory is given: a
// pointer to it, or the object makes_target makes of it.
template <typename Target, typename Source> Target make_value(Source &&source) {
  using value_type = remove_cvref_t<Source>;
  if constexpr (std::is_constructible_v<Target, Source &&>) {
    return Target(std::forward<Source>(source));
  } else if constexpr (std::is_pointer_v<Target> &&
                       std::is_lvalue_reference_v<Source>) {
    return std::addressof(source);
  } else if constexpr (emplaces_array<Target, value_type>()) {
    return type_traits<Target>::emplace(std::forward<Source>(source));
  } else {
    return wrap_value<value_type, value_type, Target>(
        std::forward<Source>(source));
  }
}

} // namespace dingo::detail::execution
