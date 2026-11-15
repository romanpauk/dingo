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

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

// The execution of a plan of the semantics (dingo/semantics/model.h) on C++
// objects: one step for each kind of step of the semantics, each by the type
// traits of its layers and by C++ itself, and the interpreter that applies
// the steps of a plan one after another, then satisfies the request of what
// they reach. What a step makes for the request is kept by a keeper, which
// constructs it in place of what makes it and keeps it while the request
// lives: keep<T>(make) returns a T & of T(make()).

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
using qualified_as_t = std::conditional_t<
    std::is_const_v<T>,
    std::conditional_t<std::is_volatile_v<T>, const volatile U, const U>,
    std::conditional_t<std::is_volatile_v<T>, volatile U, U>>;

// The element type of a layer of type T, toward Leaf, as the type declares
// it: what a pointer points to; what a wrapper holds, the array a handle of
// an array holds, of the bound its type traits give; the alternative of an
// alternative type that holds the leaf or its interface. An element held in
// place is of the qualifiers of the layer too.
template <typename T, typename Leaf> struct element_of {
  using layer_type = std::remove_cv_t<T>;
  static auto make() {
    if constexpr (std::is_pointer_v<layer_type>) {
      return type_tag<std::remove_pointer_t<layer_type>>{};
    } else if constexpr (std::is_array_v<layer_type>) {
      return type_tag<std::remove_extent_t<T>>{};
    } else if constexpr (is_alternative_type_v<layer_type>) {
      return type_tag<qualified_as_t<
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
        return type_tag<qualified_as_t<T, element>>{};
      }
    }
  }
  template <typename U> struct type_tag {
    using type = U;
  };
  using type = typename decltype(make())::type;
};

template <typename T, typename Leaf>
using element_of_t = typename element_of<T, Leaf>::type;

// The address of the element of a layer, or null when it holds none.
template <typename T, typename Leaf>
std::add_pointer_t<element_of_t<T, Leaf>> element_address(T &layer) {
  using layer_type = std::remove_cv_t<T>;
  using element = element_of_t<T, Leaf>;
  // The layer is only read through; what its element is declares whether
  // the element may be changed.
  auto &plain = const_cast<layer_type &>(layer);
  if constexpr (std::is_pointer_v<layer_type>) {
    return plain;
  } else if constexpr (is_alternative_type_v<layer_type>) {
    return type_traits<layer_type>::template get<
        element_of_t<layer_type, Leaf>>(plain);
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

// Whether a keeper keeps what lives as long as the container apart.
template <typename Keeper, typename = void>
struct has_keeper_at : std::false_type {};

template <typename Keeper>
struct has_keeper_at<
    Keeper, std::void_t<decltype(std::declval<Keeper &>().template at<true>())>>
    : std::true_type {};

// A step of a kind, to the layer Target where it makes one.
template <semantics::step Kind, typename Target = void> struct step;

// The element of a layer.
// What a pointer the request owns points to, released: the request owns it,
// and what keeps it deletes it as the request ends, as an array when the
// pointer is to the first element of one.
template <typename T, bool Array> struct released_pointer {
  T *pointer;
  explicit released_pointer(T *of) : pointer(of) {}
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
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    static_assert(Owned && std::is_pointer_v<T>,
                  "only a pointer the request owns is released");
    using pointee = std::remove_pointer_t<T>;
    T pointer = *from.object;
    keeper.template keep<released_pointer<pointee, Array>>(
        [&] { return released_pointer<pointee, Array>(pointer); });
    // What it points to is released from what it was taken out of.
    from.released();
    return carrier<pointee, true>{pointer};
  }
};

// The element of a layer. A request of the element of an empty layer
// fails: the keeper rejects it.
template <> struct step<semantics::step::element> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    using element = element_of_t<T, Leaf>;
    carrier<element, Owned && owns_alone<T>()> reached{
        element_address<T, Leaf>(*from.object), from.owner, from.release};
    if (reached.object == nullptr) {
      keeper.absent();
    }
    if constexpr (Owned && releases_element_v<T>) {
      // The element of a handle the request owns is handed over with it.
      reached.owner = from.object;
      reached.release = &release_element<T>;
    }
    return reached;
  }
};

