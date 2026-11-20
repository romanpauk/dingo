//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/request/carrier.h>

#include <cassert>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

// The operation C++ does for each kind of step of the semantics
// (dingo/semantics/model.h), by the type traits of its layers and by C++
// itself, what it makes of a type, and the C++ properties of a type as the
// steps apply to it.

namespace dingo::detail::execution {

// A step of a kind, to the layer Target where it makes one.
template <semantics::step Kind, typename Target = void> struct operation;

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

template <bool Array> struct release_operation {
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
template <> struct operation<semantics::step::element> {
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
template <> struct operation<semantics::step::element_pointer> {
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
template <> struct operation<semantics::step::maybe_element> {
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

// A copy of what maybe_element gives.
template <> struct operation<semantics::step::maybe_element_copy> {
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
template <> struct operation<semantics::step::transfer> {
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
template <> struct operation<semantics::step::unknown_bound> {
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
template <> struct operation<semantics::step::upcast> {
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
template <> struct operation<semantics::step::copy> {
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
template <typename Target> struct operation<semantics::step::wrap, Target> {
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
template <> struct operation<semantics::step::decay> {
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
template <> struct operation<semantics::step::hand_over> {
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
struct observe_operation {
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
struct adopt_operation {
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
template <std::uint8_t Quals> struct qualify_operation {
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
struct operation_result {
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
struct operation_result<
    operation<semantics::step::copy>, Leaf, Interface, T, Owned,
    std::enable_if_t<std::is_object_v<T> && !std::is_array_v<T>>> {
  using made_type = std::remove_cv_t<T>;
  static constexpr bool possible = std::is_constructible_v<made_type, T &>;
  using type = carrier<made_type, true>;
};

template <typename Leaf, typename Interface, typename T, bool Owned>
struct operation_result<operation<semantics::step::decay>, Leaf, Interface, T,
                        Owned, std::enable_if_t<std::is_array_v<T>>> {
  static constexpr bool possible = true;
  using type = carrier<std::add_pointer_t<std::remove_extent_t<T>>, Owned>;
};

template <typename Leaf, typename Interface, typename T, bool Owned>
struct operation_result<operation<semantics::step::hand_over>, Leaf, Interface,
                        T, Owned> {
  static constexpr bool possible = true;
  using type = carrier<T *, Owned>;
};

template <typename Leaf, typename Interface, typename T, bool Owned>
struct operation_result<operation<semantics::step::transfer>, Leaf, Interface,
                        T, Owned,
                        std::void_t<typename type_traits<T>::template share_t<
                            typename type_traits<T>::value_type>>> {
  using made_type = typename type_traits<T>::template share_t<
      typename type_traits<T>::value_type>;
  static constexpr bool possible =
      Owned && std::is_constructible_v<made_type, T &&>;
  using type = carrier<made_type, true>;
};

template <typename Leaf, typename Interface, typename T, bool Owned>
struct operation_result<
    operation<semantics::step::unknown_bound>, Leaf, Interface, T, Owned,
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
struct operation_result<
    observe_operation<Target, Quals>, Leaf, Interface, T, Owned,
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
struct operation_result<adopt_operation<Target, Array, Quals>, Leaf, Interface,
                        T, Owned, std::enable_if_t<std::is_pointer_v<T>>> {
  using pointee = held_t<std::remove_pointer_t<T>, Quals>;
  static constexpr bool whole =
      !Array && is_array_like_type_v<Target> && std::is_array_v<pointee>;
  using pointer = std::conditional_t<whole, std::remove_extent_t<pointee> *, T>;
  template <typename U, typename = void> struct made {
    static constexpr bool possible = false;
    using type = void;
  };
  // A handle its type traits do not describe, as of an array whose deletion
  // they do not declare, is not made.
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
struct operation_result<
    qualify_operation<Quals>, Leaf, Interface, T, Owned,
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
// request: a store that keeps them apart gives each with part<Stable>().
template <bool Stable, typename Store>
decltype(auto) lifetime_store(Store &store) {
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
template <typename Step, std::uint8_t Quals> struct holding_operation;
template <typename Target, std::uint8_t Given, std::uint8_t Quals>
struct holding_operation<observe_operation<Target, Given>, Quals> {
  using type = observe_operation<Target, Quals>;
};
template <typename Target, bool Array, std::uint8_t Given, std::uint8_t Quals>
struct holding_operation<adopt_operation<Target, Array, Given>, Quals> {
  using type = adopt_operation<Target, Array, Quals>;
};

// What a derivation makes lives as long as the container when it is kept with
// the binding, under a key of its type.
template <typename T> inline constexpr char retained_key = 0;

// The C++ properties of a type T toward the leaf Leaf, of the interface
// Interface, as the steps of the semantics apply to it.
template <typename T, typename Leaf, typename Interface> struct layer_types {
  using type = std::remove_cv_t<T>;

  // Whether a value of U can be copied, of any qualifiers.
  template <typename U> static constexpr bool copyable() {
    using value_type = std::remove_cv_t<U>;
    // NOLINTBEGIN(bugprone-branch-clone)
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
    // NOLINTEND(bugprone-branch-clone)
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
    // NOLINTBEGIN(bugprone-branch-clone)
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
    // NOLINTEND(bugprone-branch-clone)
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
  // NOLINTBEGIN(bugprone-branch-clone)
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
  // NOLINTEND(bugprone-branch-clone)
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
