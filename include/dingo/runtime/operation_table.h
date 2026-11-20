//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/memory/object_layout.h>
#include <dingo/request/operation.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

// The operations of the runtime container: a carrier of no type, and the
// erased operations of each type a conversion steps through, in its table.

namespace dingo::detail::execution {

// A carrier of no type, which the runtime container passes between the
// operations of a conversion: the object, whether the request owns it, and what
// it was taken out of, with how to release it.
struct erased_carrier {
  void *object = nullptr;
  bool owned = false;
  void *owner = nullptr;
  void (*release)(void *owner) = nullptr;
};

template <typename T, bool Owned>
erased_carrier erase(carrier<T, Owned> source) {
  return {const_cast<void *>(static_cast<const volatile void *>(source.object)),
          Owned, source.owner, source.release};
}

template <typename T, bool Owned>
carrier<T, Owned> unerase(erased_carrier source) {
  return {static_cast<T *>(source.object), source.owner, source.release};
}

// Constructs at an address the object that what make points to makes.
using construct_function = void (*)(void *place, void *make);

template <typename T, typename Make>
void construct_made(void *place, void *make) {
  new (place) T((*static_cast<Make *>(make))());
}

// The store of a runtime resolution, of no context: it keeps an object of a
// layout, which construct constructs of make, as long as the container when
// it is stable, as long as the request otherwise; and it rejects a request of
// what an empty layer does not hold.
class runtime_store {
public:
  virtual void *keep(bool stable, const object_layout &layout,
                     construct_function construct, void *make) = 0;
  [[noreturn]] virtual void absent() = 0;

protected:
  ~runtime_store() = default;
};

// A store of a runtime resolution as an operation uses it: what it makes is
// kept as long as the container when it is stable, as long as the request
// otherwise.
struct store_view {
  runtime_store &store;
  bool stable;

  template <typename T, typename Make> T &keep(Make &&make) {
    using make_type = std::remove_reference_t<Make>;
    return *static_cast<T *>(store.keep(
        stable, object_layout_v<T>, &construct_made<T, make_type>,
        const_cast<void *>(static_cast<const void *>(std::addressof(make)))));
  }

  void absent() { store.absent(); }
};

// An operation of a runtime resolution, of no type.
using operation_function = erased_carrier (*)(erased_carrier source,
                                              bool stable,
                                              runtime_store &store);

struct type_table;

// An operation on an object of a type: for one the request owns and for one
// it does not, none where C++ does not apply it, and the table of the type
// of what it makes. Whether it has each function is held apart from the
// function: GCC does not compare the address of a function to null in a
// constant expression when it keeps null pointer checks, as -fsanitize=null
// makes it.
struct erased_operation {
  operation_function owned = nullptr;
  operation_function borrowed = nullptr;
  const type_table *made = nullptr;
  bool has_owned = false;
  bool has_borrowed = false;
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
struct full_table {
  erased_operation operations[step_kinds] = {};
  erased_operation release_array = {};
  erased_operation wrap_runs[semantics::max_layers + 1] = {};
  erased_operation adopted = {};
  erased_operation adopted_array = {};
  erased_operation observing = {};
  erased_operation qualified[4] = {};
};

// Where an operation is in a table: a kind of step from the type, the
// release of a pointer to the first element of an array, the run of wraps
// that makes the type of its nested type at a depth, the adoption of a
// pointer to its element or to the first element of its array, the
// observer of a pointer, and the pointer or the handle of an element of more
// qualifiers, by the qualifiers it adds.
enum class slot : std::uint8_t {
  release_array = step_kinds,
  wrap_runs,
  adopted = wrap_runs + semantics::max_layers + 1,
  adopted_array,
  observing,
  qualified,
  count = qualified + 4
};

constexpr std::uint8_t operation_slot(semantics::step kind) {
  return static_cast<std::uint8_t>(kind);
}
constexpr std::uint8_t operation_slot(slot index, std::size_t depth = 0) {
  return static_cast<std::uint8_t>(static_cast<std::size_t>(index) + depth);
}

struct slot_entry {
  std::uint8_t slot;
  erased_operation operation;
};

// A table as a runtime resolution keeps it: the operations it has, as many
// as there are.
struct type_table {
  void *(*pointee)(void *pointer);
  const slot_entry *entries;
  std::size_t count;

