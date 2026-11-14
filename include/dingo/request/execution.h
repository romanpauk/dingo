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
#include <dingo/type/type_traits.h>

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

// The element type of a layer of type T, toward Leaf: what a pointer points
// to; what a wrapper holds, the array a handle of an array holds, of the
// bound its type traits give; the alternative of an alternative type that
// holds the leaf or its interface.
template <typename T, typename Leaf> struct element_of {
  static auto make() {
    if constexpr (std::is_pointer_v<T>) {
      return type_tag<std::remove_pointer_t<T>>{};
    } else if constexpr (is_alternative_type_v<T>) {
      return type_tag<typename alternatives_stack<
          Leaf, alternative_type_alternatives_t<T>>::alternative>{};
    } else {
      using element = typename type_traits<T>::value_type;
      if constexpr (!is_array_like_type_v<T>) {
        return type_tag<element>{};
      } else if constexpr (handle_extent_v<T> == 0) {
        return type_tag<element[]>{};
      } else {
        return type_tag<element[handle_extent_v<T>]>{};
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
  using element = element_of_t<T, Leaf>;
  if constexpr (std::is_pointer_v<T>) {
    return layer;
  } else if constexpr (is_alternative_type_v<T>) {
    return type_traits<T>::template get<element>(layer);
  } else if constexpr (std::is_array_v<element>) {
    return reinterpret_cast<std::add_pointer_t<element>>(
        type_traits<T>::get(layer));
  } else {
    return type_traits<T>::get(layer);
  }
}

// Whether the element of an owned layer is owned: a layer that owns it
// alone hands it over with itself.
template <typename T> constexpr bool owns_alone() {
  if constexpr (std::is_pointer_v<T>) {
    return false;
  } else if constexpr (is_alternative_type_v<T>) {
    return true;
  } else {
    return std::is_same_v<typename type_traits<T>::semantics,
                          ::dingo::semantics::sole_owner> ||
           std::is_same_v<typename type_traits<T>::semantics,
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
template <> struct step<semantics::step::element> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &) {
    using element = element_of_t<T, Leaf>;
    carrier<element, Owned && owns_alone<T>()> reached{
        element_address<T, Leaf>(*from.object), from.owner, from.release};
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
    return carrier<element, owned>{std::addressof(keeper.template keep<element>(
        [] { return type_traits<element>::empty_value(); }))};
  }
};

// A copy of what element_or_empty gives.
template <> struct step<semantics::step::element_or_empty_copy> {
  template <typename Leaf, typename Interface, typename T, bool Owned,
            typename Keeper>
  static auto apply(carrier<T, Owned> from, Keeper &keeper) {
    using element = element_of_t<T, Leaf>;
    auto *held = element_address<T, Leaf>(*from.object);
    auto &made = keeper.template keep<element>([&]() -> element {
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
template <typename T, typename Leaf, typename Interface> struct upcast_of {
  static auto make() {
    if constexpr (std::is_same_v<std::remove_cv_t<T>, Leaf>) {
      return type_tag<Interface>{};
    } else if constexpr (std::is_pointer_v<T>) {
      return type_tag<typename upcast_of<std::remove_pointer_t<T>, Leaf,
                                         Interface>::type *>{};
    } else {
      return type_tag<
          typename type_traits<T>::template rebind_t<typename upcast_of<
              typename type_traits<T>::value_type, Leaf, Interface>::type>>{};
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
    if constexpr (std::is_same_v<T, Leaf>) {
      return carrier<Interface, Owned>{static_cast<Interface *>(from.object),
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
      // one the request owns, or copied, as a new object the request owns.
      using made_type = typename upcast_of<T, Leaf, Interface>::type;
      auto &made = keeper.template keep<made_type>([&]() -> made_type {
        if constexpr (Owned && !std::is_copy_constructible_v<T>) {
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
    auto &made = keeper.template keep<T>([&] { return T(*from.object); });
    return carrier<T, true>{std::addressof(made)};
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

// What the last steps of a plan make, a run of wraps of what Source carries,
// not yet made.
template <typename Made, typename Source, bool Stable = false> struct pending {
  Source source;
};

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
        return pending<made_type, Carrier, stable>{from};
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
          typename Interface, typename Made, typename Source, bool Stable,
          typename Keeper>
Request satisfy(pending<Made, Source, Stable> reached, Keeper &keeper) {
  if constexpr (How == semantics::satisfaction::move) {
    // A value is made of what the wraps are made of, where it is returned.
    return made_of<Leaf, Interface, Made>(std::move(*reached.source.object));
  } else {
    auto &made = keeper_of<Stable>(keeper).template keep<Made>([&] {
      return made_of<Leaf, Interface, Made>(std::move(*reached.source.object));
    });
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

// A request of a stored object, planned as the code compiles: what C++ says
// of the stored type Stored, of the request type Request and of the leaf
// Leaf, classified as the semantics know them, and the plan of the request
// for each lifetime it may need: as long as the container, when what it
// refers to is passed to the caller, or as long as the request. The stored
// object is made for the request by a scope that makes its instance
// (Makes), or kept, and it is published early by a scope that does
// (Early).
template <typename Stored, typename Request, typename Leaf, bool Makes,
          bool Early>
struct request_plan {
  using category = requested_category<Request>;
  using shape = typename category::shape;
  using stored_stack = stack_of_t<Stored, Leaf>;
  using request_stack = stack_of_t<shape, Leaf>;
  using universe_type = layer_universe<typename joined<
      typename joined<typename stored_stack::layers,
                      typename request_stack::layers>::type,
      typename owning_layers<typename owning_handles<>::type>::type>::type>;
  using identities = typename universe_type::identities;
  using interface_type = innermost_t<shape, Leaf>;

  static constexpr bool modeled =
      category::modeled && stored_stack::element != stack_element::other &&
      request_stack::element != stack_element::other;

  static constexpr semantics::context context() {
    return {leaf_traits_of<Leaf>(),
            {Makes, false, Early},
            request_stack::element == stack_element::interface,
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
    for (std::size_t index = 0; index <= made.depth; ++index) {
      made.lifetimes[index] =
          Makes ? semantics::lifetime::request : semantics::lifetime::stable;
    }
    made.from = Makes ? semantics::origin::made : semantics::origin::borrowed;
    made.instance = true;
    return made;
  }

  static constexpr semantics::request goal() {
    semantics::request made;
    place_layers<universe_type>(made.layers, typename request_stack::layers{});
    made.depth = static_cast<std::uint8_t>(
        type_list_size_v<typename request_stack::layers>);
    made.of = element_of_stack(request_stack::element);
    return made;
  }

  static constexpr semantics::plan make(semantics::lifetime needed) {
    if constexpr (!modeled) {
      (void)needed;
      return {};
    } else {
      constexpr semantics::context with = context();
      const semantics::search found = semantics::reach(start(), with, goal());
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

// The C++ step of the step at an index of a plan, to the layers of a universe
// of identities.
template <const semantics::plan &Plan, typename Identities, std::size_t Index>
struct typed_step {
  static constexpr semantics::step kind = Plan.steps[Index];
  static constexpr semantics::thing reached = Plan.things[Index + 1];
  static constexpr bool stable =
      reached.lifetimes[0] == semantics::lifetime::stable;

  template <semantics::layer Layer>
  using layer_type = type_list_element_t<Layer - 4, Identities>;

  static auto make() {
    if constexpr (kind == semantics::step::wrap) {
      return planned<step<semantics::step::wrap, layer_type<reached.layers[0]>>,
                     stable>{};
    } else if constexpr (kind == semantics::step::adopt) {
      return planned<adopt_step<layer_type<reached.layers[0]>,
                                reached.layers[1] == semantics::array_layer>,
                     stable>{};
    } else {
      return planned<step<kind>, stable>{};
    }
  }
  using type = decltype(make());
};

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
} // namespace dingo::detail::execution