// A pointer to the element of a layer, null when it holds none: to its first
// element when it is an array.
template <> struct step<semantics::step::element_pointer> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    using element = element_of_t<T, Leaf>;
    using pointer = std::add_pointer_t<std::remove_extent_t<element>>;
    auto &made = keeper.template keep<pointer>([&] {
      return reinterpret_cast<pointer>(element_address<T, Leaf>(*from.object));
    });
    return carrier<pointer, false>{std::addressof(made)};
  }
};

// The element of a layer that may be empty, which holds a value that may be
// empty itself: that value, or an empty one when the layer is empty.
template <> struct step<semantics::step::element_or_empty> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    using element = element_of_t<T, Leaf>;
    constexpr bool owned = Owned && owns_alone<T>();
    if (auto *held = element_address<T, Leaf>(*from.object)) {
      return carrier<element, owned>{held};
    }
    using value_type = std::remove_cv_t<element>;
    return carrier<element, owned>{
        std::addressof(keeper.template keep<value_type>(
            [] { return type_traits<value_type>::empty_value(); }))};
  }
};

// A copy of what element_or_empty gives.
template <> struct step<semantics::step::element_or_empty_copy> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    using element = std::remove_cv_t<element_of_t<T, Leaf>>;
    auto &made = keeper.template keep<element>(defer<Leaf, Interface>(from));
    return carrier<element, true>{std::addressof(made)};
  }

  // What makes the copy.
  template <typename Leaf, typename Interface, typename T, bool Owned>
  static auto defer(carrier<T, Owned> from) {
    using element = std::remove_cv_t<element_of_t<T, Leaf>>;
    auto *held = element_address<T, Leaf>(*from.object);
    return [held]() -> element {
      if (held) {
        return *held;
      }
      return type_traits<element>::empty_value();
    };
  }
};

// The sharing handle an owned sole owner converts to, taking over its
// element: the share_t of its type traits.
template <> struct step<semantics::step::convert> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    static_assert(Owned, "only an owned value converts");
    using made_type = typename type_traits<T>::template share_t<
        typename type_traits<T>::value_type>;
    auto &made = keeper.template keep<made_type>(
        [&] { return made_type(std::move(*from.object)); });
    return carrier<made_type, true>{std::addressof(made)};
  }
};

// The handle of an array of unknown bound a handle of an array of a known
// bound converts to, holding the same array: the unbound_t of its type
// traits, moved out of one the request owns or copied of another.
template <> struct step<semantics::step::unbound> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    using type = std::remove_cv_t<T>;
    using made_type = typename type_traits<type>::template unbound_t<
        typename type_traits<type>::value_type>;
    auto &made = keeper.template keep<made_type>([&] {
      if constexpr (Owned) {
        return made_type(std::move(*from.object));
      } else {
        return made_type(*from.object);
      }
    });
    return carrier<made_type, true>{std::addressof(made)};
  }
};

// The type of a stack with its leaf replaced by Interface.
// The leaf keeps its qualifiers as its interface; a new pointer or handle
// is of none.
template <typename T, typename Leaf, typename Interface> struct upcast_of {
  using layer_type = std::remove_cv_t<T>;
  static auto make() {
    if constexpr (std::is_same_v<layer_type, Leaf>) {
      return type_tag<qualified_as_t<T, Interface>>{};
    } else if constexpr (std::is_pointer_v<layer_type>) {
      return type_tag<typename upcast_of<std::remove_pointer_t<layer_type>,
                                         Leaf, Interface>::type *>{};
    } else {
      return type_tag<typename type_traits<layer_type>::template rebind_t<
          typename upcast_of<typename type_traits<layer_type>::value_type, Leaf,
                             Interface>::type>>{};
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
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    if constexpr (std::is_same_v<std::remove_cv_t<T>, Leaf>) {
      using interface = qualified_as_t<T, Interface>;
      return carrier<interface, Owned>{static_cast<interface *>(from.object),
                                       from.owner, from.release};
    } else if constexpr (std::is_pointer_v<T>) {
      // A pointer of the interface, to the same object: a pointer the request
      // owns stays owned, and still releases what it was taken out of.
      using made_type = typename upcast_of<T, Leaf, Interface>::type;
      auto &made = keeper.template keep<made_type>(
          [&] { return made_type(*from.object); });
      return carrier<made_type, Owned>{std::addressof(made), from.owner,
                                       from.release};
    } else {
      // A handle, or a stack of layers, of the interface: converted of the
      // one the request owns, which is moved, or copied, as a new object the
      // request owns.
      using made_type = typename upcast_of<T, Leaf, Interface>::type;
      auto &made = keeper.template keep<made_type>([&]() -> made_type {
        if constexpr (Owned) {
          return made_type(std::move(*from.object));
        } else {
          return made_type(*from.object);
        }
      });
      return carrier<made_type, Owned || std::is_copy_constructible_v<T>>{
          std::addressof(made)};
    }
  }
};

// A copy: a new object the request owns.
template <> struct step<semantics::step::copy> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    using made_type = std::remove_cv_t<T>;
    auto &made = keeper.template keep<made_type>(defer<Leaf, Interface>(from));
    return carrier<made_type, true>{std::addressof(made)};
  }

  // What makes the copy: an object of no qualifiers.
  template <typename Leaf, typename Interface, typename T, bool Owned>
  static auto defer(carrier<T, Owned> from) {
    using made_type = std::remove_cv_t<T>;
    return [object = from.object]() -> made_type { return made_type(*object); };
  }
};