  // The operation at a slot, or one of no function.
  const erased_operation &operator[](std::uint8_t slot) const {
    for (std::size_t index = 0; index < count; ++index) {
      if (entries[index].slot == slot) {
        return entries[index].operation;
      }
    }
    static constexpr erased_operation none{};
    return none;
  }
};

// The operation at a slot of a full table.
constexpr const erased_operation &slot_operation(const full_table &table,
                                                 std::uint8_t index) {
  if (index < step_kinds) {
    return table.operations[index];
  }
  if (index == operation_slot(slot::release_array)) {
    return table.release_array;
  }
  if (index < operation_slot(slot::adopted)) {
    return table.wrap_runs[index - operation_slot(slot::wrap_runs)];
  }
  if (index == operation_slot(slot::adopted)) {
    return table.adopted;
  }
  if (index == operation_slot(slot::adopted_array)) {
    return table.adopted_array;
  }
  if (index == operation_slot(slot::observing)) {
    return table.observing;
  }
  return table.qualified[index - operation_slot(slot::qualified)];
}

constexpr bool has(const erased_operation &operation) {
  return operation.has_owned || operation.has_borrowed;
}

// The operations a table of a binding has: a bit for each kind of step from
// its type, one for the release of a pointer to the first element of an
// array, and one for the steps that make an object of it.
constexpr std::uint32_t operation_mask(semantics::step kind) {
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
         kind == semantics::step::transfer ||
         kind == semantics::step::release ||
         kind == semantics::step::unknown_bound;
}

// The steps that make an object of type Made of another.
// A run of wraps of a value the request owns: Made at once, of the T moved
// into it.
template <typename Made, typename Leaf, typename Interface, typename T>
erased_carrier wrap_operation(erased_carrier source, bool stable,
                              runtime_store &store) {
  store_view view{store, stable};
  auto &made = view.template keep<Made>([&] {
    return wrap_value<Leaf, Interface, Made>(
        std::move(*static_cast<T *>(source.object)));
  });
  return erase(carrier<Made, true>{std::addressof(made)});
}

// A raw pointer the request owns, taken over by the handle Made: of a Whole
// array Made holds, as the pointer to its first element.
template <typename Made, typename Pointer, bool Whole = false>
erased_carrier adopted_operation(erased_carrier source, bool stable,
                                 runtime_store &store) {
  store_view view{store, stable};
  auto &made = view.template keep<Made>([&] {
    return Made(adopted_pointer<Whole>(*static_cast<Pointer *>(source.object)));
  });
  // The handle owns what the pointer points to now.
  carrier<Pointer, true>{nullptr, source.owner, source.release}.released();
  return erase(carrier<Made, true>{std::addressof(made)});
}

// A handle Made that does not own its element, made of a pointer to it.
template <typename Made, typename Pointer>
erased_carrier observing_operation(erased_carrier source, bool stable,
                                   runtime_store &store) {
  store_view view{store, stable};
  auto &made = view.template keep<Made>(
      [&] { return Made(*static_cast<Pointer *>(source.object)); });
  return erase(carrier<Made, true>{std::addressof(made)});
}

// A pointer or a handle Made of an element of more qualifiers, made of one of
// the element of fewer, Source: moved out of one the request owns, copied of
// another.
template <typename Made, typename Source, bool Owned>
erased_carrier qualified_operation(erased_carrier source, bool stable,
                                   runtime_store &store) {
  (void)stable;
  store_view view{store, false};
  auto &made = view.template keep<Made>([&]() -> Made {
    if constexpr (Owned) {
      return Made(std::move(*static_cast<Source *>(source.object)));
    } else {
      return Made(*static_cast<const Source *>(source.object));
    }
  });
  return erase(carrier<Made, true>{std::addressof(made)});
}

// The table of a type T, toward the leaf Leaf, of the interface Interface:
// an operation where C++ applies the step to it.
template <typename T, typename Leaf, typename Interface, std::uint32_t Mask = 0>
struct operation_table;

// The full table of a type, of which operation_table keeps the operations it
// has.
template <typename T, typename Leaf, typename Interface, std::uint32_t Mask>
struct table_maker : layer_types<T, Leaf, Interface> {
  using facts = layer_types<T, Leaf, Interface>;
  using type = typename facts::type;
  template <bool Layer = facts::layer()>
  using element = typename facts::template element<Layer>;
  template <std::size_t K> using nested = typename facts::template nested<K>;

