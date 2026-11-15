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
    return carrier<pointer, Owned>{std::addressof(made), from.owner,
                                   from.release};
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

// A handle of the layer Target that does not own its element, made of a raw
// pointer to it.
template <typename Target> struct observe_step {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    using pointee = std::remove_pointer_t<std::remove_cv_t<T>>;
    using made_type = typename type_traits<Target>::template rebind_t<pointee>;
    auto &made = keeper.template keep<made_type>(
        [&] { return made_type(*from.object); });
    return carrier<made_type, true>{std::addressof(made)};
  }
};

// A raw pointer the request owns, taken over by a handle of the layer
// Target: of an array when it points to the first element of one.
template <typename Target, bool Array> struct adopt_step {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    using pointee = std::remove_pointer_t<T>;
    using made_type = typename type_traits<Target>::template rebind_t<
        std::conditional_t<Array, pointee[], pointee>>;
    auto &made = keeper.template keep<made_type>(
        [&] { return made_type(*from.object); });
    // The handle owns what the pointer points to now.
    from.released();
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

  template <typename Wraps, typename... Steps, typename Carrier,
            typename Keeper>
  static auto wrap_run(Carrier from, Keeper &keeper) {
    if constexpr (starts_wrap<Steps...>()) {
      return wrap_more<Wraps, Steps...>(from, keeper);
    } else {
      using made_type =
          typename wrapped_of<typename Carrier::type, Wraps>::type;
      constexpr bool stable = step_of_plan<last_of_t<Wraps>>::stable;
      if constexpr (sizeof...(Steps) == 0) {
        // The last steps: what they make is made where the request needs it.
        return pending_of<stable>([from]() -> made_type {
          return made_of<Leaf, Interface, made_type>(std::move(*from.object));
        });
      } else {
        auto &made = keeper_of<stable>(keeper).template keep<made_type>([&] {
          return made_of<Leaf, Interface, made_type>(std::move(*from.object));
        });
        return run<Steps...>(carrier<made_type, true>{std::addressof(made)},
                             keeper);
      }
    }
  }

  template <typename Wraps, typename Next, typename... Steps, typename Carrier,
            typename Keeper>
  static auto wrap_more(Carrier from, Keeper &keeper) {
    return wrap_run<typename appended<Wraps, Next>::type, Steps...>(from,
                                                                    keeper);
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
      typename owning_layers<typename owning_handles<>::type>::type>::type;
  using universe_type = layer_universe<layers>;

  static constexpr semantics::context context(bool abstract) {
    return {leaf_traits_of<Leaf>(),
            {Makes, false, Early},
            abstract,
            static_cast<std::uint8_t>(
                static_cast<unsigned>(
                    semantics::limitation::pointer_out_of_made_holder) |
                static_cast<unsigned>(
                    semantics::limitation::pointer_into_converted_copy)),
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

// A carrier of no type, which the runtime container passes between the
// operations of a plan: the object, whether the request owns it, and what it
// was taken out of, with how to release it.
struct erased_carrier {
  void *object = nullptr;
  bool owned = false;
  void *owner = nullptr;
  void (*release)(void *owner) = nullptr;
};

template <typename T, bool Owned> erased_carrier erase(carrier<T, Owned> from) {
  return {const_cast<void *>(static_cast<const volatile void *>(from.object)),
          Owned, from.owner, from.release};
}

template <typename T, bool Owned>
carrier<T, Owned> unerase(erased_carrier from) {
  return {static_cast<T *>(from.object), from.owner, from.release};
}

// A keeper of a runtime resolution as an operation uses it: what it makes is
// kept as long as the container when it is stable, as long as the request
// otherwise.
template <typename Keeper> struct keeper_view {
  Keeper &keeper;
  bool stable;

  template <typename T, typename Make> T &keep(Make &&make) {
    if (stable) {
      return keeper.template keep_stable<T>(std::forward<Make>(make));
    }
    return keeper.template keep_transient<T>(std::forward<Make>(make));
  }

  [[noreturn]] void absent() { keeper.absent(); }
};

// An operation of a runtime resolution, of no type.
template <typename Keeper>
using step_function = erased_carrier (*)(erased_carrier from, bool stable,
                                         Keeper &keeper);

template <typename Keeper> struct type_table;

// An operation on an object of a type: for one the request owns and for one
// it does not, none where C++ does not apply it, and the table of the type
// of what it makes.
template <typename Keeper> struct operation {
  step_function<Keeper> owned = nullptr;
  step_function<Keeper> borrowed = nullptr;
  const type_table<Keeper> *made = nullptr;
};

inline constexpr std::size_t step_kinds =
    static_cast<std::size_t>(semantics::step::observe) + 1;

// What a runtime resolution does with an object of a type, as the steps of
// the semantics do it, one table for each type: the steps from an object of
// it, by their kind, and the release of a pointer to the first element of
// an array; and the steps that make one of it: of the value its nested type
// at a depth is, moved into all its layers at once; of a pointer it takes
// over to its element, or to the first element of its array; and of a
// pointer to its element it does not own.
template <typename Keeper> struct full_table {
  operation<Keeper> from[step_kinds] = {};
  operation<Keeper> release_array = {};
  operation<Keeper> made_of[semantics::max_layers + 1] = {};
  operation<Keeper> adopted = {};
  operation<Keeper> adopted_array = {};
  operation<Keeper> observing = {};
};

// Where an operation is in a table: a kind of step from the type, the
// release of a pointer to the first element of an array, the run of wraps
// that makes the type of its nested type at a depth, the adoption of a
// pointer to its element or to the first element of its array, and the
// observer of a pointer.
enum class slot : std::uint8_t {
  release_array = step_kinds,
  made_of,
  adopted = made_of + semantics::max_layers + 1,
  adopted_array,
  observing,
  count
};

constexpr std::uint8_t slot_of(semantics::step kind) {
  return static_cast<std::uint8_t>(kind);
}
constexpr std::uint8_t slot_of(slot at, std::size_t depth = 0) {
  return static_cast<std::uint8_t>(static_cast<std::size_t>(at) + depth);
}

template <typename Keeper> struct slot_entry {
  std::uint8_t at;
  operation<Keeper> step;
};

// A table as a runtime resolution keeps it: the operations it has, as many
// as there are.
template <typename Keeper> struct type_table {
  void *(*pointee)(void *pointer);
  const slot_entry<Keeper> *entries;
  std::size_t count;

  // The operation at a slot, or one of no function.
  const operation<Keeper> &operator[](std::uint8_t at) const {
    for (std::size_t index = 0; index < count; ++index) {
      if (entries[index].at == at) {
        return entries[index].step;
      }
    }
    static constexpr operation<Keeper> none{};
    return none;
  }
};

// The operation at a slot of a full table.
template <typename Keeper>
constexpr const operation<Keeper> &slot_in(const full_table<Keeper> &table,
                                           std::uint8_t at) {
  if (at < step_kinds) {
    return table.from[at];
  }
  if (at == slot_of(slot::release_array)) {
    return table.release_array;
  }
  if (at < slot_of(slot::adopted)) {
    return table.made_of[at - slot_of(slot::made_of)];
  }
  if (at == slot_of(slot::adopted)) {
    return table.adopted;
  }
  if (at == slot_of(slot::adopted_array)) {
    return table.adopted_array;
  }
  return table.observing;
}

template <typename Keeper> constexpr bool has(const operation<Keeper> &step) {
  return step.owned != nullptr || step.borrowed != nullptr;
}

// The operations a table of a binding has: a bit for each kind of step from
// its type, one for the release of a pointer to the first element of an
// array, and one for the steps that make an object of it.
constexpr std::uint32_t mask_of(semantics::step kind) {
  return std::uint32_t{1} << static_cast<unsigned>(kind);
}
inline constexpr std::uint32_t release_array_mask = std::uint32_t{1}
                                                    << step_kinds;
inline constexpr std::uint32_t makers_mask = std::uint32_t{1}
                                             << (step_kinds + 1);

// Whether what a step of a kind makes lives as long as the request, as the
// semantics give it: nothing kept with the binding is made of it.
constexpr bool makes_transient(semantics::step kind) {
  return kind == semantics::step::copy || kind == semantics::step::decay ||
         kind == semantics::step::hand_over ||
         kind == semantics::step::convert || kind == semantics::step::release;
}

// A keeper of what lives as long as the request.
template <typename Keeper> struct transient_view {
  Keeper &keeper;

  template <typename T, typename Make> T &keep(Make &&make) {
    return keeper.template keep_transient<T>(std::forward<Make>(make));
  }

  [[noreturn]] void absent() { keeper.absent(); }
};

// The steps that make an object of type Made of another.
// A run of wraps of a value the request owns: Made at once, of the T moved
// into it.
template <typename Made, typename Leaf, typename Interface, typename T,
          typename Keeper>
erased_carrier made_of_erased(erased_carrier from, bool stable,
                              Keeper &keeper) {
  keeper_view<Keeper> view{keeper, stable};
  auto &made = view.template keep<Made>([&] {
    return made_of<Leaf, Interface, Made>(
        std::move(*static_cast<T *>(from.object)));
  });
  return erase(carrier<Made, true>{std::addressof(made)});
}

// A raw pointer the request owns, taken over by the handle Made.
template <typename Made, typename Pointer, typename Keeper>
erased_carrier adopted_erased(erased_carrier from, bool stable,
                              Keeper &keeper) {
  keeper_view<Keeper> view{keeper, stable};
  auto &made = view.template keep<Made>(
      [&] { return Made(*static_cast<Pointer *>(from.object)); });
  // The handle owns what the pointer points to now.
  carrier<Pointer, true>{nullptr, from.owner, from.release}.released();
  return erase(carrier<Made, true>{std::addressof(made)});
}

// A handle Made that does not own its element, made of a pointer to it.
template <typename Made, typename Pointer, typename Keeper>
erased_carrier observing_erased(erased_carrier from, bool stable,
                                Keeper &keeper) {
  keeper_view<Keeper> view{keeper, stable};
  auto &made = view.template keep<Made>(
      [&] { return Made(*static_cast<Pointer *>(from.object)); });
  return erase(carrier<Made, true>{std::addressof(made)});
}

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
      return movable<U>();
    } else if constexpr (!made_of_value<U>()) {
      return false;
    } else {
      return runs<K - 1, typename layer_types<
                             U, Leaf, Interface>::template element<>::type>();
    }
  }
};

// The table of a type T, toward the leaf Leaf, of the interface Interface:
// an operation where C++ applies the step to it.
template <typename T, typename Leaf, typename Interface, typename Keeper,
          std::uint32_t Mask = 0>
struct table_of;

// The full table of a type, of which table_of keeps the operations it has.
template <typename T, typename Leaf, typename Interface, typename Keeper,
          std::uint32_t Mask>
struct table_maker : layer_types<T, Leaf, Interface> {
  using facts = layer_types<T, Leaf, Interface>;
  using type = typename facts::type;
  template <bool Layer = facts::layer()>
  using element = typename facts::template element<Layer>;
  template <std::size_t K> using nested = typename facts::template nested<K>;

  template <typename Step, bool Owned, bool Transient>
  static erased_carrier apply(erased_carrier from, bool stable,
                              Keeper &keeper) {
    if constexpr (Transient) {
      (void)stable;
      transient_view<Keeper> view{keeper};
      return erase(
          Step::template apply<Leaf, Interface>(unerase<T, Owned>(from), view));
    } else {
      keeper_view<Keeper> view{keeper, stable};
      return erase(
          Step::template apply<Leaf, Interface>(unerase<T, Owned>(from), view));
    }
  }

  template <typename Step, bool Owned>
  using made_t = typename decltype(Step::template apply<Leaf, Interface>(
      std::declval<carrier<T, Owned>>(),
      std::declval<transient_view<Keeper> &>()))::type;

  // An operation of a step of a kind, for an object the request owns, for
  // one it does not, or for both, where the table has it.
  template <semantics::step Kind, typename Step, bool Owned, bool Borrowed,
            std::uint32_t Bit = mask_of(Kind)>
  static constexpr operation<Keeper> step_of() {
    operation<Keeper> made;
    constexpr bool transient = makes_transient(Kind);
    if constexpr ((Mask & Bit) != 0 && Owned) {
      made.owned = &apply<Step, true, transient>;
      made.made = &table_of<made_t<Step, true>, Leaf, Interface, Keeper>::value;
    }
    if constexpr ((Mask & Bit) != 0 && Borrowed) {
      made.borrowed = &apply<Step, false, transient>;
      made.made =
          &table_of<made_t<Step, false>, Leaf, Interface, Keeper>::value;
    }
    return made;
  }

  template <semantics::step Kind, typename Step = execution::step<Kind>>
  static constexpr operation<Keeper> both() {
    return step_of<Kind, Step, true, true>();
  }
  template <semantics::step Kind, typename Step = execution::step<Kind>,
            std::uint32_t Bit = mask_of(Kind)>
  static constexpr operation<Keeper> owned() {
    return step_of<Kind, Step, true, false, Bit>();
  }

  template <std::size_t K> static constexpr operation<Keeper> run() {
    operation<Keeper> made;
    if constexpr (std::is_same_v<T, type> && facts::template runs<K>()) {
      using of = typename nested<K>::type;
      made.owned = &made_of_erased<type, Leaf, Interface, of, Keeper>;
    }
    return made;
  }

  template <std::size_t... K>
  static constexpr void runs_into(full_table<Keeper> &made,
                                  std::index_sequence<K...>) {
    ((made.made_of[K + 1] = run<K + 1>()), ...);
  }

  static constexpr bool wants(semantics::step kind) {
    return (Mask & mask_of(kind)) != 0;
  }

  // The steps from a layer to its element.
  static constexpr void elements(full_table<Keeper> &made) {
    using semantics::step;
    using of = typename element<>::type;
    auto at = [&](step kind) -> operation<Keeper> & {
      return made.from[static_cast<std::size_t>(kind)];
    };
    at(step::element) = both<step::element>();
    at(step::element_pointer) = both<step::element_pointer>();
    if constexpr (has_empty_value<std::remove_cv_t<of>>::value) {
      at(step::element_or_empty) = both<step::element_or_empty>();
      if constexpr (!std::is_pointer_v<type> &&
                    facts::template copyable<of>() && !std::is_array_v<of>) {
        at(step::element_or_empty_copy) = both<step::element_or_empty_copy>();
      }
    }
  }

  static constexpr full_table<Keeper> make() {
    full_table<Keeper> made;
    using semantics::step;
    auto at = [&](step kind) -> operation<Keeper> & {
      return made.from[static_cast<std::size_t>(kind)];
    };
    // Each operation the mask has, where C++ applies its step to the type.
    if constexpr (wants(step::copy)) {
      if constexpr (facts::template copyable<T>()) {
        at(step::copy) = both<step::copy>();
      }
    }
    if constexpr (wants(step::decay) || wants(step::hand_over)) {
      if constexpr (std::is_array_v<type> &&
                    is_complete_v<std::remove_extent_t<type>>) {
        at(step::decay) = both<step::decay>();
        at(step::hand_over) = owned<step::hand_over>();
      }
    }
    if constexpr (wants(step::element) || wants(step::element_pointer) ||
                  wants(step::element_or_empty) ||
                  wants(step::element_or_empty_copy)) {
      if constexpr (facts::layer()) {
        elements(made);
      }
    }
    if constexpr (wants(step::release) || (Mask & release_array_mask) != 0) {
      if constexpr (std::is_pointer_v<type> &&
                    std::is_object_v<std::remove_pointer_t<type>> &&
                    is_complete_v<std::remove_pointer_t<type>>) {
        at(step::release) = owned<step::release, release_step<false>>();
        made.release_array =
            owned<step::release, release_step<true>, release_array_mask>();
      }
    }
    if constexpr (wants(step::convert)) {
      if constexpr (converts()) {
        at(step::convert) = owned<step::convert>();
      }
    }
    if constexpr (wants(step::upcast)) {
      if constexpr (upcasts<true>() || upcasts<false>()) {
        at(step::upcast) = step_of<step::upcast, execution::step<step::upcast>,
                                   upcasts<true>(), upcasts<false>()>();
      }
    }
    if constexpr (std::is_same_v<T, type> && (Mask & makers_mask) != 0) {
      runs_into(made, std::make_index_sequence<semantics::max_layers>{});
      takes_pointers(made);
    }
    return made;
  }

  // Whether a type has an empty value of its own.
  template <typename U, typename = void>
  struct has_empty_value : std::false_type {};
  template <typename U>
  struct has_empty_value<U,
                         std::void_t<decltype(type_traits<U>::empty_value())>>
      : std::true_type {};

  // Whether an owned sole owner converts to the sharing handle of its family.
  template <typename U, typename = void> struct shared {
    using type = void;
  };
  template <typename U>
  struct shared<U, std::void_t<typename type_traits<U>::template share_t<
                       typename type_traits<U>::value_type>>> {
    using type = typename type_traits<U>::template share_t<
        typename type_traits<U>::value_type>;
  };

  static constexpr bool converts() {
    if constexpr (!std::is_same_v<T, type> || std::is_pointer_v<type> ||
                  std::is_array_v<type> || !is_complete_v<type>) {
      return false;
    } else if constexpr (!type_traits<type>::enabled ||
                         is_alternative_type_v<type>) {
      return false;
    } else if constexpr (std::is_void_v<typename shared<type>::type>) {
      return false;
    } else {
      return std::is_constructible_v<typename shared<type>::type, type &&>;
    }
  }

  // Whether a stack holds the leaf apart from it, in a pointer or a handle
  // that holds it apart, which a stack of the interface can hold too.
  template <typename U> static constexpr bool holds_leaf_apart() {
    using layer_type = std::remove_cv_t<U>;
    if constexpr (std::is_pointer_v<layer_type>) {
      using pointee = std::remove_cv_t<std::remove_pointer_t<layer_type>>;
      return std::is_same_v<pointee, Leaf> || holds_leaf_apart<pointee>();
    } else if constexpr (std::is_array_v<layer_type> ||
                         is_alternative_type_v<layer_type> ||
                         !type_traits<layer_type>::enabled) {
      return false;
    } else {
      using held =
          std::remove_cv_t<typename type_traits<layer_type>::value_type>;
      if constexpr (std::is_same_v<held, Leaf>) {
        return wrapper_kind_v<layer_identity_t<layer_type>>.shares_element &&
               !is_array_like_type_v<layer_type>;
      } else {
        return holds_leaf_apart<held>();
      }
    }
  }

  // Whether the leaf, or a pointer, a handle or a stack of them of it, is
  // upcast to the interface, of an object the request owns or does not.
  template <bool Owned> static constexpr bool upcasts() {
    if constexpr (std::is_same_v<Leaf, Interface>) {
      return false;
    } else if constexpr (std::is_same_v<type, Leaf>) {
      return true;
    } else if constexpr (!holds_leaf_apart<type>()) {
      return false;
    } else if constexpr (std::is_pointer_v<type>) {
      return true;
    } else {
      using made_type = typename upcast_of<T, Leaf, Interface>::type;
      if constexpr (Owned) {
        return std::is_constructible_v<made_type, T &&>;
      } else {
        return std::is_constructible_v<made_type, const T &>;
      }
    }
  }

  // The handle the type is, made of a pointer it takes over, or the
  // observer, of one it does not.
  static constexpr void takes_pointers(full_table<Keeper> &made) {
    if constexpr (type_traits<type>::enabled && !std::is_pointer_v<type> &&
                  !is_alternative_type_v<type> && is_complete_v<type>) {
      takes_pointers_of(made);
    }
  }

  static constexpr void takes_pointers_of(full_table<Keeper> &made) {
    if constexpr (is_complete_v<std::remove_extent_t<
                      typename type_traits<type>::value_type>>) {
      using of = typename element<>::type;
      if constexpr (type_traits<type>::is_owning_handle) {
        if constexpr (std::is_array_v<of>) {
          using pointer = std::remove_extent_t<of> *;
          if constexpr (std::is_constructible_v<type, pointer>) {
            made.adopted_array = {&adopted_erased<type, pointer, Keeper>,
                                  nullptr, nullptr};
          }
        } else if constexpr (std::is_constructible_v<type, of *>) {
          made.adopted = {&adopted_erased<type, of *, Keeper>, nullptr,
                          nullptr};
        }
      } else if constexpr (wrapper_kind_v<layer_identity_t<type>>.observes &&
                           std::is_constructible_v<type, of *>) {
        made.observing = {&observing_erased<type, of *, Keeper>,
                          &observing_erased<type, of *, Keeper>, nullptr};
      }
    }
  }

  static constexpr full_table<Keeper> full = make();

  static constexpr std::size_t count() {
    std::size_t made = 0;
    for (std::uint8_t at = 0; at < slot_of(slot::count); ++at) {
      made += has(slot_in(full, at)) ? 1 : 0;
    }
    return made;
  }

  // The slot of the n-th operation the table has.
  static constexpr std::uint8_t nth(std::size_t index) {
    for (std::uint8_t at = 0; at < slot_of(slot::count); ++at) {
      if (has(slot_in(full, at)) && index-- == 0) {
        return at;
      }
    }
    return 0;
  }

  template <std::size_t Count, std::size_t... I>
  static constexpr std::array<slot_entry<Keeper>, Count + 1>
  compact(std::index_sequence<I...>) {
    return {{{nth(I), slot_in(full, nth(I))}..., {}}};
  }
};

// The address a pointer of type T points to.
template <typename T> void *pointee_of(void *pointer) {
  return const_cast<void *>(static_cast<const volatile void *>(
      *static_cast<std::remove_cv_t<T> *>(pointer)));
}

template <typename T> constexpr void *(*pointee_function())(void *) {
  if constexpr (std::is_pointer_v<std::remove_cv_t<T>>) {
    return &pointee_of<T>;
  } else {
    return nullptr;
  }
}

template <typename T, typename Leaf, typename Interface, typename Keeper,
          std::uint32_t Mask>
struct table_of {
  using maker = table_maker<T, Leaf, Interface, Keeper, Mask>;
  static constexpr std::size_t count = maker::count();
  static constexpr std::array<slot_entry<Keeper>, count + 1> entries =
      maker::template compact<count>(std::make_index_sequence<count>{});
  static constexpr type_table<Keeper> value{pointee_function<T>(),
                                            entries.data(), count};
};

// A table of no operations: of the type of what a step makes that nothing is
// taken of but the address a pointer points to.
template <typename T, typename Leaf, typename Interface, typename Keeper>
struct table_of<T, Leaf, Interface, Keeper, 0> {
  static constexpr type_table<Keeper> value{pointee_function<T>(), nullptr, 0};
};

// The identity of a layer, as an address both sides of a runtime resolution
// compare: of the layers of C++, or of the identity of a wrapper.
struct array_pointer_layer_tag {};

template <typename T> inline constexpr char identity_tag_v = 0;

template <typename Tag> constexpr const void *identity_of() {
  if constexpr (is_language_tag_v<Tag> ||
                std::is_same_v<Tag, array_pointer_layer_tag>) {
    return &identity_tag_v<Tag>;
  } else {
    return &identity_tag_v<typename Tag::identity>;
  }
}

// The identities of the layers of C++, at their places in every universe.
inline constexpr const void *language_identities[4] = {
    &identity_tag_v<pointer_layer_tag>,
    &identity_tag_v<array_pointer_layer_tag>, &identity_tag_v<array_layer_tag>,
    &identity_tag_v<bounded_array_layer_tag>};

// A wrapper layer of a universe as a runtime resolution merges it with
// another: what it is, its identity, and the identity of the layer an owned
// value of it converts to, or none.
struct runtime_layer {
  semantics::layer_kind kind;
  const void *identity;
  const void *converts_to;
};

template <typename Identity> constexpr runtime_layer runtime_layer_of() {
  semantics::layer_kind kind = wrapper_kind_v<Identity>;
  kind.converts = 0;
  using target = typename shared_layer<Identity>::type;
  if constexpr (std::is_void_v<target>) {
    return {kind, &identity_tag_v<Identity>, nullptr};
  } else {
    return {kind, &identity_tag_v<Identity>, &identity_tag_v<target>};
  }
}

// The wrapper layers of a universe, in its order.
template <typename Identities> struct runtime_layers;
template <typename... Identities>
struct runtime_layers<type_list<Identities...>> {
  static constexpr std::size_t count = sizeof...(Identities);
  static constexpr runtime_layer value[count + 1] = {
      runtime_layer_of<Identities>()..., {}};
};

// A request as a runtime resolution plans it: its layers in a universe of
// its own, whose wrapper layers it gives, what it asks for, and the table of
// each type it spells, from its shape to its element, which make the layers
// it makes.
template <typename Keeper> struct request_layout {
  const runtime_layer *layers;
  std::size_t layer_count;
  semantics::request goal;
  semantics::category category;
  const type_table<Keeper> *const *tables;
};

// A binding as a runtime resolution plans it: its layer sources, the states
// of a search in its order, in a universe of its own, whose wrapper layers
// it gives, and the table of the type of each.
template <typename Keeper> struct binding_layout {
  const semantics::state *sources;
  std::size_t source_count;
  semantics::context with;
  const runtime_layer *layers;
  std::size_t layer_count;
  const type_table<Keeper> *const *tables;
};

// What the last step of a plan of a value makes, which the request makes
// where it takes the value: nothing, a copy, or the value its nested type at
// a depth is, moved into all the layers of its shape.
enum class last_step : std::uint8_t { none, copy, run };

// A plan as a runtime resolution runs it: the operations of the binding and
// of the request, each with whether what it makes lives as long as the
// container; how it satisfies the request; the last step a value request
// makes, and whether what it gives the request the address of lives as long
// as the container.
template <typename Keeper> struct runtime_plan {
  struct call {
    step_function<Keeper> owned;
    step_function<Keeper> borrowed;
    bool stable;
  };
  bool found = false;
  semantics::satisfaction how = semantics::satisfaction::refer;
  last_step last = last_step::none;
  std::uint8_t depth = 0;
  std::uint8_t quals = 0;
  bool stable_address = false;
  // The table of the type of what the plan reaches.
  const type_table<Keeper> *reached = nullptr;
  std::size_t count = 0;
  call calls[semantics::max_plan] = {};
};

// The plan of a request of a binding, for the lifetime it needs: of the
// universe of the binding, which the layers of the request it does not have
// follow, searched from the binding's layer sources; compiled into the
// operations of the tables of the types it steps through.
template <typename Keeper>
runtime_plan<Keeper> plan_runtime(const binding_layout<Keeper> &binding,
                                  const request_layout<Keeper> &request,
                                  semantics::lifetime needed) {
  runtime_plan<Keeper> made;
  semantics::universe merged = *binding.with.layers;
  const void *identities[semantics::max_kinds] = {};
  const void *targets[semantics::max_kinds] = {};
  for (std::size_t index = 0; index < 4; ++index) {
    identities[index] = language_identities[index];
  }
  for (std::size_t index = 0; index < binding.layer_count; ++index) {
    identities[4 + index] = binding.layers[index].identity;
    targets[4 + index] = binding.layers[index].converts_to;
  }
  semantics::layer mapped[semantics::max_kinds] = {0, 1, 2, 3};
  for (std::size_t index = 0; index < request.layer_count; ++index) {
    const runtime_layer &layer = request.layers[index];
    std::size_t at = 4;
    while (at < merged.count && identities[at] != layer.identity) {
      ++at;
    }
    if (at == merged.count) {
      if (merged.count == semantics::max_kinds) {
        return made;
      }
      identities[at] = layer.identity;
      targets[at] = layer.converts_to;
      merged.add(layer.kind);
    }
    mapped[4 + index] = static_cast<semantics::layer>(at);
  }
  for (std::size_t index = 4; index < merged.count; ++index) {
    merged.kinds[index].converts = 0;
    for (std::size_t to = 4; targets[index] != nullptr && to < merged.count;
         ++to) {
      if (identities[to] == targets[index]) {
        merged.kinds[index].converts |= std::uint32_t{1} << to;
      }
    }
  }

  semantics::context with = binding.with;
  with.layers = &merged;
  semantics::request goal = request.goal;
  for (std::size_t index = 0; index < goal.depth; ++index) {
    goal.layers[index] = mapped[goal.layers[index]];
  }
  goal.of =
      with.abstract ? semantics::element::interface : semantics::element::leaf;

  semantics::search found;
  for (std::size_t index = 0; index < binding.source_count; ++index) {
    found.add(binding.sources[index]);
  }
  semantics::reach_into(found, with, goal);
  const semantics::plan plan = semantics::plan_of(
      found, semantics::satisfy(found, goal, request.category, needed, with));
  if (!plan.found) {
    return made;
  }

  auto stable = [](const semantics::thing &of) {
    return of.lifetimes[0] == semantics::lifetime::stable;
  };
  auto add = [&](const operation<Keeper> &step, const semantics::thing &to) {
    made.calls[made.count++] = {step.owned, step.borrowed, stable(to)};
  };
  // The table of the type of what the plan reached so far.
  const type_table<Keeper> *table = binding.tables[0];
  std::size_t at = 1;
  while (at <= plan.count) {
    const semantics::step kind = plan.steps[at - 1];
    const semantics::thing &from = plan.things[at - 1];
    // A run of wraps is one step.
    std::size_t last = at;
    while (kind == semantics::step::wrap && last < plan.count &&
           plan.steps[last] == semantics::step::wrap &&
           (plan.states[last] < binding.source_count) ==
               (plan.states[at] < binding.source_count)) {
      ++last;
    }
    const semantics::thing &to = plan.things[last];
    const bool binding_step = plan.states[last] < binding.source_count;
    const std::uint8_t run = static_cast<std::uint8_t>(last - at + 1);
    // The type a layer the plan makes is of: a source of the binding, or a
    // type the request spells, by its depth in the request.
    const type_table<Keeper> *maker =
        binding_step ? binding.tables[plan.states[last]]
                     : request.tables[goal.depth - to.depth];
    const operation<Keeper> *step = nullptr;
    switch (kind) {
    case semantics::step::wrap:
      step = &(*maker)[slot_of(slot::made_of, run)];
      break;
    case semantics::step::adopt:
      step = &(*maker)[to.layers[1] == semantics::array_layer && to.depth > 1
                           ? slot_of(slot::adopted_array)
                           : slot_of(slot::adopted)];
      break;
    case semantics::step::observe:
      step = &(*maker)[slot_of(slot::observing)];
      break;
    case semantics::step::release:
      step = &(*table)[from.layers[0] == semantics::array_pointer_layer
                           ? slot_of(slot::release_array)
                           : slot_of(kind)];
      break;
    default:
      step = &(*table)[slot_of(kind)];
      break;
    }
    const bool final_step = last == plan.count;
    // The last step of a plan of a value makes it where the request takes
    // it: a copy, or a run of wraps of the shape of the request.
    if (final_step && !binding_step &&
        (plan.how == semantics::satisfaction::move ||
         plan.how == semantics::satisfaction::copy)) {
      if (kind == semantics::step::copy) {
        made.last = last_step::copy;
        made.quals = from.quals[0];
        break;
      }
      if (kind == semantics::step::wrap) {
        made.last = last_step::run;
        made.depth = run;
        break;
      }
    }
    add(*step, to);
    // The table of what a step of the binding makes is that of its source;
    // of what a step of a request makes, that its operation names.
    table =
        binding_step ? binding.tables[plan.states[last]]
        : (kind == semantics::step::wrap || kind == semantics::step::adopt ||
           kind == semantics::step::observe)
            ? maker
            : step->made;
    at = last + 1;
  }
  made.found = true;
  made.reached = table;
  made.how = plan.how;
  const semantics::thing &reached = plan.things[plan.count];
  made.stable_address =
      (plan.how == semantics::satisfaction::point
           ? reached.lifetimes[1]
           : reached.lifetimes[0]) == semantics::lifetime::stable;
  return made;
}

// The C++ types of the states of a search, and its steps between them: a
// state is of the class of the first state with its layers, its element, its
// qualifiers and its origin, and a step into a state is the first step of its
// kind between their classes.
struct state_classes {
  std::uint16_t of[semantics::max_states] = {};
  std::uint16_t edge_of[semantics::max_states] = {};
  std::uint16_t edges[semantics::max_states] = {};
  std::size_t edge_count = 0;
};

constexpr bool same_type(const semantics::thing &a, const semantics::thing &b) {
  if (a.depth != b.depth || a.of != b.of || a.from != b.from) {
    return false;
  }
  for (std::size_t index = 0; index < a.depth; ++index) {
    if (a.layers[index] != b.layers[index]) {
      return false;
    }
  }
  for (std::size_t index = 0; index <= a.depth; ++index) {
    if (a.quals[index] != b.quals[index]) {
      return false;
    }
  }
  return true;
}

constexpr std::uint32_t type_hash(const semantics::thing &of) {
  std::uint32_t hash = 2166136261u;
  hash = (hash ^ of.depth) * 16777619u;
  hash = (hash ^ static_cast<unsigned>(of.of)) * 16777619u;
  hash = (hash ^ static_cast<unsigned>(of.from)) * 16777619u;
  for (std::size_t index = 0; index < of.depth; ++index) {
    hash = (hash ^ of.layers[index]) * 16777619u;
  }
  for (std::size_t index = 0; index <= of.depth; ++index) {
    hash = (hash ^ of.quals[index]) * 16777619u;
  }
  return hash;
}

// Whether each state of the sources of a binding is on the way to what a
// request takes: in a context that requests the interface, a request takes
// nothing of what holds the leaf, which is only on the way to what holds
// the interface.
constexpr void used_states(const semantics::search &found, bool abstract,
                           bool *used) {
  for (std::size_t at = 0; at < found.count; ++at) {
    used[at] = !abstract;
  }
  for (std::size_t at = found.count; at-- > 0;) {
    const semantics::state &each = found.states[at];
    if (each.reached.of == semantics::element::interface) {
      used[at] = true;
    }
    if (used[at] && each.parent != semantics::no_state) {
      used[each.parent] = true;
    }
  }
}

constexpr state_classes classes_of(const semantics::search &found,
                                   bool abstract = false) {
  state_classes made;
  std::uint16_t types[semantics::search_slots] = {};
  std::uint16_t steps[semantics::search_slots] = {};
  bool used[semantics::max_states] = {};
  used_states(found, abstract, used);
  for (std::size_t at = 0; at < found.count; ++at) {
    const semantics::state &each = found.states[at];
    std::size_t slot = type_hash(each.reached) % semantics::search_slots;
    while (types[slot] != 0 &&
           !same_type(found.states[types[slot] - 1].reached, each.reached)) {
      slot = (slot + 1) % semantics::search_slots;
    }
    if (types[slot] == 0) {
      types[slot] = static_cast<std::uint16_t>(at + 1);
    }
    made.of[at] = static_cast<std::uint16_t>(types[slot] - 1);
    if (at == 0 || !used[at]) {
      continue;
    }
    const std::uint16_t from = made.of[each.parent];
    const std::uint16_t to = made.of[at];
    slot = ((from * 31u + to) * 31u + static_cast<unsigned>(each.via)) %
           semantics::search_slots;
    while (steps[slot] != 0) {
      const std::uint16_t edge = made.edges[steps[slot] - 1];
      const semantics::state &other = found.states[edge];
      if (made.of[other.parent] == from && made.of[edge] == to &&
          other.via == each.via) {
        break;
      }
      slot = (slot + 1) % semantics::search_slots;
    }
    if (steps[slot] == 0) {
      made.edges[made.edge_count++] = static_cast<std::uint16_t>(at);
      steps[slot] = static_cast<std::uint16_t>(made.edge_count);
    }
    made.edge_of[at] = static_cast<std::uint16_t>(steps[slot] - 1);
  }
  return made;
}

// The layer sources of a binding, as it publishes them: those a request
// takes something of, and those on the way to them, in their order; and the
// table of the type of each, which the steps into it are checked against as
// the code compiles.
// Whether a step makes a layer of what it is taken from, which the table of
// what it makes has.
constexpr bool makes_by(semantics::step kind) {
  return kind == semantics::step::wrap || kind == semantics::step::adopt ||
         kind == semantics::step::observe;
}

// The kinds of the steps a request takes from a thing, as a mask.
struct step_kinds_sink {
  std::uint32_t kinds = 0;
  constexpr void operator()(semantics::step kind, const semantics::thing &) {
    kinds |= std::uint32_t{1} << static_cast<unsigned>(kind);
  }
};

constexpr std::uint32_t request_kinds(const semantics::thing &of,
                                      const semantics::context &with) {
  step_kinds_sink sink;
  semantics::each_request_step(of, with, sink);
  return sink.kinds;
}

template <typename Plan, const semantics::search &Sources, typename Identities,
          typename Stored, typename Leaf, typename Interface, typename Keeper>
struct binding_sources {
  static constexpr bool abstract = !std::is_same_v<Leaf, Interface>;
  static constexpr state_classes classes = classes_of(Sources, abstract);

  struct used_list {
    bool value[semantics::max_states] = {};
    std::uint16_t index[semantics::max_states] = {};
    std::uint16_t states[semantics::max_states] = {};
    std::size_t count = 0;
  };
  static constexpr used_list make_used() {
    used_list made;
    used_states(Sources, abstract, made.value);
    for (std::size_t at = 0; at < Sources.count; ++at) {
      if (made.value[at]) {
        made.index[at] = static_cast<std::uint16_t>(made.count);
        made.states[made.count++] = static_cast<std::uint16_t>(at);
      }
    }
    return made;
  }
  static constexpr used_list used = make_used();

  template <std::size_t State>
  using step_into =
      typename typed_step_at<Identities, search_at<Sources, State>>::step_type;

  template <typename U> struct tag {
    using type = U;
  };

  // The carrier a step into a state makes of the carrier From.
  template <std::size_t State, typename From>
  using reached_t = decltype(step_into<State>::template apply<Leaf, Interface>(
      std::declval<From>(), std::declval<transient_view<Keeper> &>()));

  // The carrier of a class of states.
  template <std::size_t Class> struct carrier_of {
    static auto make() {
      if constexpr (Class == 0) {
        return tag<carrier<Stored,
                           semantics::owned(Sources.states[0].reached.from)>>{};
      } else {
        return tag<reached_t<
            Class, typename carrier_of<
                       classes.of[Sources.states[Class].parent]>::type>>{};
      }
    }
    using type = typename decltype(make())::type;
  };

  // The operations each class of states needs: the steps the binding takes
  // from it, the steps a request may take from it, and the wraps that make
  // it. A sole owner converts to the sharing handle its type traits name,
  // which a request may bring into the universe: of every source that is
  // one.
  struct mask_list {
    std::uint32_t of[semantics::max_states] = {};
  };
  static constexpr mask_list make_masks() {
    mask_list made;
    constexpr std::uint32_t taken =
        mask_of(semantics::step::copy) | mask_of(semantics::step::decay) |
        mask_of(semantics::step::hand_over) | mask_of(semantics::step::convert);
    const semantics::context with = Plan::context(abstract);
    for (std::size_t index = 0; index < used.count; ++index) {
      const std::size_t at = used.states[index];
      const semantics::state &each = Sources.states[at];
      made.of[classes.of[at]] |= (request_kinds(each.reached, with) & taken) |
                                 mask_of(semantics::step::convert);
      if (at == 0) {
        continue;
      }
      if (makes_by(each.via)) {
        made.of[classes.of[at]] |= makers_mask;
      } else if (each.via == semantics::step::release &&
                 Sources.states[each.parent].reached.layers[0] ==
                     semantics::array_pointer_layer) {
        made.of[classes.of[each.parent]] |= release_array_mask;
      } else {
        made.of[classes.of[each.parent]] |= mask_of(each.via);
      }
    }
    return made;
  }
  static constexpr mask_list masks = make_masks();

  template <std::size_t State>
  using table_t =
      table_of<typename carrier_of<classes.of[State]>::type::type, Leaf,
               Interface, Keeper, masks.of[classes.of[State]]>;

  // A published state, renumbered, of the table of its type; the step into
  // it is checked to be an operation of the table it is taken from.
  template <std::size_t State> static constexpr semantics::state source() {
    if constexpr (State != 0) {
      constexpr semantics::state into = Sources.states[State];
      using from = typename carrier_of<classes.of[into.parent]>::type;
      static_assert(
          std::is_same_v<reached_t<State, from>,
                         typename carrier_of<classes.of[State]>::type>,
          "a step into a state makes the type of its class");
      if constexpr (makes_by(into.via)) {
        using made = typename table_t<State>::maker;
        constexpr const semantics::thing &reached =
            Sources.states[State].reached;
        static_assert(into.via == semantics::step::wrap
                          ? made::full.made_of[run_of(State)].owned != nullptr
                      : into.via == semantics::step::observe
                          ? has(made::full.observing)
                          : has(reached.depth > 1 && reached.layers[1] ==
                                                         semantics::array_layer
                                    ? made::full.adopted_array
                                    : made::full.adopted),
                      "the type a binding makes a layer of is made so");
      } else {
        using parent = table_t<into.parent>;
        constexpr operation<Keeper> step =
            into.via == semantics::step::release &&
                    Sources.states[into.parent].reached.layers[0] ==
                        semantics::array_pointer_layer
                ? parent::maker::full.release_array
                : parent::maker::full.from[static_cast<std::size_t>(into.via)];
        static_assert((from::owned ? step.owned : step.borrowed) != nullptr,
                      "the table of a type has every step from it");
      }
    }
    // Every step a request takes of it that its type knows.
    using own = typename carrier_of<classes.of[State]>::type;
    constexpr std::uint32_t kinds =
        request_kinds(Sources.states[State].reached, Plan::context(abstract));
    constexpr const full_table<Keeper> &table = table_t<State>::maker::full;
    static_assert(known<own>(table, kinds, semantics::step::copy) &&
                      known<own>(table, kinds, semantics::step::decay) &&
                      known<own>(table, kinds, semantics::step::hand_over) &&
                      known<own>(table, kinds, semantics::step::convert),
                  "the table of a source has every step a request takes of it");
    semantics::state made = Sources.states[State];
    if (made.parent != semantics::no_state) {
      made.parent = used.index[made.parent];
    }
    return made;
  }

  // Whether a table has a step of a kind of a mask, for the carrier Own.
  template <typename Own>
  static constexpr bool known(const full_table<Keeper> &table,
                              std::uint32_t kinds, semantics::step kind) {
    if (!(kinds & (std::uint32_t{1} << static_cast<unsigned>(kind)))) {
      return true;
    }
    const operation<Keeper> &step = table.from[static_cast<std::size_t>(kind)];
    return (Own::owned ? step.owned : step.borrowed) != nullptr;
  }

  // How many wraps make a state at once, down to the first step that is
  // not one.
  static constexpr std::size_t run_of(std::size_t state) {
    std::size_t run = 0;
    while (Sources.states[state].via == semantics::step::wrap) {
      ++run;
      state = Sources.states[state].parent;
    }
    return run;
  }

  template <std::size_t... I>
  static constexpr auto publish(std::index_sequence<I...>) {
    struct table {
      semantics::state states[sizeof...(I)];
      const type_table<Keeper> *tables[sizeof...(I)];
    };
    return table{{source<used.states[I]>()...},
                 {&table_t<used.states[I]>::value...}};
  }

  static constexpr auto published =
      publish(std::make_index_sequence<used.count>{});
};

// The types a request of Request spells, from its shape at depth 0 to its
// element, of which Element is the innermost.
template <typename Request, typename Element> struct request_types {
  using shape = typename requested_category<Request>::shape;
  static constexpr std::size_t depth =
      type_list_size_v<typename stack_of_t<shape, Element>::layers>;

  template <std::size_t K, typename T = shape> struct nested {
    using type = typename nested<K - 1, element_of_t<T, Element>>::type;
  };
  template <typename T> struct nested<0, T> {
    using type = T;
  };
};

// The tables of the types a request spells, which make the layers it makes.
template <typename Request, typename Element, typename Keeper>
struct request_tables {
  using types = request_types<Request, Element>;

  // Whether a type is a layer a request makes: of a wrapper or an
  // alternative type.
  template <typename T> static constexpr bool makes() {
    using type = std::remove_cv_t<T>;
    return !std::is_pointer_v<type> && !std::is_array_v<type> &&
           (is_alternative_type_v<type> || type_traits<type>::enabled);
  }

  template <std::size_t K>
  using table_t = table_of<
      typename types::template nested<K>::type, Element, Element, Keeper,
      makes<typename types::template nested<K>::type>() ? makers_mask : 0>;

  // A layer the semantics make of what it holds, as a wrapper the request
  // spells is made of its element value, adopts a pointer or observes one,
  // has the operation that makes it.
  template <std::size_t K> static constexpr bool checked() {
    using nested = typename types::template nested<K>::type;
    using type = std::remove_cv_t<nested>;
    // What the semantics make is of no qualifiers.
    if constexpr (!makes<type>() || is_alternative_type_v<type> ||
                  !is_complete_v<type> || !std::is_same_v<nested, type>) {
      return true;
    } else {
      constexpr semantics::layer_kind kind =
          wrapper_kind_v<layer_identity_t<type>>;
      using maker = typename table_t<K>::maker;
      using of = typename types::template nested<K + 1>::type;
      return (!kind.made_from_value ||
              !layer_types<of, Element, Element>::template movable<of>() ||
              has(maker::full.made_of[1])) &&
             (!kind.observes || has(maker::full.observing));
    }
  }

  template <std::size_t... K>
  static constexpr auto make(std::index_sequence<K...>) {
    static_assert((checked<K>() && ...),
                  "a request has the operation that makes each layer it makes");
    struct table {
      const type_table<Keeper> *value[sizeof...(K)];
    };
    return table{{&table_t<K>::value...}};
  }

  static constexpr auto value =
      make(std::make_index_sequence<types::depth + 1>{});
};
} // namespace dingo::detail::execution