// A new layer of the layer Target holding what the request owns, moved into
// it.
template <typename Target> struct step<semantics::step::wrap, Target> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    using made_type = wrapped_t<Leaf, Interface, T>;
    auto &made = keeper.template keep<made_type>(
        [&] { return wrapped<Leaf, Interface>(std::move(*from.object)); });
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
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    using pointer = std::add_pointer_t<std::remove_extent_t<T>>;
    auto &made = keeper.template keep<pointer>(
        [&] { return reinterpret_cast<pointer>(from.object); });
    if constexpr (Owned) {
      // The pointer of an array the request owns owns it: it is given up by
      // what it was taken out of.
      from.released();
    }
    return carrier<pointer, Owned>{std::addressof(made)};
  }
};

// An array the request owns, handed over as the pointer that owns it.
template <> struct step<semantics::step::hand_over> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    auto &made = keeper.template keep<T *>([&] { return from.object; });
    // The pointer owns the array now.
    from.released();
    return carrier<T *, Owned>{std::addressof(made)};
  }
};

// A type with qualifiers of its own, as a mask of the qualifiers of the
// semantics.
template <typename T, std::uint8_t Quals>
using with_quals_t = std::conditional_t<
    (Quals & semantics::volatile_qual) != 0,
    std::add_volatile_t<std::conditional_t<(Quals & semantics::const_qual) != 0,
                                           std::add_const_t<T>, T>>,
    std::conditional_t<(Quals & semantics::const_qual) != 0,
                       std::add_const_t<T>, T>>;

// The pointer or the handle T of the same element of the qualifiers Quals.
template <typename T, std::uint8_t Quals, typename = void> struct requalified {
  using element = std::remove_cv_t<typename type_traits<T>::value_type>;
  using type =
      typename type_traits<T>::template rebind_t<with_quals_t<element, Quals>>;
};

template <typename T, std::uint8_t Quals>
struct requalified<T, Quals, std::enable_if_t<std::is_pointer_v<T>>> {
  using type =
      with_quals_t<std::remove_cv_t<std::remove_pointer_t<T>>, Quals> *;
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
using held_of_t =
    std::conditional_t<Quals == made_quals, T,
                       with_quals_t<std::remove_cv_t<T>, Quals & 3>>;

template <typename Target, std::uint8_t Quals = made_quals>
struct observe_step {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    using pointee =
        held_of_t<std::remove_pointer_t<std::remove_cv_t<T>>, Quals>;
    using made_type = typename type_traits<Target>::template rebind_t<pointee>;
    auto &made = keeper.template keep<made_type>(
        [&] { return made_type(*from.object); });
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
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    using pointee = held_of_t<std::remove_pointer_t<T>, Quals>;
    using made_type = adopting_handle_t<Target, pointee, Array>;
    // A handle of an array takes over a pointer to the whole array as the
    // pointer to its first element.
    constexpr bool whole =
        !Array && is_array_like_type_v<Target> && std::is_array_v<pointee>;
    auto &made = keeper.template keep<made_type>(
        [&] { return made_type(adopted_pointer<whole>(*from.object)); });
    // The handle owns what the pointer points to now.
    from.released();
    return carrier<made_type, true>{std::addressof(made)};
  }
};

// A pointer or a handle converted to one of an element of the qualifiers
// Quals: a new object, moved out of one the request owns, copied of another.
template <std::uint8_t Quals> struct qualify_step {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    using made_type = requalified_t<T, Quals>;
    auto &made = keeper.template keep<made_type>([&]() -> made_type {
      if constexpr (Owned) {
        return made_type(std::move(*from.object));
      } else {
        return made_type(*from.object);
      }
    });
    return carrier<made_type, true>{std::addressof(made)};
  }
};