  template <typename Step, bool Owned, bool Transient>
  static erased_carrier apply(erased_carrier source, bool stable,
                              runtime_store &store) {
    // What a step of a transient kind makes lives as long as the request.
    store_view view{store, !Transient && stable};
    return erase(
        Step::template apply<Leaf, Interface>(unerase<T, Owned>(source), view));
  }

  template <typename Step, bool Owned>
  using reached_t = decltype(Step::template apply<Leaf, Interface>(
      std::declval<carrier<T, Owned>>(), std::declval<store_view &>()));

  template <typename Step, bool Owned>
  using made_t = typename reached_t<Step, Owned>::type;

  // Whether a step of a kind does to an object the request owns what it does
  // to one it does not, and gives the request the same of it: it neither
  // moves it, nor releases what it was taken out of, nor hands its element
  // over with it. One operation is then both.
  template <semantics::step Kind, typename Step>
  static constexpr bool borrows() {
    using semantics::step;
    if constexpr (!std::is_same_v<reached_t<Step, true>,
                                  reached_t<Step, false>>) {
      return false;
    } else {
      return Kind == step::copy || Kind == step::element_pointer ||
             Kind == step::maybe_element || Kind == step::maybe_element_copy ||
             (Kind == step::element && !releases_element_v<T>);
    }
  }

  // An operation of a step of a kind, for an object the request owns, for
  // one it does not, or for both, where the table has it.
  template <semantics::step Kind, typename Step, bool Owned, bool Borrowed,
            std::uint32_t Bit = operation_mask(Kind)>
  static constexpr erased_operation erase_operation() {
    erased_operation made;
    constexpr bool transient = makes_transient(Kind);
    if constexpr ((Mask & Bit) != 0 && Borrowed) {
      made.borrowed = &apply<Step, false, transient>;
      made.made = &operation_table<made_t<Step, false>, Leaf, Interface>::value;
      made.has_borrowed = true;
    }
    if constexpr ((Mask & Bit) != 0 && Owned) {
      made.owned = owned_operation<Kind, Step, Borrowed, transient>();
      made.made = &operation_table<made_t<Step, true>, Leaf, Interface>::value;
      made.has_owned = true;
    }
    return made;
  }

  template <semantics::step Kind, typename Step, bool Borrowed, bool Transient>
  static constexpr operation_function owned_operation() {
    if constexpr (Borrowed) {
      if constexpr (borrows<Kind, Step>()) {
        return &apply<Step, false, Transient>;
      }
    }
    return &apply<Step, true, Transient>;
  }

  template <semantics::step Kind, typename Step = execution::operation<Kind>>
  static constexpr erased_operation both() {
    return erase_operation<Kind, Step, true, true>();
  }
  template <semantics::step Kind, typename Step = execution::operation<Kind>,
            std::uint32_t Bit = operation_mask(Kind)>
  static constexpr erased_operation owned() {
    return erase_operation<Kind, Step, true, false, Bit>();
  }

  template <std::size_t K> static constexpr erased_operation run() {
    erased_operation made;
    if constexpr (std::is_same_v<T, type> && facts::template runs<K>()) {
      using nested_type = std::remove_cv_t<typename nested<K>::type>;
      made.owned = &wrap_operation<type, Leaf, Interface, nested_type>;
      made.has_owned = true;
    }
    return made;
  }

  template <std::size_t... K>
  static constexpr void add_runs(full_table &made, std::index_sequence<K...>) {
    ((made.wrap_runs[K + 1] = run<K + 1>()), ...);
  }

  static constexpr bool mask_has(semantics::step kind) {
    return (Mask & operation_mask(kind)) != 0;
  }

  // The steps from a layer to its element.
  static constexpr void elements(full_table &made) {
    using semantics::step;
    using element_type = typename element<>::type;
    auto index = [&](step kind) -> erased_operation & {
      return made.operations[static_cast<std::size_t>(kind)];
    };
    index(step::element) = both<step::element>();
    index(step::element_pointer) = both<step::element_pointer>();
    if constexpr (has_empty_value<std::remove_cv_t<element_type>>::value) {
      index(step::maybe_element) = both<step::maybe_element>();
      if constexpr (!std::is_pointer_v<type> &&
                    facts::template copyable<element_type>() &&
                    !std::is_array_v<element_type>) {
        index(step::maybe_element_copy) = both<step::maybe_element_copy>();
      }
    }
  }

