//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/memory/object_layout.h>
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
using step_function = erased_carrier (*)(erased_carrier source, bool stable,
                                         runtime_store &store);

struct type_table;

// An operation on an object of a type: for one the request owns and for one
// it does not, none where C++ does not apply it, and the table of the type
// of what it makes.
struct operation {
  step_function owned = nullptr;
  step_function borrowed = nullptr;
  const type_table *made = nullptr;
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
  operation steps[step_kinds] = {};
  operation release_array = {};
  operation wrap_runs[semantics::max_layers + 1] = {};
  operation adopted = {};
  operation adopted_array = {};
  operation observing = {};
  operation qualified[4] = {};
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

constexpr std::uint8_t step_slot(semantics::step kind) {
  return static_cast<std::uint8_t>(kind);
}
constexpr std::uint8_t step_slot(slot index, std::size_t depth = 0) {
  return static_cast<std::uint8_t>(static_cast<std::size_t>(index) + depth);
}

struct slot_entry {
  std::uint8_t slot;
  operation step;
};

// A table as a runtime resolution keeps it: the operations it has, as many
// as there are.
struct type_table {
  void *(*pointee)(void *pointer);
  const slot_entry *entries;
  std::size_t count;

  // The operation at a slot, or one of no function.
  const operation &operator[](std::uint8_t slot) const {
    for (std::size_t index = 0; index < count; ++index) {
      if (entries[index].slot == slot) {
        return entries[index].step;
      }
    }
    static constexpr operation none{};
    return none;
  }
};

// The operation at a slot of a full table.
constexpr const operation &slot_operation(const full_table &table,
                                          std::uint8_t index) {
  if (index < step_kinds) {
    return table.steps[index];
  }
  if (index == step_slot(slot::release_array)) {
    return table.release_array;
  }
  if (index < step_slot(slot::adopted)) {
    return table.wrap_runs[index - step_slot(slot::wrap_runs)];
  }
  if (index == step_slot(slot::adopted)) {
    return table.adopted;
  }
  if (index == step_slot(slot::adopted_array)) {
    return table.adopted_array;
  }
  if (index == step_slot(slot::observing)) {
    return table.observing;
  }
  return table.qualified[index - step_slot(slot::qualified)];
}

constexpr bool has(const operation &step) {
  return step.owned != nullptr || step.borrowed != nullptr;
}

// The operations a table of a binding has: a bit for each kind of step from
// its type, one for the release of a pointer to the first element of an
// array, and one for the steps that make an object of it.
constexpr std::uint32_t step_mask(semantics::step kind) {
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
             Kind == step::element_or_empty ||
             Kind == step::element_or_empty_copy ||
             (Kind == step::element && !releases_element_v<T>);
    }
  }

  // An operation of a step of a kind, for an object the request owns, for
  // one it does not, or for both, where the table has it.
  template <semantics::step Kind, typename Step, bool Owned, bool Borrowed,
            std::uint32_t Bit = step_mask(Kind)>
  static constexpr operation step_operation() {
    operation made;
    constexpr bool transient = makes_transient(Kind);
    if constexpr ((Mask & Bit) != 0 && Borrowed) {
      made.borrowed = &apply<Step, false, transient>;
      made.made = &operation_table<made_t<Step, false>, Leaf, Interface>::value;
    }
    if constexpr ((Mask & Bit) != 0 && Owned) {
      made.owned = owned_operation<Kind, Step, Borrowed, transient>();
      made.made = &operation_table<made_t<Step, true>, Leaf, Interface>::value;
    }
    return made;
  }

  template <semantics::step Kind, typename Step, bool Borrowed, bool Transient>
  static constexpr step_function owned_operation() {
    if constexpr (Borrowed) {
      if constexpr (borrows<Kind, Step>()) {
        return &apply<Step, false, Transient>;
      }
    }
    return &apply<Step, true, Transient>;
  }

  template <semantics::step Kind, typename Step = execution::step<Kind>>
  static constexpr operation both() {
    return step_operation<Kind, Step, true, true>();
  }
  template <semantics::step Kind, typename Step = execution::step<Kind>,
            std::uint32_t Bit = step_mask(Kind)>
  static constexpr operation owned() {
    return step_operation<Kind, Step, true, false, Bit>();
  }

  template <std::size_t K> static constexpr operation run() {
    operation made;
    if constexpr (std::is_same_v<T, type> && facts::template runs<K>()) {
      using nested_type = std::remove_cv_t<typename nested<K>::type>;
      made.owned = &wrap_operation<type, Leaf, Interface, nested_type>;
    }
    return made;
  }

  template <std::size_t... K>
  static constexpr void add_runs(full_table &made, std::index_sequence<K...>) {
    ((made.wrap_runs[K + 1] = run<K + 1>()), ...);
  }

  static constexpr bool wants(semantics::step kind) {
    return (Mask & step_mask(kind)) != 0;
  }

  // The steps from a layer to its element.
  static constexpr void elements(full_table &made) {
    using semantics::step;
    using element_type = typename element<>::type;
    auto index = [&](step kind) -> operation & {
      return made.steps[static_cast<std::size_t>(kind)];
    };
    index(step::element) = both<step::element>();
    index(step::element_pointer) = both<step::element_pointer>();
    if constexpr (has_empty_value<std::remove_cv_t<element_type>>::value) {
      index(step::element_or_empty) = both<step::element_or_empty>();
      if constexpr (!std::is_pointer_v<type> &&
                    facts::template copyable<element_type>() &&
                    !std::is_array_v<element_type>) {
        index(step::element_or_empty_copy) =
            both<step::element_or_empty_copy>();
      }
    }
  }

  static constexpr full_table make() {
    full_table made;
    using semantics::step;
    auto index = [&](step kind) -> operation & {
      return made.steps[static_cast<std::size_t>(kind)];
    };
    // Each operation the mask has, where C++ applies its step to the type.
    if constexpr (wants(step::copy)) {
      if constexpr (facts::template copyable<T>()) {
        index(step::copy) = both<step::copy>();
      }
    }
    if constexpr (wants(step::decay) || wants(step::hand_over)) {
      if constexpr (std::is_array_v<type> &&
                    is_complete_v<std::remove_extent_t<type>>) {
        index(step::decay) = both<step::decay>();
        index(step::hand_over) = owned<step::hand_over>();
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
        index(step::release) = owned<step::release, release_step<false>>();
        made.release_array =
            owned<step::release, release_step<true>, release_array_mask>();
      }
    }
    if constexpr (wants(step::convert)) {
      if constexpr (converts()) {
        index(step::convert) = owned<step::convert>();
      }
    }
    if constexpr (wants(step::unknown_bound)) {
      index(step::unknown_bound) = step_operation<
          step::unknown_bound, execution::step<step::unknown_bound>,
          converts_unknown_bound<true>(), converts_unknown_bound<false>()>();
    }
    if constexpr (wants(step::upcast)) {
      if constexpr (upcasts<true>() || upcasts<false>()) {
        index(step::upcast) =
            step_operation<step::upcast, execution::step<step::upcast>,
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
  template <std::uint8_t Adds> static constexpr operation qualified() {
    operation made;
    constexpr std::uint8_t quals = element_quals<type>();
    if constexpr ((quals & Adds) == Adds) {
      using source =
          requalified_t<type, static_cast<std::uint8_t>(quals & ~Adds)>;
      if constexpr (std::is_constructible_v<type, source &&>) {
        made.owned = &qualified_operation<type, source, true>;
      }
      if constexpr (std::is_constructible_v<type, const source &>) {
        made.borrowed = &qualified_operation<type, source, false>;
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
      using made_type = typename upcast_type<T, Leaf, Interface>::type;
      if constexpr (Owned) {
        return std::is_constructible_v<made_type, T &&>;
      } else {
        return std::is_constructible_v<made_type, const T &>;
      }
    }
  }

  // The handle the type is, made of a pointer it takes over, or the
  // observer, of one it does not.
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
                                    nullptr};
              // A handle of an array of a known bound takes over a pointer to
              // a whole one too, as the pointer to its first element.
              if constexpr (std::extent_v<element_type> != 0) {
                made.adopted = {&adopted_operation<type, element_type *, true>,
                                nullptr, nullptr};
              }
            }
          } else if constexpr (std::is_constructible_v<type, element_type *>) {
            made.adopted = {&adopted_operation<type, element_type *>, nullptr,
                            nullptr};
          }
        } else if constexpr (wrapper_kind_v<layer_identity_t<type>>.observes &&
                             std::is_constructible_v<type, element_type *>) {
          made.observing = {&observing_operation<type, element_type *>,
                            &observing_operation<type, element_type *>,
                            nullptr};
        }
      }
    }
  }

  static constexpr full_table full = make();

  static constexpr std::size_t count() {
    std::size_t made = 0;
    for (std::uint8_t slot_index = 0; slot_index < step_slot(slot::count);
         ++slot_index) {
      made += has(slot_operation(full, slot_index)) ? 1 : 0;
    }
    return made;
  }

  // The slot of the n-th operation the table has.
  static constexpr std::uint8_t nth(std::size_t index) {
    for (std::uint8_t slot_index = 0; slot_index < step_slot(slot::count);
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

// The identity of a layer, as an address both sides of a runtime resolution
// compare: of the layers of C++, or of the identity of a wrapper.
struct array_pointer_layer_tag {};

template <typename T> inline constexpr char identity_tag_v = 0;

template <typename Tag> constexpr const void *identity_v() {
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
  const void *sharing_identity;
  const void *unknown_bound_identity;
};

// The identity tag of a layer, or none.
template <typename Identity> constexpr const void *layer_identity_v() {
  if constexpr (std::is_void_v<Identity>) {
    return nullptr;
  } else {
    return &identity_tag_v<Identity>;
  }
}

template <typename Identity> constexpr runtime_layer runtime_layer_v() {
  semantics::layer_kind kind = wrapper_kind_v<Identity>;
  kind.converts = 0;
  kind.converts_unknown_bound = 0;
  using unknown_bound_target =
      std::conditional_t<converts_unknown_bound<Identity>(),
                         typename unknown_bound_layer<Identity>::type, void>;
  return {kind, &identity_tag_v<Identity>,
          layer_identity_v<typename shared_layer<Identity>::type>(),
          layer_identity_v<unknown_bound_target>()};
}

// The wrapper layers of a universe, in its order.
template <typename Identities> struct runtime_layers;
template <typename... Identities>
struct runtime_layers<type_list<Identities...>> {
  static constexpr std::size_t count = sizeof...(Identities);
  static constexpr runtime_layer value[count + 1] = {
      runtime_layer_v<Identities>()..., {}};
};

// A request as a runtime resolution plans it: its layers in a universe of
// its own, whose wrapper layers it gives, what it asks for, and the table of
// each type it spells, from its shape to its element, which make the layers
// it makes.
struct request_layout {
  const runtime_layer *layers;
  std::size_t layer_count;
  semantics::request goal;
  semantics::category category;
  const type_table *const *tables;
};

// A binding as a runtime resolution plans it: its layer sources, the states
// of a search in its order, in a universe of its own, whose wrapper layers
// it gives, and the table of the type of each.
// How the route of a state of the closure of a binding gives a value the
// request takes: whether it makes it of what the route reaches, a copy of
// the qualifiers quals, rather than taking what the route reaches.
struct route_finish {
  bool copies = false;
  std::uint8_t quals = 0;
};

// The key of a type a request is of, without its own qualifiers, which a
// binding finds the answers of its closure for a request by.
template <typename T> inline constexpr char shape_key = 0;

// How a state of the closure of a binding satisfies a request, as the code
// compiles: its route, whether it is the one of a value, how the request
// takes what it reaches and whether what it gives lives as long as the
// container.
struct closed_answer {
  bool found = false;
  bool value_route = false;
  bool stable_address = false;
  std::uint16_t route = 0;
  semantics::satisfaction how = semantics::satisfaction::refer;
  std::uint8_t quals = 0;
};

// The answers of a request, for each lifetime it may need.
struct closed_answers {
  closed_answer stable;
  closed_answer transient;
};

// The answers of the closure for the requests of the type of the key shape:
// of each category, of an object of each qualifiers, at
// first + category * 4 + qualifiers.
inline constexpr std::size_t shape_answer_count = 7 * 4;

struct closed_group {
  const void *shape;
  std::uint16_t first;
};

struct binding_layout {
  // The layer sources of the binding, closed under every step a request
  // takes but a wrap: the first source_count are those the binding steps
  // into, of which it has the table of the type of each.
  const semantics::state *sources;
  std::size_t source_count;
  std::size_t state_count;
  semantics::context context;
  const runtime_layer *layers;
  std::size_t layer_count;
  const type_table *const *tables;
  // How the route of each state gives a value.
  const route_finish *finishes;
  // The answers of the closure, by the type a request is of.
  const closed_group *groups;
  std::size_t group_count;
  const closed_answers *answers;
};

// A plan as a runtime resolution runs it: the operations of the instructions
// of its program, each with whether what it makes lives as long as the
// container; how it satisfies the request, how the request takes the value of
// what it reaches, and whether what it gives the request the address of
// lives as long as the container.
struct runtime_plan {
  struct call {
    step_function owned;
    step_function borrowed;
    bool stable;
  };
  bool found = false;
  // The route of the binding the plan takes first, to a state of its
  // closure, and whether it is the one of a value.
  std::uint16_t route = 0;
  bool value_route = false;
  semantics::satisfaction how = semantics::satisfaction::refer;
  finish last = finish::satisfy;
  std::uint8_t depth = 0;
  std::uint8_t quals = 0;
  bool stable_address = false;
  // The table of the type of what the plan reaches.
  const type_table *reached = nullptr;
  std::size_t count = 0;
  call calls[semantics::max_plan] = {};
};

// The plans of a request of a binding, for a request that needs what it
// refers to to live as long as the container, and for one that needs it to
// live as long as the request: of one search, in the universe of the
// binding, which the layers of the request it does not have follow, from the
// binding's layer sources.
struct runtime_plans {
  runtime_plan stable;
  runtime_plan transient;
};

inline runtime_plan make_runtime_plan(const semantics::plan &plan,
                                      const binding_layout &binding,
                                      const request_layout &request,
                                      const semantics::request &goal);

inline runtime_plans plan_runtime(const binding_layout &binding,
                                  const request_layout &request) {
  runtime_plans made;
  // The layers of the request in the universe of the binding: those it
  // knows by their index, any other after them.
  semantics::layer mapped[semantics::max_kinds] = {0, 1, 2, 3};
  bool foreign = false;
  for (std::size_t index = 0; index < request.layer_count && !foreign;
       ++index) {
    std::size_t binding_index = 0;
    while (binding_index < binding.layer_count &&
           binding.layers[binding_index].identity !=
               request.layers[index].identity) {
      ++binding_index;
    }
    foreign = binding_index == binding.layer_count;
    mapped[4 + index] = static_cast<semantics::layer>(4 + binding_index);
  }
  semantics::context context = binding.context;
  if (foreign) {
    // Of the thread, as the search is: a universe holds the kinds of every
    // standard layer, which making one copies.
    thread_local semantics::universe merged;
    // The universe of the binding, which the layers of the request it does
    // not know follow, and the conversions between them all.
    merged = *binding.context.layers;
    const void *identities[semantics::max_kinds] = {};
    const void *targets[semantics::max_kinds] = {};
    const void *unknown_bound_targets[semantics::max_kinds] = {};
    for (std::size_t index = 0; index < 4; ++index) {
      identities[index] = language_identities[index];
    }
    for (std::size_t index = 0; index < binding.layer_count; ++index) {
      identities[4 + index] = binding.layers[index].identity;
      targets[4 + index] = binding.layers[index].sharing_identity;
      unknown_bound_targets[4 + index] =
          binding.layers[index].unknown_bound_identity;
    }
    for (std::size_t index = 0; index < request.layer_count; ++index) {
      const runtime_layer &layer = request.layers[index];
      std::size_t merged_index = 4;
      while (merged_index < merged.count &&
             identities[merged_index] != layer.identity) {
        ++merged_index;
      }
      if (merged_index == merged.count) {
        if (merged.count == semantics::max_kinds) {
          return made;
        }
        identities[merged_index] = layer.identity;
        targets[merged_index] = layer.sharing_identity;
        unknown_bound_targets[merged_index] = layer.unknown_bound_identity;
        merged.add(layer.kind);
      }
      mapped[4 + index] = static_cast<semantics::layer>(merged_index);
    }
    for (std::size_t index = 4; index < merged.count; ++index) {
      merged.kinds[index].converts = 0;
      merged.kinds[index].converts_unknown_bound = 0;
      for (std::size_t target = 4; target < merged.count; ++target) {
        if (targets[index] != nullptr && identities[target] == targets[index]) {
          merged.kinds[index].converts |= std::uint32_t{1} << target;
        }
        if (unknown_bound_targets[index] != nullptr &&
            identities[target] == unknown_bound_targets[index]) {
          merged.kinds[index].converts_unknown_bound |= std::uint32_t{1}
                                                        << target;
        }
      }
    }
    context.layers = &merged;
  }
  semantics::request goal = request.goal;
  for (std::size_t index = 0; index < goal.depth; ++index) {
    goal.layers[index] = mapped[goal.layers[index]];
  }
  goal.element = context.abstract ? semantics::element::abstract
                                  : semantics::element::leaf;

  // The search of a thread, cleared rather than made: what makes one is its
  // states, which a search reads only once it added them. Planning runs no
  // code of a user, so it is never planning already on the thread.
  thread_local semantics::search found;
  found.clear();
  // The closed states are distinct already, so they are not hashed: an object
  // the request reaches again is only a state more.
  for (std::size_t index = 0; index < binding.state_count; ++index) {
    found.states[index] = binding.sources[index];
  }
  found.count = binding.state_count;
  found.closed = binding.state_count;
  semantics::extend_reach(found, binding.state_count,
                          binding.context.layers->count, context, goal);
  // The plans of the lifetimes differ in what satisfies the request alone.
  made.stable = make_runtime_plan(
      semantics::answer_plan(
          found, semantics::satisfy(found, goal, request.category,
                                    semantics::lifetime::stable, context)),
      binding, request, goal);
  made.transient = make_runtime_plan(
      semantics::answer_plan(
          found, semantics::satisfy(found, goal, request.category,
                                    semantics::lifetime::request, context)),
      binding, request, goal);
  return made;
}

// A plan as a runtime resolution runs it: compiled into the operations of
// the tables of the types it steps through.
inline runtime_plan make_runtime_plan(const semantics::plan &plan,
                                      const binding_layout &binding,
                                      const request_layout &request,
                                      const semantics::request &goal) {
  runtime_plan made;
  if (!plan.found) {
    return made;
  }

  const program compiled = compile_program(plan, binding.source_count);
  // The instructions into states of the closure are the binding's route;
  // the rest the request takes, of the tables of the types it spells.
  std::size_t first = 0;
  while (first < compiled.count &&
         plan.states[compiled.entries[first].last + 1] < binding.state_count) {
    ++first;
  }
  if (compiled.last == finish::copy && first == compiled.count &&
      plan.states[plan.count] < binding.state_count) {
    made.route = plan.states[plan.count];
    made.value_route = true;
  } else {
    made.route = first ? plan.states[compiled.entries[first - 1].last + 1]
                       : std::uint16_t{0};
  }
  // The table of the type of what the plan reached so far: of a state of
  // the closure, only of one a step after its route takes an operation of.
  const type_table *table = binding.tables[made.route];
  for (std::size_t index = first; index < compiled.count; ++index) {
    const instruction &each = compiled.entries[index];
    const semantics::object &source = plan.objects[each.first];
    const semantics::object &target = plan.objects[each.last + 1];
    const std::uint16_t state = plan.states[each.last + 1];
    // The table of what an instruction makes a layer of: of a source of the
    // binding, or of a type the request spells, by its depth in the request.
    const bool tabled = state < binding.source_count;
    const type_table *maker = tabled
                                  ? binding.tables[state]
                                  : request.tables[goal.depth - target.depth];
    const operation *step = nullptr;
    switch (each.kind) {
    case semantics::step::wrap:
      step = &(*maker)[step_slot(slot::wrap_runs, each.wraps)];
      break;
    case semantics::step::adopt:
      step = &(
          *maker)[target.layers[1] == semantics::array_layer && target.depth > 1
                      ? step_slot(slot::adopted_array)
                      : step_slot(slot::adopted)];
      break;
    case semantics::step::observe:
      step = &(*maker)[step_slot(slot::observing)];
      break;
    case semantics::step::qualify:
      step = &(*maker)[step_slot(
          slot::qualified,
          static_cast<std::uint8_t>(target.quals[1] & ~source.quals[1]))];
      break;
    case semantics::step::release:
      step = &(*table)[source.layers[0] == semantics::array_pointer_layer
                           ? step_slot(slot::release_array)
                           : step_slot(each.kind)];
      break;
    default:
      if (table == nullptr) {
        return {};
      }
      step = &(*table)[step_slot(each.kind)];
      break;
    }
    made.calls[made.count++] = {step->owned, step->borrowed,
                                target.lifetimes[0] ==
                                    semantics::lifetime::stable};
    // The table of what a step of the binding makes is that of its source;
    // of what a step of a request makes, that its operation names.
    table = tabled ? binding.tables[state]
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
  const semantics::object &reached = plan.objects[plan.count];
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
  std::uint16_t state_class[semantics::max_states] = {};
  std::uint16_t edge_of[semantics::max_states] = {};
  std::uint16_t edges[semantics::max_states] = {};
  std::size_t edge_count = 0;
};

constexpr bool same_type(const semantics::object &a,
                         const semantics::object &b) {
  if (a.depth != b.depth || a.element != b.element || a.origin != b.origin) {
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

constexpr std::uint32_t type_hash(const semantics::object &object) {
  std::uint32_t hash = 2166136261u;
  hash = (hash ^ object.depth) * 16777619u;
  hash = (hash ^ static_cast<unsigned>(object.element)) * 16777619u;
  hash = (hash ^ static_cast<unsigned>(object.origin)) * 16777619u;
  for (std::size_t index = 0; index < object.depth; ++index) {
    hash = (hash ^ object.layers[index]) * 16777619u;
  }
  for (std::size_t index = 0; index <= object.depth; ++index) {
    hash = (hash ^ object.quals[index]) * 16777619u;
  }
  return hash;
}

// Whether each state of the sources of a binding is on the way to what a
// request takes: in a context that requests the interface, a request takes
// nothing of what holds the leaf, which is only on the way to what holds
// the interface.
constexpr void used_states(const semantics::search &found, bool abstract,
                           bool *used) {
  for (std::size_t index = 0; index < found.count; ++index) {
    used[index] = !abstract;
  }
  for (std::size_t index = found.count; index-- > 0;) {
    const semantics::state &each = found.states[index];
    if (each.reached.element == semantics::element::abstract) {
      used[index] = true;
    }
    if (used[index] && each.parent != semantics::no_state) {
      used[each.parent] = true;
    }
  }
}

constexpr state_classes classify_states(const semantics::search &found,
                                        bool abstract = false) {
  state_classes made;
  std::uint16_t types[semantics::search_slots] = {};
  std::uint16_t steps[semantics::search_slots] = {};
  bool used[semantics::max_states] = {};
  used_states(found, abstract, used);
  for (std::size_t index = 0; index < found.count; ++index) {
    const semantics::state &each = found.states[index];
    std::size_t slot = type_hash(each.reached) % semantics::search_slots;
    while (types[slot] != 0 &&
           !same_type(found.states[types[slot] - 1].reached, each.reached)) {
      slot = (slot + 1) % semantics::search_slots;
    }
    if (types[slot] == 0) {
      types[slot] = static_cast<std::uint16_t>(index + 1);
    }
    made.state_class[index] = static_cast<std::uint16_t>(types[slot] - 1);
    if (index == 0 || !used[index]) {
      continue;
    }
    const std::uint16_t source = made.state_class[each.parent];
    const std::uint16_t target = made.state_class[index];
    slot = ((source * 31u + target) * 31u + static_cast<unsigned>(each.via)) %
           semantics::search_slots;
    while (steps[slot] != 0) {
      const std::uint16_t edge = made.edges[steps[slot] - 1];
      const semantics::state &other = found.states[edge];
      if (made.state_class[other.parent] == source &&
          made.state_class[edge] == target && other.via == each.via) {
        break;
      }
      slot = (slot + 1) % semantics::search_slots;
    }
    if (steps[slot] == 0) {
      made.edges[made.edge_count++] = static_cast<std::uint16_t>(index);
      steps[slot] = static_cast<std::uint16_t>(made.edge_count);
    }
    made.edge_of[index] = static_cast<std::uint16_t>(steps[slot] - 1);
  }
  return made;
}

// The layer sources of a binding, as it publishes them: those a request
// takes something of, and those on the way to them, in their order; and the
// table of the type of each, which the steps into it are checked against as
// the code compiles.
// Whether a step makes a layer of what it is taken from, which the table of
// what it makes has.
constexpr bool step_makes(semantics::step kind) {
  return kind == semantics::step::wrap || kind == semantics::step::adopt ||
         kind == semantics::step::observe;
}

// The kinds of the steps a request takes from an object, as a mask.
struct step_kinds_sink {
  std::uint32_t kinds = 0;
  // It takes a step of every outer layer.
  constexpr bool takes(semantics::step, semantics::layer, std::size_t) const {
    return true;
  }
  constexpr bool takes_quals(std::size_t, std::uint8_t, std::size_t) const {
    return true;
  }
  constexpr void operator()(semantics::step kind, const semantics::object &) {
    kinds |= std::uint32_t{1} << static_cast<unsigned>(kind);
  }
};

constexpr std::uint32_t request_kinds(const semantics::object &object,
                                      const semantics::context &context) {
  step_kinds_sink sink;
  semantics::each_request_step(object, context, sink);
  return sink.kinds;
}

// The semantics of the closure of a binding of a shape, of no C++ type: the
// classes of the states of its layer sources, those a request takes, the
// published states closed under every step a request takes but a wrap, and
// the plan and the program of the route of each state. Bindings of every
// type of a shape share them.
template <typename Shape> struct closed_semantics {
  static constexpr const semantics::search &Sources = Shape::sources;
  static constexpr bool abstract = Shape::context.abstract;
  static constexpr semantics::context context() { return Shape::context; }
  static constexpr state_classes classes = classify_states(Sources, abstract);
  struct used_list {
    bool value[semantics::max_states] = {};
    std::uint16_t index[semantics::max_states] = {};
    std::uint16_t states[semantics::max_states] = {};
    std::size_t count = 0;
  };
  static constexpr used_list make_used() {
    used_list made;
    used_states(Sources, abstract, made.value);
    for (std::size_t index = 0; index < Sources.count; ++index) {
      if (made.value[index]) {
        made.index[index] = static_cast<std::uint16_t>(made.count);
        made.states[made.count++] = static_cast<std::uint16_t>(index);
      }
    }
    return made;
  }
  static constexpr used_list used = make_used();
  // The operations each class of states needs: the steps the binding takes
  // from it, the steps a request may take from it, and the wraps that make
  // it. A sole owner converts to the sharing handle its type traits name, and
  // a handle of an array of a known bound to the handle of an array of unknown
  // bound they name, which a request may bring into the universe: of every
  // source that is one.
  struct mask_list {
    std::uint32_t state_mask[semantics::max_states] = {};
  };
  static constexpr mask_list make_masks() {
    mask_list made;
    constexpr std::uint32_t taken = step_mask(semantics::step::copy) |
                                    step_mask(semantics::step::decay) |
                                    step_mask(semantics::step::hand_over) |
                                    step_mask(semantics::step::convert) |
                                    step_mask(semantics::step::unknown_bound);
    const semantics::context context = Shape::context;
    for (std::size_t index = 0; index < used.count; ++index) {
      const std::size_t state = used.states[index];
      const semantics::state &each = Sources.states[state];
      made.state_mask[classes.state_class[state]] |=
          (request_kinds(each.reached, context) & taken) |
          step_mask(semantics::step::convert) |
          step_mask(semantics::step::unknown_bound);
      if (state == 0) {
        continue;
      }
      if (step_makes(each.via)) {
        made.state_mask[classes.state_class[state]] |= makers_mask;
      } else if (each.via == semantics::step::release &&
                 Sources.states[each.parent].reached.layers[0] ==
                     semantics::array_pointer_layer) {
        made.state_mask[classes.state_class[each.parent]] |= release_array_mask;
      } else {
        made.state_mask[classes.state_class[each.parent]] |=
            step_mask(each.via);
        // A request makes the copy of what element_or_empty gives where it
        // takes it.
        if (each.via == semantics::step::element_or_empty_copy) {
          made.state_mask[classes.state_class[each.parent]] |=
              step_mask(semantics::step::element_or_empty);
        }
      }
    }
    return made;
  }
  static constexpr mask_list masks = make_masks();
  // How many wraps make a state at once, down to the first step that is
  // not one.
  static constexpr std::size_t run_length(std::size_t state) {
    std::size_t run = 0;
    while (Sources.states[state].via == semantics::step::wrap) {
      ++run;
      state = Sources.states[state].parent;
    }
    return run;
  }
  // The published states closed under every step a request takes but a
  // wrap, in the layers the binding knows: what a request reaches from. The
  // binding has the tables of the layer sources alone: a type a step of a
  // request makes is one the request spells, and C++ may not make every one
  // the semantics allow of a type it never spells.
  static constexpr semantics::search make_closure() {
    semantics::search found;
    for (std::size_t index = 0; index < used.count; ++index) {
      semantics::state each = Sources.states[used.states[index]];
      if (each.parent != semantics::no_state) {
        each.parent = used.index[each.parent];
      }
      found.add(each);
    }
    semantics::close_layer_sources(found, Shape::context);
    return found;
  }
  static constexpr semantics::search closure = make_closure();
  static_assert(!closure.overflow,
                "the stored object publishes more layer sources than "
                "semantics::max_states");

  template <std::size_t... I>
  static constexpr auto close(std::index_sequence<I...>) {
    struct list {
      semantics::state states[sizeof...(I)];
    };
    return list{{closure.states[I]...}};
  }
  static constexpr auto closed =
      close(std::make_index_sequence<closure.count>{});
  // The route of a state of the closure: the program of its plan from the
  // stored object, run as the code compiles, to what it reaches; of a value
  // the request takes (Value), which it makes of what the program reaches
  // when the step into the state is a copy.
  static constexpr semantics::answer route_answer(std::size_t state,
                                                  bool value) {
    semantics::answer made;
    made.found = true;
    made.state = static_cast<std::uint16_t>(state);
    made.how =
        value ? semantics::satisfaction::copy : semantics::satisfaction::refer;
    return made;
  }
  template <std::size_t State, bool Value>
  static constexpr semantics::plan route_plan =
      semantics::answer_plan(closure, route_answer(State, Value));
  template <std::size_t State, bool Value>
  static constexpr program route_program =
      compile_program(route_plan<State, Value>, used.count);
  template <std::size_t State> static constexpr route_finish route_finish_v() {
    return {route_program<State, true>.last == finish::copy,
            route_program<State, true>.quals};
  }
  template <std::size_t... I>
  static constexpr auto make_finishes(std::index_sequence<I...>) {
    struct list {
      route_finish entries[sizeof...(I)];
    };
    return list{{route_finish_v<I>()...}};
  }
  static constexpr auto route_finishes =
      make_finishes(std::make_index_sequence<closure.count>{});
};

// The answers of the closure of a binding of a shape, of the states C++
// makes of it, as the bits of Possible: of each shape of a request the
// closure serves, of each category and qualifiers of the object it is of.
template <typename Semantic, std::uint64_t... Possible>
struct closed_answer_table {
  static constexpr std::uint64_t possible_bits[] = {Possible..., 0};
  static constexpr bool possible(std::size_t state) {
    return ((possible_bits[state / 64] >> (state % 64)) & 1u) != 0;
  }
  static constexpr const semantics::search &closure = Semantic::closure;
  static constexpr const auto &route_finishes = Semantic::route_finishes;
  // The shapes the closure serves a request of: that of each state C++
  // makes, and what each raw pointer of them points to, of the qualifiers of
  // every level below the object; with a state of the shape, and whether it
  // is the pointer to it.
  static constexpr std::size_t shape_bound = 2 * closure.count;
  struct shape_list {
    semantics::request goals[shape_bound] = {};
    std::uint16_t states[shape_bound] = {};
    bool pointee[shape_bound] = {};
    std::size_t count = 0;
  };
  static constexpr bool same_goal(const semantics::request &a,
                                  const semantics::request &b) {
    if (a.depth != b.depth || a.element != b.element) {
      return false;
    }
    for (std::size_t index = 0; index < a.depth; ++index) {
      if (a.layers[index] != b.layers[index]) {
        return false;
      }
    }
    for (std::size_t index = 1; index <= a.depth; ++index) {
      if (a.quals[index] != b.quals[index]) {
        return false;
      }
    }
    return true;
  }
  static constexpr void add_shape(shape_list &made,
                                  const semantics::request &goal,
                                  std::uint16_t state, bool pointee) {
    for (std::size_t index = 0; index < made.count; ++index) {
      if (same_goal(made.goals[index], goal)) {
        return;
      }
    }
    made.goals[made.count] = goal;
    made.states[made.count] = state;
    made.pointee[made.count] = pointee;
    ++made.count;
  }
  static constexpr shape_list make_shapes() {
    shape_list made;
    const semantics::context context = Semantic::context();
    for (std::size_t state = 0; state < closure.count; ++state) {
      if (!possible(state)) {
        continue;
      }
      const semantics::object &object = closure.states[state].reached;
      semantics::request goal;
      goal.depth = object.depth;
      goal.element = object.element;
      goal.shape_depth = object.depth;
      for (std::size_t index = 0; index < object.depth; ++index) {
        goal.layers[index] = object.layers[index];
      }
      for (std::size_t index = 1; index <= object.depth; ++index) {
        goal.quals[index] = object.quals[index];
      }
      add_shape(made, goal, static_cast<std::uint16_t>(state), false);
      if (object.depth && context.kind(object.layers[0]).adopted) {
        semantics::request pointed;
        pointed.depth = static_cast<std::uint8_t>(object.depth - 1);
        pointed.element = object.element;
        pointed.shape_depth = pointed.depth;
        for (std::size_t index = 1; index < object.depth; ++index) {
          pointed.layers[index - 1] = object.layers[index];
        }
        for (std::size_t index = 2; index <= object.depth; ++index) {
          pointed.quals[index - 1] = object.quals[index];
        }
        add_shape(made, pointed, static_cast<std::uint16_t>(state), true);
      }
    }
    return made;
  }
  static constexpr shape_list shapes = make_shapes();

  // The answer of the closure of a request, as closed_plan makes it.
  static constexpr closed_answer find_answer(const semantics::request &goal,
                                             semantics::category taken,
                                             semantics::lifetime needed) {
    const semantics::answer result = semantics::satisfy_states(
        closure.states, closure.count, closure.overflow, goal, taken, needed,
        Semantic::context(), semantics::affected_cost{});
    closed_answer made;
    if (!result.found || !possible(result.state)) {
      return made;
    }
    made.found = true;
    made.route = result.state;
    made.how = result.how;
    const route_finish finished = route_finishes.entries[result.state];
    made.value_route = (result.how == semantics::satisfaction::move ||
                        result.how == semantics::satisfaction::copy) &&
                       finished.copies;
    made.quals = finished.quals;
    const semantics::object &reached = closure.states[result.state].reached;
    made.stable_address =
        (result.how == semantics::satisfaction::point
             ? reached.lifetimes[1]
             : reached.lifetimes[0]) == semantics::lifetime::stable;
    return made;
  }

  // The answers of every shape the closure serves, of each category and
  // qualifiers of the object a request is of, that it answers for a
  // lifetime.
  struct answer_list {
    closed_answers entries[shapes.count * shape_answer_count + 1] = {};
  };
  static constexpr answer_list make_answers() {
    answer_list made;
    for (std::size_t shape = 0; shape < shapes.count; ++shape) {
      for (unsigned taken = 0; taken < 7; ++taken) {
        for (std::uint8_t quals = 0; quals < 4; ++quals) {
          semantics::request goal = shapes.goals[shape];
          goal.quals[0] = quals;
          const auto category = static_cast<semantics::category>(taken);
          made.entries[shape * shape_answer_count + taken * 4 + quals] = {
              find_answer(goal, category, semantics::lifetime::stable),
              find_answer(goal, category, semantics::lifetime::request)};
        }
      }
    }
    return made;
  }
  static constexpr answer_list answers = make_answers();

  // Whether a step a request takes of what the binding does not know may
  // start from a state: a wrap of what it owns, a handle made of a raw
  // pointer, or a handle it owns converted.
  static constexpr bool starts_steps(const semantics::object &object) {
    const semantics::context context = Semantic::context();
    const bool wraps =
        (object.origin == semantics::origin::made ||
         (object.origin == semantics::origin::copied && object.instance)) &&
        semantics::can_move(object, context) &&
        object.depth < semantics::derivation_depth && !object.owns_pointee;
    const bool pointer = object.depth && context.kind(object.layers[0]).adopted;
    const bool handle = object.depth && semantics::owned(object.origin);
    const bool bound =
        object.depth >= 2 && context.kind(object.layers[1]).bounded;
    return wraps || pointer || handle || bound;
  }

  // The states a route of the binding reaches, of a value (Value) or not: of
  // an answer, or of a plan that takes steps of the request after it.
  struct routed_list {
    bool through[closure.count + 1] = {};
    bool value[closure.count + 1] = {};
  };
  static constexpr routed_list make_routed() {
    routed_list made;
    for (std::size_t index = 0; index < shapes.count * shape_answer_count;
         ++index) {
      for (const closed_answer &each :
           {answers.entries[index].stable, answers.entries[index].transient}) {
        if (each.found) {
          (each.value_route ? made.value : made.through)[each.route] = true;
        }
      }
    }
    for (std::size_t index = 0; index < closure.count; ++index) {
      if (possible(index) && starts_steps(closure.states[index].reached)) {
        made.through[index] = true;
      }
    }
    return made;
  }
  static constexpr routed_list routed = make_routed();

  // The pointers an answer gives what they point to of.
  struct pointed_list {
    bool entries[closure.count + 1] = {};
  };
  static constexpr pointed_list make_pointed() {
    pointed_list made;
    for (std::size_t index = 0; index < shapes.count * shape_answer_count;
         ++index) {
      for (const closed_answer &each :
           {answers.entries[index].stable, answers.entries[index].transient}) {
        if (each.found && each.how == semantics::satisfaction::point) {
          made.entries[each.route] = true;
        }
      }
    }
    return made;
  }
  static constexpr pointed_list pointed = make_pointed();
  static constexpr bool any_routed_value() {
    for (std::size_t index = 0; index < closure.count; ++index) {
      if (routed.value[index]) {
        return true;
      }
    }
    return false;
  }
  static constexpr bool any_value = any_routed_value();
};

template <typename Plan, const semantics::search &Sources, typename Identities,
          typename Stored, typename Leaf, typename Interface>
struct binding_sources {
  static constexpr bool abstract = !std::is_same_v<Leaf, Interface>;
  static_assert(Plan::template whole<abstract>());
  using semantic = closed_semantics<typename Plan::template shape<abstract>>;
  static constexpr const state_classes &classes = semantic::classes;
  static constexpr const auto &used = semantic::used;
  static constexpr const auto &masks = semantic::masks;
  static constexpr const semantics::search &closure = semantic::closure;
  static constexpr const auto &closed = semantic::closed;
  static constexpr const auto &route_finishes = semantic::route_finishes;
  template <std::size_t State, bool Value>
  static constexpr const semantics::plan &route_plan =
      semantic::template route_plan<State, Value>;
  template <std::size_t State, bool Value>
  static constexpr const program &route_program =
      semantic::template route_program<State, Value>;
  static constexpr std::size_t run_length(std::size_t state) {
    return semantic::run_length(state);
  }

  template <std::size_t State>
  using reaching_step =
      typename typed_step<Identities, search_step<Sources, State>>::step_type;

  template <typename U> struct tag {
    using type = U;
  };

  // The carrier a step into a state makes of the carrier From.
  template <std::size_t State, typename From>
  using reached_t =
      decltype(reaching_step<State>::template apply<Leaf, Interface>(
          std::declval<From>(), std::declval<store_view &>()));

  // The carrier of a class of states.
  template <std::size_t Class> struct class_carrier {
    static auto make() {
      if constexpr (Class == 0) {
        return tag<carrier<Stored, semantics::owned(
                                       Sources.states[0].reached.origin)>>{};
      } else {
        return tag<reached_t<
            Class,
            typename class_carrier<
                classes.state_class[Sources.states[Class].parent]>::type>>{};
      }
    }
    using type = typename decltype(make())::type;
  };

  template <std::size_t State>
  using table_t = operation_table<
      typename class_carrier<classes.state_class[State]>::type::type, Leaf,
      Interface, masks.state_mask[classes.state_class[State]]>;

  // A published state, renumbered, of the table of its type; the step into
  // it is checked to be an operation of the table it is taken from.
  template <std::size_t State> static constexpr semantics::state source() {
    if constexpr (State != 0) {
      constexpr semantics::state target = Sources.states[State];
      using source_carrier =
          typename class_carrier<classes.state_class[target.parent]>::type;
      static_assert(
          std::is_same_v<
              reached_t<State, source_carrier>,
              typename class_carrier<classes.state_class[State]>::type>,
          "a step into a state makes the type of its class");
      if constexpr (step_makes(target.via)) {
        using made = typename table_t<State>::maker;
        constexpr const semantics::object &reached =
            Sources.states[State].reached;
        static_assert(target.via == semantics::step::wrap
                          ? made::full.wrap_runs[run_length(State)].owned !=
                                nullptr
                      : target.via == semantics::step::observe
                          ? has(made::full.observing)
                          : has(reached.depth > 1 && reached.layers[1] ==
                                                         semantics::array_layer
                                    ? made::full.adopted_array
                                    : made::full.adopted),
                      "the type a binding makes a layer of is made so");
      } else {
        using parent = table_t<target.parent>;
        constexpr operation step =
            target.via == semantics::step::release &&
                    Sources.states[target.parent].reached.layers[0] ==
                        semantics::array_pointer_layer
                ? parent::maker::full.release_array
                : parent::maker::full
                      .steps[static_cast<std::size_t>(target.via)];
        static_assert((source_carrier::owned ? step.owned : step.borrowed) !=
                          nullptr,
                      "the table of a type has every step from it");
      }
    }
    // Every step a request takes of it that its type knows.
    using own = typename class_carrier<classes.state_class[State]>::type;
    constexpr std::uint32_t kinds =
        request_kinds(Sources.states[State].reached, Plan::context(abstract));
    constexpr const full_table &table = table_t<State>::maker::full;
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
  static constexpr bool known(const full_table &table, std::uint32_t kinds,
                              semantics::step kind) {
    if (!(kinds & (std::uint32_t{1} << static_cast<unsigned>(kind)))) {
      return true;
    }
    const operation &step = table.steps[static_cast<std::size_t>(kind)];
    return (Own::owned ? step.owned : step.borrowed) != nullptr;
  }

  template <std::size_t... I>
  static constexpr auto publish(std::index_sequence<I...>) {
    struct table {
      semantics::state states[sizeof...(I)];
      const type_table *tables[sizeof...(I)];
    };
    return table{{source<used.states[I]>()...},
                 {&table_t<used.states[I]>::value...}};
  }

  static constexpr auto published =
      publish(std::make_index_sequence<used.count>{});

  // The carrier of each state of the closure, and whether C++ makes it: a
  // published state is of the class of its type; what a step of a request
  // makes of a state is, when the types of the step make it.
  template <std::size_t State, bool Published = (State < used.count)>
  struct closed_carrier {
    static constexpr bool possible = true;
    // MSVC does not parse a subscript in a subscript of a template argument.
    static constexpr std::size_t state = used.states[State];
    using type = typename class_carrier<classes.state_class[state]>::type;
  };
  template <std::size_t State> struct closed_carrier<State, false> {
    using parent = closed_carrier<closure.states[State].parent>;
    using step_type =
        typename typed_step<Identities,
                            search_step<semantic::closure, State>>::step_type;
    template <bool Made, typename = void> struct reached_carrier {
      static constexpr bool possible = false;
      using type = void;
    };
    template <typename Unused> struct reached_carrier<true, Unused> {
      using result =
          step_result<step_type, Leaf, Interface, typename parent::type::type,
                      parent::type::owned>;
      static constexpr bool possible = result::possible;
      using type = typename result::type;
    };
    static constexpr bool possible =
        reached_carrier<parent::possible>::possible;
    using type = typename reached_carrier<parent::possible>::type;
  };

  using start_carrier = typename closed_carrier<0>::type;

  // The step into a state of the closure.
  template <std::size_t State>
  using closed_step =
      typename typed_step<Identities,
                          search_step<semantic::closure, State>>::step_type;

  // What the route of a state reaches: the last instruction of its program,
  // applied to what the route of the state it starts from reaches, so that
  // each state compiles one step, and the routes share what leads to them.
  // The instruction is the step into the state, or of the state before it
  // with the qualifiers of the state, when the semantics make it at once; or
  // the step the program takes in place of it.
  template <std::size_t State, bool Value, typename Store>
  static auto reach_state(start_carrier source, Store &store) {
    constexpr const program &made = route_program<State, Value>;
    if constexpr (made.count == 0) {
      return source;
    } else {
      constexpr const semantics::plan &plan = route_plan<State, Value>;
      constexpr instruction each = made.entries[made.count - 1];
      constexpr std::uint16_t before = plan.states[each.first];
      constexpr std::uint16_t first = plan.states[each.first + 1];
      constexpr semantics::object reached = plan.objects[each.last + 1];
      auto &&kept =
          store_of<reached.lifetimes[0] == semantics::lifetime::stable>(store);
      auto reached_before = reach_state<before, false, Store>(source, store);
      if constexpr (each.qualified) {
        using step_type =
            typename holding_step<closed_step<first>, reached.quals[1]>::type;
        return step_type::template apply<Leaf, Interface>(reached_before, kept);
      } else if constexpr (each.kind == plan.steps[each.first]) {
        return closed_step<first>::template apply<Leaf, Interface>(
            reached_before, kept);
      } else {
        return step<each.kind>::template apply<Leaf, Interface>(reached_before,
                                                                kept);
      }
    }
  }

  template <std::size_t State, bool Value, typename Store>
  static erased_carrier route(start_carrier source, Store &store) {
    return erase(reach_state<State, Value, Store>(source, store));
  }

  // Runs the route of a state, of a value when Value: the routes are
  // compiled in place, chosen among by the state.
  template <bool Value, typename Store, std::size_t... I>
  DINGO_ALWAYS_INLINE static erased_carrier
  run_route(std::size_t state, start_carrier source, Store &store,
            std::index_sequence<I...>) {
    erased_carrier reached;
    using fn = void (*)(start_carrier, Store &, erased_carrier &);
    static constexpr fn routes[] = {&run_state_route<I, Value, Store>...};
    routes[state](source, store, reached);
    return reached;
  }
  template <std::size_t State, bool Value, typename Store>
  static void run_state_route(start_carrier source, Store &store,
                              erased_carrier &reached) {
    if constexpr (closed_carrier<State>::possible &&
                  (Value ? answer_table::routed.value[State]
                         : answer_table::routed.through[State])) {
      reached = route<State, Value, Store>(source, store);
    }
  }
  template <bool Value, typename Store>
  DINGO_ALWAYS_INLINE static erased_carrier
  run_route(std::size_t state, start_carrier source, Store &store) {
    return run_route<Value>(state, source, store,
                            std::make_index_sequence<closure.count>{});
  }

  // Whether C++ makes each state of the closure.
  template <std::size_t... I>
  static constexpr auto make_possible(std::index_sequence<I...>) {
    struct list {
      bool entries[sizeof...(I)];
    };
    return list{{closed_carrier<I>::possible...}};
  }
  static constexpr auto possible_states =
      make_possible(std::make_index_sequence<closure.count>{});

  // The answers of the closure, of the states C++ makes, as bits.
  static constexpr std::size_t possible_words = (closure.count + 63) / 64;
  static constexpr std::uint64_t possible_word(std::size_t word) {
    std::uint64_t made = 0;
    for (std::size_t index = 0; index < 64 && word * 64 + index < closure.count;
         ++index) {
      if (possible_states.entries[word * 64 + index]) {
        made |= std::uint64_t{1} << index;
      }
    }
    return made;
  }
  template <std::size_t... I>
  static auto make_answer_table(std::index_sequence<I...>)
      -> closed_answer_table<semantic, possible_word(I)...>;
  using answer_table =
      decltype(make_answer_table(std::make_index_sequence<possible_words>{}));
  static constexpr const auto &shapes = answer_table::shapes;
  static constexpr const auto &answers = answer_table::answers;

  // Whether a request converts what a state of the closure reaches by an
  // operation of its type, after the route of the state: a handle to the
  // sharing one, or to one of an array of unknown bound, of a layer the
  // binding does not know. Wraps, observers and adopters are made of the
  // types the request spells.
  template <std::size_t State> static constexpr bool converts_unknown_layer() {
    if constexpr (!closed_carrier<State>::possible) {
      return false;
    } else {
      using own = typename closed_carrier<State>::type;
      return step_result<step<semantics::step::convert>, Leaf, Interface,
                         typename own::type, true>::possible ||
             step_result<step<semantics::step::unknown_bound>, Leaf, Interface,
                         typename own::type, own::owned>::possible;
    }
  }

  // The table of the type of each state the closure adds, of the steps a
  // request takes of it; none of a state C++ does not make.
  template <std::size_t State>
  static constexpr const type_table *closed_table() {
    if constexpr (State < used.count) {
      return published.tables[State];
    } else if constexpr (!closed_carrier<State>::possible ||
                         !(answer_table::pointed.entries[State] ||
                           converts_unknown_layer<State>())) {
      return nullptr;
    } else {
      using own = typename closed_carrier<State>::type;
      constexpr std::uint32_t taken = step_mask(semantics::step::copy) |
                                      step_mask(semantics::step::decay) |
                                      step_mask(semantics::step::hand_over) |
                                      step_mask(semantics::step::convert) |
                                      step_mask(semantics::step::unknown_bound);
      constexpr std::uint32_t mask =
          (request_kinds(closure.states[State].reached,
                         Plan::context(abstract)) &
           taken) |
          step_mask(semantics::step::convert) |
          step_mask(semantics::step::unknown_bound);
      return &operation_table<typename own::type, Leaf, Interface, mask>::value;
    }
  }
  template <std::size_t... I>
  static constexpr auto make_tables(std::index_sequence<I...>) {
    struct list {
      const type_table *entries[sizeof...(I)];
    };
    return list{{closed_table<I>()...}};
  }
  static constexpr auto closed_tables =
      make_tables(std::make_index_sequence<closure.count>{});

  // The key of the type of a shape: of a state, or what it points to.
  template <std::size_t Shape> static constexpr const void *shape_type_key() {
    using type = typename closed_carrier<shapes.states[Shape]>::type::type;
    if constexpr (shapes.pointee[Shape]) {
      return &shape_key<std::remove_cv_t<std::remove_pointer_t<type>>>;
    } else {
      return &shape_key<std::remove_cv_t<type>>;
    }
  }
  template <std::size_t... I>
  static constexpr auto make_groups(std::index_sequence<I...>) {
    struct list {
      closed_group entries[sizeof...(I) + 1];
    };
    return list{{{shape_type_key<I>(),
                  static_cast<std::uint16_t>(I * shape_answer_count)}...,
                 {}}};
  }
  static constexpr auto groups =
      make_groups(std::make_index_sequence<shapes.count>{});
};

// The types a request of Request spells, from its shape at depth 0 to its
// element, of which Element is the innermost.
template <typename Request, typename Element> struct request_types {
  using shape = typename requested_category<Request>::shape;
  static constexpr std::size_t depth =
      type_list_size_v<typename stack_t<shape, Element>::layers>;

  template <std::size_t K, typename T = shape> struct nested {
    using type = typename nested<K - 1, held_element_t<T, Element>>::type;
  };
  template <typename T> struct nested<0, T> {
    using type = T;
  };
};

// The tables of the types a request spells, which make the layers it makes.
template <typename Request, typename Element> struct request_tables {
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

  template <typename T> static constexpr std::uint32_t type_mask() {
    return (makes<T>() ? makers_mask : 0) |
           (qualifies<T>() ? qualified_mask : 0);
  }

  template <std::size_t K>
  using table_t =
      operation_table<typename types::template nested<K>::type, Element,
                      Element,
                      type_mask<typename types::template nested<K>::type>()>;

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
      using nested_type = typename types::template nested<K + 1>::type;
      return (!kind.wraps_value ||
              !layer_types<nested_type, Element,
                           Element>::template movable<nested_type>() ||
              has(maker::full.wrap_runs[1])) &&
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
      const type_table *value[sizeof...(K)];
    };
    return table{{&table_t<K>::value...}};
  }

  static constexpr auto value =
      make(std::make_index_sequence<types::depth + 1>{});
};
} // namespace dingo::detail::execution