// What the last steps of a plan make, not yet made: a run of wraps, or a
// copy. It is made where the request takes it, so that a value the request
// takes is made there at once; make returns it.
template <typename Made, typename Make, bool Stable = false> struct pending {
  using made_type = Made;
  static constexpr bool stable = Stable;
  Make make;
};

template <bool Stable, typename Make> auto pending_of(Make make) {
  return pending<decltype(make()), Make, Stable>{make};
}

template <typename T> inline constexpr bool is_pending_v = false;

template <typename Made, typename Make, bool Stable>
inline constexpr bool is_pending_v<pending<Made, Make, Stable>> = true;

// Whether a step makes a copy, which the last step of a plan makes where the
// request takes it.
template <typename Step> inline constexpr bool copies_v = false;
template <> inline constexpr bool copies_v<step<semantics::step::copy>> = true;
template <>
inline constexpr bool copies_v<step<semantics::step::element_or_empty_copy>> =
    true;

// A step of a plan, with whether what it makes lives as long as the
// container.
template <typename Step, bool Stable> struct planned {
  using type = Step;
  static constexpr bool stable = Stable;
};

// A step, and whether what it makes lives as long as the container: a step
// that is not planned makes what lives as long as the request.
template <typename Step> struct step_of_plan {
  using type = Step;
  static constexpr bool stable = false;
};

template <typename Step, bool Stable>
struct step_of_plan<planned<Step, Stable>> {
  using type = Step;
  static constexpr bool stable = Stable;
};

// The keeper of what lives as long as the container, or as long as the
// request: a keeper that keeps them apart gives each with at<Stable>().
template <bool Stable, typename Keeper> auto &keeper_of(Keeper &keeper) {
  if constexpr (has_keeper_at<Keeper>::value) {
    return keeper.template at<Stable>();
  } else {
    return keeper;
  }
}

// Whether a step wraps.
template <typename Step> inline constexpr bool is_wrap_v = false;

template <typename Target>
inline constexpr bool is_wrap_v<step<semantics::step::wrap, Target>> = true;

template <typename Step, bool Stable>
inline constexpr bool is_wrap_v<planned<Step, Stable>> = is_wrap_v<Step>;

// The type a run of wraps makes of a value of T, the first the innermost.
template <typename Leaf, typename Interface, typename T, typename... Wraps>
struct wrapped_stack {
  using type = T;
};

template <typename Leaf, typename Interface, typename T, typename Wrap,
          typename... Wraps>
struct wrapped_stack<Leaf, Interface, T, Wrap, Wraps...> {
  using type = typename wrapped_stack<
      Leaf, Interface,
      typename step_of_plan<Wrap>::type::template wrapped_t<Leaf, Interface, T>,
      Wraps...>::type;
};

// A layer made of what its element is made of, in place: the layer's emplace
// or wrap is given it when the element can be constructed of it, which the
// element then is in place; otherwise the element is made the same way
// first, a handle, which moves without its leaf. The leaf is moved once,
// however many layers it is moved into.
template <typename Leaf, typename Interface, typename Made, typename T>
Made made_of(T &&value) {
  using value_type = std::remove_reference_t<T>;
  if constexpr (std::is_same_v<Made, value_type>) {
    return Made(std::forward<T>(value));
  } else if constexpr (is_alternative_type_v<Made>) {
    using element = element_of_t<Made, Leaf>;
    if constexpr (std::is_same_v<element, value_type>) {
      return type_traits<Made>::template wrap<element>(std::forward<T>(value));
    } else {
      return type_traits<Made>::template wrap<element>(
          made_of<Leaf, Interface, element>(std::forward<T>(value)));
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
          made_of<Leaf, Interface, element>(std::forward<T>(value)));
    }
  }
}

// Whether a step converts to a more qualified element, and the qualifiers of
// the element it converts to.
template <typename Step> inline constexpr bool is_qualify_v = false;
template <std::uint8_t Quals>
inline constexpr bool is_qualify_v<qualify_step<Quals>> = true;

template <typename Step> inline constexpr std::uint8_t qualified_quals_v = 0;
template <std::uint8_t Quals>
inline constexpr std::uint8_t qualified_quals_v<qualify_step<Quals>> = Quals;
template <typename Step, bool Stable>
inline constexpr std::uint8_t qualified_quals_v<planned<Step, Stable>> =
    qualified_quals_v<Step>;