  static constexpr full_table make() {
    full_table made;
    using semantics::step;
    auto index = [&](step kind) -> erased_operation & {
      return made.operations[static_cast<std::size_t>(kind)];
    };
    // Each operation the mask has, where C++ applies its step to the type.
    if constexpr (mask_has(step::copy)) {
      if constexpr (facts::template copyable<T>()) {
        index(step::copy) = both<step::copy>();
      }
    }
    if constexpr (mask_has(step::decay) || mask_has(step::hand_over)) {
      if constexpr (std::is_array_v<type> &&
                    is_complete_v<std::remove_extent_t<type>>) {
        index(step::decay) = both<step::decay>();
        index(step::hand_over) = owned<step::hand_over>();
      }
    }
    if constexpr (mask_has(step::element) || mask_has(step::element_pointer) ||
                  mask_has(step::maybe_element) ||
                  mask_has(step::maybe_element_copy)) {
      if constexpr (facts::layer()) {
        elements(made);
      }
    }
    if constexpr (mask_has(step::release) || (Mask & release_array_mask) != 0) {
      if constexpr (std::is_pointer_v<type> &&
                    std::is_object_v<std::remove_pointer_t<type>> &&
                    is_complete_v<std::remove_pointer_t<type>>) {
        index(step::release) = owned<step::release, release_operation<false>>();
        made.release_array =
            owned<step::release, release_operation<true>, release_array_mask>();
      }
    }
    if constexpr (mask_has(step::transfer)) {
      if constexpr (transfers()) {
        index(step::transfer) = owned<step::transfer>();
      }
    }
    if constexpr (mask_has(step::unknown_bound)) {
      index(step::unknown_bound) = erase_operation<
          step::unknown_bound, execution::operation<step::unknown_bound>,
          converts_unknown_bound<true>(), converts_unknown_bound<false>()>();
    }
    if constexpr (mask_has(step::upcast)) {
      if constexpr (upcasts<true>() || upcasts<false>()) {
        index(step::upcast) =
            erase_operation<step::upcast, execution::operation<step::upcast>,
                            upcasts<true>(), upcasts<false>()>();
      }
    }
    if constexpr (std::is_same_v<T, type> && (Mask & makers_mask) != 0) {
      add_runs(made, std::make_index_sequence<semantics::max_layers>{});
      takes_pointers(made);
    }
    if constexpr (std::is_same_v<T, type> && (Mask & qualified_mask) != 0) {
      add_qualified(made, std::index_sequence<1, 2, 3>{});
    }
    return made;
  }

  // The pointer or the handle the type is, made of one of an element of
  // fewer qualifiers: the qualifiers of its element without Adds.
  template <std::uint8_t Adds> static constexpr erased_operation qualified() {
    erased_operation made;
    constexpr std::uint8_t quals = element_quals<type>();
    if constexpr ((quals & Adds) == Adds) {
      using source =
          requalified_t<type, static_cast<std::uint8_t>(quals & ~Adds)>;
      if constexpr (std::is_constructible_v<type, source &&>) {
        made.owned = &qualified_operation<type, source, true>;
        made.has_owned = true;
      }
      if constexpr (std::is_constructible_v<type, const source &>) {
        made.borrowed = &qualified_operation<type, source, false>;
        made.has_borrowed = true;
      }
    }
    return made;
  }

