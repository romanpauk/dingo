//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/request/classification.h>
#include <dingo/request/execution.h>
#include <dingo/semantics/model.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/type_traits.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

// The execution of plans by the runtime container: a carrier of no type, the
// table of the operations of each type a plan steps through, the plan of a
// request compiled into those operations as it is resolved, and the tables a
// binding and a request give it.

namespace dingo::detail::execution {

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

// The layout of a type a runtime resolution keeps an object of: its size, its
// alignment, and its destructor, none where it has none. A keeper keeps one
// object of each layout with a binding, under the address of the layout.
struct kept_layout {
  std::size_t size;
  std::size_t alignment;
  void (*destroy)(void *) noexcept;
};

template <typename T> void destroy_kept(void *object) noexcept {
  static_cast<T *>(object)->~T();
}

template <typename T>
inline constexpr kept_layout kept_layout_v{
    sizeof(T), alignof(T),
    std::is_trivially_destructible_v<T> ? nullptr : &destroy_kept<T>};

// Constructs at an address the object that what make points to makes.
using construct_function = void (*)(void *at, void *make);

template <typename T, typename Make> void construct_kept(void *at, void *make) {
  new (at) T((*static_cast<Make *>(make))());
}

// A keeper of a runtime resolution as an operation uses it: what it makes is
// kept as long as the container when it is stable, as long as the request
// otherwise. The keeper knows only the layout of what it keeps.
template <typename Keeper> struct keeper_view {
  Keeper &keeper;
  bool stable;

  template <typename T, typename Make> T &keep(Make &&make) {
    using make_type = std::remove_reference_t<Make>;
    return *static_cast<T *>(keeper.keep(
        stable, kept_layout_v<T>, &construct_kept<T, make_type>,
        const_cast<void *>(static_cast<const void *>(std::addressof(make)))));
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
    static_cast<std::size_t>(semantics::step::unknown_bound) + 1;

// What a runtime resolution does with an object of a type, as the steps of
// the semantics do it, one table for each type: the steps from an object of
// it, by their kind, and the release of a pointer to the first element of
// an array; and the steps that make one of it: of the value its nested type
// at a depth is, moved into all its layers at once; of a pointer it takes
// over to its element, or to the first element of its array; and of a
// pointer to its element it does not own; and of a pointer or a handle of
// its element of fewer qualifiers, by the qualifiers it lacks.
template <typename Keeper> struct full_table {
  operation<Keeper> from[step_kinds] = {};
  operation<Keeper> release_array = {};
  operation<Keeper> made_of[semantics::max_layers + 1] = {};
  operation<Keeper> adopted = {};
  operation<Keeper> adopted_array = {};
  operation<Keeper> observing = {};
  operation<Keeper> qualified[4] = {};
};

// Where an operation is in a table: a kind of step from the type, the
// release of a pointer to the first element of an array, the run of wraps
// that makes the type of its nested type at a depth, the adoption of a
// pointer to its element or to the first element of its array, the
// observer of a pointer, and the pointer or the handle of an element of more
// qualifiers, by the qualifiers it adds.
enum class slot : std::uint8_t {
  release_array = step_kinds,
  made_of,
  adopted = made_of + semantics::max_layers + 1,
  adopted_array,
  observing,
  qualified,
  count = qualified + 4
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
  if (at == slot_of(slot::observing)) {
    return table.observing;
  }
  return table.qualified[at - slot_of(slot::qualified)];
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
inline constexpr std::uint32_t qualified_mask = std::uint32_t{1}
                                                << (step_kinds + 2);

// Whether what a step of a kind makes lives as long as the request, as the
// semantics give it: nothing kept with the binding is made of it.
constexpr bool makes_transient(semantics::step kind) {
  return kind == semantics::step::copy || kind == semantics::step::decay ||
         kind == semantics::step::hand_over ||
         kind == semantics::step::convert || kind == semantics::step::release ||
         kind == semantics::step::unknown_bound;
}

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

// A raw pointer the request owns, taken over by the handle Made: of a Whole
// array Made holds, as the pointer to its first element.
template <typename Made, typename Pointer, typename Keeper, bool Whole = false>
erased_carrier adopted_erased(erased_carrier from, bool stable,
                              Keeper &keeper) {
  keeper_view<Keeper> view{keeper, stable};
  auto &made = view.template keep<Made>([&] {
    return Made(adopted_pointer<Whole>(*static_cast<Pointer *>(from.object)));
  });
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

// A pointer or a handle Made of an element of more qualifiers, made of one of
// the element of fewer, Source: moved out of one the request owns, copied of
// another.
template <typename Made, typename Source, bool Owned, typename Keeper>
erased_carrier qualified_erased(erased_carrier from, bool stable,
                                Keeper &keeper) {
  (void)stable;
  keeper_view<Keeper> view{keeper, false};
  auto &made = view.template keep<Made>([&]() -> Made {
    if constexpr (Owned) {
      return Made(std::move(*static_cast<Source *>(from.object)));
    } else {
      return Made(*static_cast<const Source *>(from.object));
    }
  });
  return erase(carrier<Made, true>{std::addressof(made)});
}

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
    // What a step of a transient kind makes lives as long as the request.
    keeper_view<Keeper> view{keeper, !Transient && stable};
    return erase(
        Step::template apply<Leaf, Interface>(unerase<T, Owned>(from), view));
  }

  template <typename Step, bool Owned>
  using reached_t = decltype(Step::template apply<Leaf, Interface>(
      std::declval<carrier<T, Owned>>(),
      std::declval<keeper_view<Keeper> &>()));

  template <typename Step, bool Owned>
  using made_t = typename reached_t<Step, Owned>::type;

  // Whether a step of a kind does to an object the request owns what it does
  // to one it does not, and gives the request the same of it: it neither
  // moves it, nor releases what it was taken out of, nor hands its element
  // over with it. One operation is then both.
  template <semantics::step Kind, typename Step>
  static constexpr bool owned_as_borrowed() {
    using semantics::step;
    if constexpr (!std::is_same_v<reached_t<Step, true>,
                                  reached_t<Step, false>>) {
      return false;
    } else {
      return Kind == step::copy || Kind == step::element_pointer ||
             Kind == step::element_or_empty ||
             Kind == step::element_or_empty_copy ||
             (Kind == step::element && !releases_element_v<T>);
    }
  }

  // An operation of a step of a kind, for an object the request owns, for
  // one it does not, or for both, where the table has it.
  template <semantics::step Kind, typename Step, bool Owned, bool Borrowed,
            std::uint32_t Bit = mask_of(Kind)>
  static constexpr operation<Keeper> step_of() {
    operation<Keeper> made;
    constexpr bool transient = makes_transient(Kind);
    if constexpr ((Mask & Bit) != 0 && Borrowed) {
      made.borrowed = &apply<Step, false, transient>;
      made.made =
          &table_of<made_t<Step, false>, Leaf, Interface, Keeper>::value;
    }
    if constexpr ((Mask & Bit) != 0 && Owned) {
      made.owned = owned_operation<Kind, Step, Borrowed, transient>();
      made.made = &table_of<made_t<Step, true>, Leaf, Interface, Keeper>::value;
    }
    return made;
  }

  template <semantics::step Kind, typename Step, bool Borrowed, bool Transient>
  static constexpr step_function<Keeper> owned_operation() {
    if constexpr (Borrowed) {
      if constexpr (owned_as_borrowed<Kind, Step>()) {
        return &apply<Step, false, Transient>;
      }
    }
    return &apply<Step, true, Transient>;
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
      using of = std::remove_cv_t<typename nested<K>::type>;
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
    if constexpr (wants(step::unknown_bound)) {
      at(step::unknown_bound) =
          step_of<step::unknown_bound, execution::step<step::unknown_bound>,
                  converts_to_unknown_bound<true>(),
                  converts_to_unknown_bound<false>()>();
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
    if constexpr (std::is_same_v<T, type> && (Mask & qualified_mask) != 0) {
      qualified_into(made, std::index_sequence<1, 2, 3>{});
    }
    return made;
  }

  // The pointer or the handle the type is, made of one of an element of
  // fewer qualifiers: the qualifiers of its element without Adds.
  template <std::uint8_t Adds> static constexpr operation<Keeper> qualified() {
    operation<Keeper> made;
    constexpr std::uint8_t quals = element_quals<type>();
    if constexpr ((quals & Adds) == Adds) {
      using source =
          requalified_t<type, static_cast<std::uint8_t>(quals & ~Adds)>;
      if constexpr (std::is_constructible_v<type, source &&>) {
        made.owned = &qualified_erased<type, source, true, Keeper>;
      }
      if constexpr (std::is_constructible_v<type, const source &>) {
        made.borrowed = &qualified_erased<type, source, false, Keeper>;
      }
    }
    return made;
  }

  template <std::size_t... Adds>
  static constexpr void qualified_into(full_table<Keeper> &made,
                                       std::index_sequence<Adds...>) {
    ((made.qualified[Adds] = qualified<static_cast<std::uint8_t>(Adds)>()),
     ...);
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

  // Whether a handle of an array of a known bound converts to the handle of
  // an array of unknown bound its type traits name: moved out of, or copied.
  template <typename U, typename = void> struct unknown_bound_handle {
    using type = void;
  };
  template <typename U>
  struct unknown_bound_handle<
      U, std::void_t<typename type_traits<U>::template unknown_bound_t<
             typename type_traits<U>::value_type>>> {
    using type = typename type_traits<U>::template unknown_bound_t<
        typename type_traits<U>::value_type>;
  };

  template <bool Owned> static constexpr bool converts_to_unknown_bound() {
    if constexpr (!std::is_same_v<T, type> || std::is_pointer_v<type> ||
                  std::is_array_v<type> || !is_complete_v<type>) {
      return false;
    } else if constexpr (!type_traits<type>::enabled ||
                         is_alternative_type_v<type>) {
      return false;
    } else if constexpr (std::is_void_v<
                             typename unknown_bound_handle<type>::type>) {
      return false;
    } else if constexpr (Owned) {
      return std::is_constructible_v<typename unknown_bound_handle<type>::type,
                                     type &&>;
    } else {
      return std::is_constructible_v<typename unknown_bound_handle<type>::type,
                                     const type &>;
    }
  }

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
      // Of a pointer to the element of no qualifiers, which the handle holds
      // of those it needs.
      using of = std::remove_cv_t<typename element<>::type>;
      if constexpr (type_traits<type>::is_owning_handle) {
        if constexpr (std::is_array_v<of>) {
          using pointer = std::remove_extent_t<of> *;
          if constexpr (std::is_constructible_v<type, pointer>) {
            made.adopted_array = {&adopted_erased<type, pointer, Keeper>,
                                  nullptr, nullptr};
            // A handle of an array of a known bound takes over a pointer to
            // a whole one too, as the pointer to its first element.
            if constexpr (std::extent_v<of> != 0) {
              made.adopted = {&adopted_erased<type, of *, Keeper, true>,
                              nullptr, nullptr};
            }
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
// another: what it is, its identity, the identity of the layer an owned value
// of it converts to, and that of the layer it converts to of an array of
// unknown bound, or none.
struct runtime_layer {
  semantics::layer_kind kind;
  const void *identity;
  const void *converts_to;
  const void *unknown_bound_to;
};

// The identity tag of a layer, or none.
template <typename Identity> constexpr const void *identity_or_none() {
  if constexpr (std::is_void_v<Identity>) {
    return nullptr;
  } else {
    return &identity_tag_v<Identity>;
  }
}

template <typename Identity> constexpr runtime_layer runtime_layer_of() {
  semantics::layer_kind kind = wrapper_kind_v<Identity>;
  kind.converts = 0;
  kind.to_unknown_bound = 0;
  using unknown_bound_target =
      std::conditional_t<converts_to_unknown_bound<Identity>(),
                         typename unknown_bound_layer<Identity>::type, void>;
  return {kind, &identity_tag_v<Identity>,
          identity_or_none<typename shared_layer<Identity>::type>(),
          identity_or_none<unknown_bound_target>()};
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

// A plan as a runtime resolution runs it: the operations of the instructions
// of its program, each with whether what it makes lives as long as the
// container; how it satisfies the request, how the request takes the value of
// what it reaches, and whether what it gives the request the address of
// lives as long as the container.
template <typename Keeper> struct runtime_plan {
  struct call {
    step_function<Keeper> owned;
    step_function<Keeper> borrowed;
    bool stable;
  };
  bool found = false;
  semantics::satisfaction how = semantics::satisfaction::refer;
  finish last = finish::satisfy;
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
  const void *unknown_bound_targets[semantics::max_kinds] = {};
  for (std::size_t index = 0; index < 4; ++index) {
    identities[index] = language_identities[index];
  }
  for (std::size_t index = 0; index < binding.layer_count; ++index) {
    identities[4 + index] = binding.layers[index].identity;
    targets[4 + index] = binding.layers[index].converts_to;
    unknown_bound_targets[4 + index] = binding.layers[index].unknown_bound_to;
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
      unknown_bound_targets[at] = layer.unknown_bound_to;
      merged.add(layer.kind);
    }
    mapped[4 + index] = static_cast<semantics::layer>(at);
  }
  for (std::size_t index = 4; index < merged.count; ++index) {
    merged.kinds[index].converts = 0;
    merged.kinds[index].to_unknown_bound = 0;
    for (std::size_t to = 4; to < merged.count; ++to) {
      if (targets[index] != nullptr && identities[to] == targets[index]) {
        merged.kinds[index].converts |= std::uint32_t{1} << to;
      }
      if (unknown_bound_targets[index] != nullptr &&
          identities[to] == unknown_bound_targets[index]) {
        merged.kinds[index].to_unknown_bound |= std::uint32_t{1} << to;
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

  const program compiled = compile_program(plan, binding.source_count);
  // The table of the type of what the plan reached so far.
  const type_table<Keeper> *table = binding.tables[0];
  for (std::size_t index = 0; index < compiled.count; ++index) {
    const instruction &each = compiled.at[index];
    const semantics::thing &from = plan.things[each.first];
    const semantics::thing &to = plan.things[each.last + 1];
    const std::uint16_t state = plan.states[each.last + 1];
    // The table of what an instruction makes a layer of: of a source of the
    // binding, or of a type the request spells, by its depth in the request.
    const type_table<Keeper> *maker =
        each.binding ? binding.tables[state]
                     : request.tables[goal.depth - to.depth];
    const operation<Keeper> *step = nullptr;
    switch (each.kind) {
    case semantics::step::wrap:
      step = &(*maker)[slot_of(slot::made_of, each.wraps)];
      break;
    case semantics::step::adopt:
      step = &(*maker)[to.layers[1] == semantics::array_layer && to.depth > 1
                           ? slot_of(slot::adopted_array)
                           : slot_of(slot::adopted)];
      break;
    case semantics::step::observe:
      step = &(*maker)[slot_of(slot::observing)];
      break;
    case semantics::step::qualify:
      step = &(
          *maker)[slot_of(slot::qualified, static_cast<std::uint8_t>(
                                               to.quals[1] & ~from.quals[1]))];
      break;
    case semantics::step::release:
      step = &(*table)[from.layers[0] == semantics::array_pointer_layer
                           ? slot_of(slot::release_array)
                           : slot_of(each.kind)];
      break;
    default:
      step = &(*table)[slot_of(each.kind)];
      break;
    }
    made.calls[made.count++] = {step->owned, step->borrowed,
                                to.lifetimes[0] == semantics::lifetime::stable};
    // The table of what a step of the binding makes is that of its source;
    // of what a step of a request makes, that its operation names.
    table = each.binding ? binding.tables[state]
            : (each.kind == semantics::step::wrap ||
               each.kind == semantics::step::adopt ||
               each.kind == semantics::step::observe ||
               each.kind == semantics::step::qualify)
                ? maker
                : step->made;
  }
  made.last = compiled.last;
  made.depth = compiled.depth;
  made.quals = compiled.quals;
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
      std::declval<From>(), std::declval<keeper_view<Keeper> &>()));

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
  // it. A sole owner converts to the sharing handle its type traits name, and
  // a handle of an array of a known bound to the handle of an array of unknown
  // bound they name, which a request may bring into the universe: of every
  // source that is one.
  struct mask_list {
    std::uint32_t of[semantics::max_states] = {};
  };
  static constexpr mask_list make_masks() {
    mask_list made;
    constexpr std::uint32_t taken = mask_of(semantics::step::copy) |
                                    mask_of(semantics::step::decay) |
                                    mask_of(semantics::step::hand_over) |
                                    mask_of(semantics::step::convert) |
                                    mask_of(semantics::step::unknown_bound);
    const semantics::context with = Plan::context(abstract);
    for (std::size_t index = 0; index < used.count; ++index) {
      const std::size_t at = used.states[index];
      const semantics::state &each = Sources.states[at];
      made.of[classes.of[at]] |= (request_kinds(each.reached, with) & taken) |
                                 mask_of(semantics::step::convert) |
                                 mask_of(semantics::step::unknown_bound);
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
        // A request makes the copy of what element_or_empty gives where it
        // takes it.
        if (each.via == semantics::step::element_or_empty_copy) {
          made.of[classes.of[each.parent]] |=
              mask_of(semantics::step::element_or_empty);
        }
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
                      known<own>(table, kinds, semantics::step::convert) &&
                      known<own>(table, kinds, semantics::step::unknown_bound),
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

  // Whether a type is a pointer or a handle of a qualified element, which a
  // request converts one of fewer qualifiers to.
  template <typename T> static constexpr bool qualifies() {
    using type = std::remove_cv_t<T>;
    if constexpr (std::is_pointer_v<type>) {
      return element_quals<type>() != 0;
    } else if constexpr (!makes<type>() || is_alternative_type_v<type> ||
                         !is_complete_v<type>) {
      return false;
    } else {
      return wrapper_kind_v<layer_identity_t<type>>.qualifies &&
             element_quals<type>() != 0;
    }
  }

  template <typename T> static constexpr std::uint32_t mask_for() {
    return (makes<T>() ? makers_mask : 0) |
           (qualifies<T>() ? qualified_mask : 0);
  }

  template <std::size_t K>
  using table_t =
      table_of<typename types::template nested<K>::type, Element, Element,
               Keeper, mask_for<typename types::template nested<K>::type>()>;

  // A layer the semantics make of what it holds, as a wrapper the request
  // spells is made of its element value, adopts a pointer or observes one,
  // has the operation that makes it.
  template <std::size_t K> static constexpr bool checked() {
    using nested = typename types::template nested<K>::type;
    using type = std::remove_cv_t<nested>;
    // A pointer or a handle of a qualified element is made of one of each
    // fewer qualifiers, moved, and copied when it can be.
    if constexpr (qualifies<type>() && std::is_same_v<nested, type>) {
      using maker = typename table_t<K>::maker;
      constexpr std::uint8_t quals = element_quals<type>();
      constexpr bool copies = std::is_copy_constructible_v<type>;
      for (std::uint8_t adds = 1; adds < 4; ++adds) {
        if ((quals & adds) == adds &&
            (maker::full.qualified[adds].owned == nullptr ||
             (copies && maker::full.qualified[adds].borrowed == nullptr))) {
          return false;
        }
      }
    }
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
                  "a request has the operation that makes each layer it makes, "
                  "and that converts each pointer or handle to the qualifiers "
                  "it asks");
    struct table {
      const type_table<Keeper> *value[sizeof...(K)];
    };
    return table{{&table_t<K>::value...}};
  }

  static constexpr auto value =
      make(std::make_index_sequence<types::depth + 1>{});
};
} // namespace dingo::detail::execution