// Whether a step makes a layer of a pointer, which holds its element of the
// qualifiers given; and the step that holds it of the qualifiers Quals.
template <typename Step> inline constexpr bool makes_held_v = false;
template <typename Target, std::uint8_t Quals>
inline constexpr bool makes_held_v<observe_step<Target, Quals>> = true;
template <typename Target, bool Array, std::uint8_t Quals>
inline constexpr bool makes_held_v<adopt_step<Target, Array, Quals>> = true;

template <typename Step, std::uint8_t Quals> struct held_as;
template <typename Target, std::uint8_t Given, std::uint8_t Quals>
struct held_as<observe_step<Target, Given>, Quals> {
  using type = observe_step<Target, Quals>;
};
template <typename Target, bool Array, std::uint8_t Given, std::uint8_t Quals>
struct held_as<adopt_step<Target, Array, Given>, Quals> {
  using type = adopt_step<Target, Array, Quals>;
};

// The steps of a plan, applied one after another. A run of wraps makes its
// outermost layer at once.
template <typename Leaf, typename Interface> struct interpreter {
  template <typename Carrier, typename Keeper>
  static Carrier run(Carrier from, Keeper &) {
    return from;
  }

  template <typename... Steps, typename Carrier, typename Keeper>
  static auto run_list(type_list<Steps...>, Carrier from, Keeper &keeper) {
    return run<Steps...>(from, keeper);
  }

  template <typename Step, typename... Steps, typename Carrier, typename Keeper>
  static auto run(Carrier from, Keeper &keeper) {
    if constexpr (is_wrap_v<Step>) {
      return wrap_run<type_list<Step>, Steps...>(from, keeper);
    } else if constexpr (makes_held_v<typename step_of_plan<Step>::type> &&
                         starts_qualify<Steps...>()) {
      // A layer a request makes is made of the qualifiers its element needs
      // at once.
      return run_fused<Step, Steps...>(from, keeper);
    } else if constexpr (sizeof...(Steps) == 0 &&
                         copies_v<typename step_of_plan<Step>::type>) {
      // The last step copies: the copy is made where the request takes it.
      using of = step_of_plan<Step>;
      return pending_of<of::stable>(
          of::type::template defer<Leaf, Interface>(from));
    } else {
      using of = step_of_plan<Step>;
      return run<Steps...>(of::type::template apply<Leaf, Interface>(
                               from, keeper_of<of::stable>(keeper)),
                           keeper);
    }
  }

  template <typename Step, typename Qualify, typename... Steps,
            typename Carrier, typename Keeper>
  static auto run_fused(Carrier from, Keeper &keeper) {
    using of = step_of_plan<Step>;
    using fused =
        typename held_as<typename of::type, qualified_quals_v<Qualify>>::type;
    return run<Steps...>(fused::template apply<Leaf, Interface>(
                             from, keeper_of<of::stable>(keeper)),
                         keeper);
  }

  template <typename Wraps, typename... Steps, typename Carrier,
            typename Keeper>
  static auto wrap_run(Carrier from, Keeper &keeper) {
    if constexpr (starts_wrap<Steps...>()) {
      return wrap_more<Wraps, Steps...>(from, keeper);
    } else if constexpr (starts_qualify<Steps...>()) {
      return wrap_qualified<Wraps, Steps...>(from, keeper);
    } else {
      using made_type =
          typename wrapped_of<typename Carrier::type, Wraps>::type;
      return wrap_made<made_type, step_of_plan<last_of_t<Wraps>>::stable,
                       Steps...>(from, keeper);
    }
  }

  // A run of wraps made of the qualifiers its element needs at once.
  template <typename Wraps, typename Qualify, typename... Steps,
            typename Carrier, typename Keeper>
  static auto wrap_qualified(Carrier from, Keeper &keeper) {
    using made_type =
        requalified_t<typename wrapped_of<typename Carrier::type, Wraps>::type,
                      qualified_quals_v<Qualify>>;
    return wrap_made<made_type, step_of_plan<last_of_t<Wraps>>::stable,
                     Steps...>(from, keeper);
  }

  template <typename Made, bool Stable, typename... Steps, typename Carrier,
            typename Keeper>
  static auto wrap_made(Carrier from, Keeper &keeper) {
    if constexpr (sizeof...(Steps) == 0) {
      // The last steps: what they make is made where the request needs it.
      return pending_of<Stable>([from]() -> Made {
        return made_of<Leaf, Interface, Made>(std::move(*from.object));
      });
    } else {
      auto &made = keeper_of<Stable>(keeper).template keep<Made>([&] {
        return made_of<Leaf, Interface, Made>(std::move(*from.object));
      });
      return run<Steps...>(carrier<Made, true>{std::addressof(made)}, keeper);
    }
  }

  template <typename Wraps, typename Next, typename... Steps, typename Carrier,
            typename Keeper>
  static auto wrap_more(Carrier from, Keeper &keeper) {
    return wrap_run<typename appended<Wraps, Next>::type, Steps...>(from,
                                                                    keeper);
  }

  // Whether the steps begin with a conversion to a more qualified element.
  template <typename... Steps> static constexpr bool starts_qualify() {
    if constexpr (sizeof...(Steps) == 0) {
      return false;
    } else {
      return is_qualify_v<typename step_of_plan<
          type_list_element_t<0, type_list<Steps...>>>::type>;
    }
  }

  // Whether the steps begin with a wrap.
  template <typename... Steps> static constexpr bool starts_wrap() {
    if constexpr (sizeof...(Steps) == 0) {
      return false;
    } else {
      return is_wrap_v<type_list_element_t<0, type_list<Steps...>>>;
    }
  }

  template <typename List> struct last_of;
  template <typename... Items> struct last_of<type_list<Items...>> {
    using type = type_list_element_t<sizeof...(Items) - 1, type_list<Items...>>;
  };
  template <typename List> using last_of_t = typename last_of<List>::type;

  template <typename List, typename Next> struct appended;
  template <typename... Items, typename Next>
  struct appended<type_list<Items...>, Next> {
    using type = type_list<Items..., Next>;
  };

  template <typename T, typename Wraps> struct wrapped_of;
  template <typename T, typename... Wraps>
  struct wrapped_of<T, type_list<Wraps...>>
      : wrapped_stack<Leaf, Interface, T, Wraps...> {};
};