  template <std::size_t... Adds>
  static constexpr void add_qualified(full_table &made,
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

  template <bool Owned> static constexpr bool converts_unknown_bound() {
    // NOLINTBEGIN(bugprone-branch-clone)
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
    // NOLINTEND(bugprone-branch-clone)
  }

  static constexpr bool transfers() {
    // NOLINTBEGIN(bugprone-branch-clone)
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
    // NOLINTEND(bugprone-branch-clone)
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
    // NOLINTBEGIN(bugprone-branch-clone)
    if constexpr (std::is_same_v<Leaf, Interface>) {
      return false;
    } else if constexpr (std::is_same_v<type, Leaf>) {
      return true;
    } else if constexpr (!holds_leaf_apart<type>()) {
      return false;
    } else if constexpr (std::is_pointer_v<type>) {
      return true;
    } else {
      using made_type = typename upcast_type<T, Leaf, Interface>::type;
      if constexpr (Owned) {
        return std::is_constructible_v<made_type, T &&>;
      } else {
        return std::is_constructible_v<made_type, const T &>;
      }
    }
    // NOLINTEND(bugprone-branch-clone)
  }

  // The handle the type is, made of a pointer it takes over, or the
  // observer, of one it does not.
  // NOLINTNEXTLINE(readability-function-size)
  static constexpr void takes_pointers(full_table &made) {
    if constexpr (type_traits<type>::enabled && !std::is_pointer_v<type> &&
                  !is_alternative_type_v<type> && is_complete_v<type>) {
      if constexpr (is_complete_v<std::remove_extent_t<
                        typename type_traits<type>::value_type>>) {
        // Of a pointer to the element of no qualifiers, which the handle holds
        // of those it needs.
        using element_type = std::remove_cv_t<typename element<>::type>;
        if constexpr (type_traits<type>::is_owning_handle) {
          if constexpr (std::is_array_v<element_type>) {
            using pointer = std::remove_extent_t<element_type> *;
            if constexpr (std::is_constructible_v<type, pointer>) {
              made.adopted_array = {&adopted_operation<type, pointer>, nullptr,
                                    nullptr, true, false};
              // A handle of an array of a known bound takes over a pointer to
              // a whole one too, as the pointer to its first element.
              if constexpr (std::extent_v<element_type> != 0) {
                made.adopted = {&adopted_operation<type, element_type *, true>,
                                nullptr, nullptr, true, false};
              }
            }
          } else if constexpr (std::is_constructible_v<type, element_type *>) {
            made.adopted = {&adopted_operation<type, element_type *>, nullptr,
                            nullptr, true, false};
          }
        } else if constexpr (wrapper_kind_v<layer_identity_t<type>>.observes &&
                             std::is_constructible_v<type, element_type *>) {
          made.observing = {&observing_operation<type, element_type *>,
                            &observing_operation<type, element_type *>, nullptr,
                            true, true};
        }
      }
    }
  }

  static constexpr full_table full = make();

  static constexpr std::size_t count() {
    std::size_t made = 0;
    for (std::uint8_t slot_index = 0; slot_index < operation_slot(slot::count);
         ++slot_index) {
      made += has(slot_operation(full, slot_index)) ? 1 : 0;
    }
    return made;
  }

  // The slot of the n-th operation the table has.
  static constexpr std::uint8_t nth(std::size_t index) {
    for (std::uint8_t slot_index = 0; slot_index < operation_slot(slot::count);
         ++slot_index) {
      if (has(slot_operation(full, slot_index)) && index-- == 0) {
        return slot_index;
      }
    }
    return 0;
  }

  template <std::size_t Count, std::size_t... I>
  static constexpr std::array<slot_entry, Count + 1>
  compact(std::index_sequence<I...>) {
    return {{{nth(I), slot_operation(full, nth(I))}..., {}}};
  }
};

// The address a pointer of type T points to.
template <typename T> void *pointee_address(void *pointer) {
  return const_cast<void *>(static_cast<const volatile void *>(
      *static_cast<std::remove_cv_t<T> *>(pointer)));
}

template <typename T> constexpr void *(*pointee_function())(void *) {
  if constexpr (std::is_pointer_v<std::remove_cv_t<T>>) {
    return &pointee_address<T>;
  } else {
    return nullptr;
  }
}

template <typename T, typename Leaf, typename Interface, std::uint32_t Mask>
struct operation_table {
  using maker = table_maker<T, Leaf, Interface, Mask>;
  static constexpr std::size_t count = maker::count();
  static constexpr std::array<slot_entry, count + 1> entries =
      maker::template compact<count>(std::make_index_sequence<count>{});
  static constexpr type_table value{pointee_function<T>(), entries.data(),
                                    count};
};

// A table of no operations: of the type of what a step makes that nothing is
// taken of but the address a pointer points to.
template <typename T, typename Leaf, typename Interface>
struct operation_table<T, Leaf, Interface, 0> {
  static constexpr type_table value{pointee_function<T>(), nullptr, 0};
};

} // namespace dingo::detail::execution