// The request satisfied of what a plan reaches.
template <semantics::satisfaction How, typename Request, typename Leaf,
          typename Interface, typename T, bool Owned, typename Keeper>
Request satisfy(carrier<T, Owned> reached, Keeper &) {
  if constexpr (How == semantics::satisfaction::refer) {
    if constexpr (std::is_pointer_v<Request>) {
      return reached.object;
    } else {
      return static_cast<Request>(*reached.object);
    }
  } else if constexpr (How == semantics::satisfaction::point) {
    return *reached.object;
  } else if constexpr (How == semantics::satisfaction::take) {
    return static_cast<Request>(std::move(*reached.object));
  } else if constexpr (How == semantics::satisfaction::move) {
    return Request(std::move(*reached.object));
  } else {
    return Request(*reached.object);
  }
}
// The request satisfied of what the last steps of a plan make.
template <semantics::satisfaction How, typename Request, typename Leaf,
          typename Interface, typename Made, typename Make, bool Stable,
          typename Keeper>
Request satisfy(pending<Made, Make, Stable> reached, Keeper &keeper) {
  if constexpr (How == semantics::satisfaction::move) {
    // A value is made where it is returned.
    return Request(reached.make());
  } else {
    auto &made = keeper_of<Stable>(keeper).template keep<Made>(reached.make);
    return satisfy<How, Request, Leaf, Interface>(
        carrier<Made, true>{std::addressof(made)}, keeper);
  }
}

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
template <typename T> struct interface_of {
  template <typename U> struct tag {
    using type = U;
  };
  static auto make() {
    using type = std::remove_cv_t<T>;
    if constexpr (std::is_array_v<type>) {
      return tag<std::remove_cv_t<type>>{};
    } else if constexpr (std::is_pointer_v<type>) {
      return tag<typename interface_of<std::remove_pointer_t<type>>::type>{};
    } else if constexpr (type_traits<type>::enabled) {
      return tag<typename interface_of<
          typename type_traits<type>::value_type>::type>{};
    } else {
      return tag<type>{};
    }
  }
  using type = typename decltype(make())::type;
};

template <typename T> using interface_of_t = typename interface_of<T>::type;

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

constexpr semantics::element element_of_stack(stack_element of) {
  return of == stack_element::interface ? semantics::element::interface : semantics::element::leaf;
}

// What a binding of a stored object publishes, planned as the code compiles,
// of no request: the layer sources of its stored object, in a universe of the
// layers it takes steps into, those it stores and those of the handles a
// pointer it makes may be taken over by. The stored object is made for the
// request by a scope that makes its instance (Makes), or kept, and it is
// published early by a scope that does (Early).
template <typename Stored, typename Leaf, bool Makes, bool Early>
struct binding_plan {
  using stored_stack = stack_of_t<Stored, Leaf>;
  using layers = typename joined<
      typename stored_stack::layers,
      typename owning_layers<typename owning_handles<
          typename owning_handles_key<Leaf>::type>::type>::type>::type;
  using universe_type = layer_universe<layers>;

  static constexpr semantics::context context(bool abstract) {
    return {leaf_traits_of<Leaf>(),
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

  static constexpr semantics::thing start() {
    semantics::thing made;
    place_layers<universe_type>(made.layers, typename stored_stack::layers{});
    made.depth = static_cast<std::uint8_t>(
        type_list_size_v<typename stored_stack::layers>);
    made.of = element_of_stack(stored_stack::element);
    quals_of<Stored, Leaf>::fill(made.quals);
    for (std::size_t index = 0; index <= made.depth; ++index) {
      made.lifetimes[index] =
          Makes ? semantics::lifetime::request : semantics::lifetime::stable;
    }
    made.from = Makes ? semantics::origin::made : semantics::origin::borrowed;
    made.instance = true;
    // A pointer a scope makes owns what it points to.
    made.owns_pointee =
        Makes && made.depth && made.layers[0] == semantics::pointer_layer;
    return made;
  }

  // What the binding publishes for a request of the leaf, and for one of its
  // abstract interface. Each is made once.
  template <bool Abstract>
  static constexpr semantics::search sources =
      semantics::layer_sources(start(), context(Abstract));
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
  using request_stack = stack_of_t<shape, Leaf>;
  using universe_type =
      layer_universe<typename joined<typename binding::layers,
                                     typename request_stack::layers>::type>;
  using identities = typename universe_type::identities;
  using interface_type = innermost_t<shape, Leaf>;

  static constexpr bool abstract =
      request_stack::element == stack_element::interface;

  static constexpr bool modeled =
      category::modeled && stored_stack::element != stack_element::other &&
      request_stack::element != stack_element::other;

  static constexpr semantics::context context() {
    semantics::context with = binding::context(abstract);
    with.layers = &universe_type::value;
    return with;
  }

  static constexpr semantics::request goal() {
    semantics::request made;
    place_layers<universe_type>(made.layers, typename request_stack::layers{});
    made.depth = static_cast<std::uint8_t>(
        type_list_size_v<typename request_stack::layers>);
    made.of = element_of_stack(request_stack::element);
    quals_of<shape, Leaf>::fill(made.quals);
    return made;
  }

  static constexpr semantics::plan make(semantics::lifetime needed) {
    if constexpr (!modeled) {
      (void)needed;
      return {};
    } else {
      constexpr semantics::context with = context();
      const semantics::search found = semantics::reach_from(
          binding::template sources<abstract>, with, goal());
      return semantics::plan_of(
          found,
          semantics::satisfy(found, goal(), category::value, needed, with));
    }
  }

  // The plans of a request that needs what it refers to to live as long as
  // the container, and as long as the request.
  static constexpr semantics::plan stable = make(semantics::lifetime::stable);
  static constexpr semantics::plan transient =
      make(semantics::lifetime::request);
};

// The C++ step of a step of the semantics, from the thing At::from() to the
// thing At::reached(), to the layers of a universe of identities.
template <typename Identities, typename At> struct typed_step_at {
  static constexpr semantics::step kind = At::kind;
  static constexpr semantics::thing reached = At::reached();
  static constexpr bool stable =
      reached.lifetimes[0] == semantics::lifetime::stable;

  template <semantics::layer Layer>
  using layer_type = type_list_element_t<Layer - 4, Identities>;

  static auto make() {
    if constexpr (kind == semantics::step::wrap) {
      return step<semantics::step::wrap, layer_type<reached.layers[0]>>{};
    } else if constexpr (kind == semantics::step::release) {
      // The pointer is to the first element of an array when the step
      // before decayed or handed over an array.
      return release_step<At::from().layers[0] ==
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
  using type = planned<step_type, stable>;
};

// The step at an index of a plan.
template <const semantics::plan &Plan, std::size_t Index> struct plan_at {
  static constexpr semantics::step kind = Plan.steps[Index];
  static constexpr semantics::thing from() { return Plan.things[Index]; }
  static constexpr semantics::thing reached() { return Plan.things[Index + 1]; }
};

// The step into a state of a search.
template <const semantics::search &Search, std::size_t State> struct search_at {
  static constexpr semantics::step kind = Search.states[State].via;
  static constexpr semantics::thing from() {
    return Search.states[Search.states[State].parent].reached;
  }
  static constexpr semantics::thing reached() {
    return Search.states[State].reached;
  }
};

template <const semantics::plan &Plan, typename Identities, std::size_t Index>
using typed_step = typed_step_at<Identities, plan_at<Plan, Index>>;

template <const semantics::plan &Plan, typename Identities, typename Indexes>
struct typed_steps;

template <const semantics::plan &Plan, typename Identities, std::size_t... I>
struct typed_steps<Plan, Identities, std::index_sequence<I...>> {
  using type = type_list<typename typed_step<Plan, Identities, I>::type...>;
};

// The C++ steps of a plan.
template <const semantics::plan &Plan, typename Identities>
using plan_steps_t =
    typename typed_steps<Plan, Identities,
                         std::make_index_sequence<Plan.count>>::type;

// The C++ steps of the plan of a request_plan for a lifetime: as long as the
// container (Stable), or as long as the request. Only that plan is made.
template <typename Plan, bool Stable> struct plan_steps_of {
  using type = plan_steps_t<Plan::transient, typename Plan::identities>;
};

template <typename Plan> struct plan_steps_of<Plan, true> {
  using type = plan_steps_t<Plan::stable, typename Plan::identities>;
};

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
    using type = element_of_t<T, Leaf>;
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
  template <typename U> static constexpr bool made_of_value() {
    using layer_type = std::remove_cv_t<U>;
    if constexpr (std::is_pointer_v<layer_type> ||
                  std::is_array_v<layer_type> || !is_complete_v<layer_type>) {
      return false;
    } else if constexpr (is_alternative_type_v<layer_type>) {
      return true;
    } else if constexpr (type_traits<layer_type>::enabled &&
                         !is_array_like_type_v<layer_type>) {
      return wrapper_kind_v<layer_identity_t<layer_type>>.made_from_value;
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
    } else if constexpr (!made_of_value<U>()) {
      return false;
    } else {
      return runs<K - 1, typename layer_types<
                             U, Leaf, Interface>::template element<>::type>();
    }
  }
};

// Whether a handle of an array is made of a value, as an array of it its
// type_traits emplaces.
template <typename Target, typename Value> constexpr bool emplaces_array_of() {
  if constexpr (type_traits<Target>::enabled && is_array_like_type_v<Target>) {
    return can_emplace_v<Target, Value &&>;
  } else {
    return false;
  }
}

// An object of type Target made of a value, Value: constructed of it, a
// handle of an array of it, or a wrapper made of it at once.
template <typename Target, typename Value> constexpr bool makes_as() {
  if constexpr (std::is_constructible_v<Target, Value &&>) {
    return true;
  } else if constexpr (emplaces_array_of<Target, Value>()) {
    return true;
  } else if constexpr (std::is_pointer_v<Target> || std::is_array_v<Target> ||
                       !type_traits<Target>::enabled) {
    return false;
  } else {
    constexpr std::size_t depth =
        type_list_size_v<typename stack_of_t<Target, Value>::layers>;
    return depth != 0 &&
           layer_types<Target, Value, Value>::template runs<depth>();
  }
}

// An object of type Target made of what a storage or a factory is given: a
// pointer to it, or the object makes_as makes of it.
template <typename Target, typename Source> Target made_as(Source &&source) {
  using value_type = remove_cvref_t<Source>;
  if constexpr (std::is_constructible_v<Target, Source &&>) {
    return Target(std::forward<Source>(source));
  } else if constexpr (std::is_pointer_v<Target> &&
                       std::is_lvalue_reference_v<Source>) {
    return std::addressof(source);
  } else if constexpr (emplaces_array_of<Target, value_type>()) {
    return type_traits<Target>::emplace(std::forward<Source>(source));
  } else {
    return made_of<value_type, value_type, Target>(
        std::forward<Source>(source));
  }
}

} // namespace dingo::detail::execution
