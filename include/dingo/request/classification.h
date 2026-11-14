//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/type_list.h>
#include <dingo/semantics/model.h>
#include <dingo/type/type_traits.h>

#include <cstddef>
#include <type_traits>

// The classification of C++ types: what the semantics (dingo/semantics/
// model.h) know of a type. A layer of a type is its properties, which its
// type_traits give: the semantics the specialization derives from, and what
// C++ says of the wrapper of an element that can be copied, and of one that
// can only be moved. The layers of C++ itself, a pointer and an array, are
// those of the semantics. No wrapper is named here.

namespace dingo::detail {

// The elements a wrapper is asked about.
struct copyable_probe {};

struct movable_probe {
  movable_probe() = default;
  movable_probe(movable_probe &&) = default;
  movable_probe &operator=(movable_probe &&) = default;
};

struct layer_probe {};

// The layers of a stack: a pointer, an array of unknown bound, a bounded
// array, or a wrapper.
struct pointer_layer_tag {};
struct array_layer_tag {};
struct bounded_array_layer_tag {};
template <typename Wrapper> struct wrapper_layer_tag {
  using type = Wrapper;
};

// The bound of the array an owning handle holds, or 0.
template <typename Wrapper, typename = void>
inline constexpr std::size_t handle_extent_v = 0;

template <typename Wrapper>
inline constexpr std::size_t handle_extent_v<
    Wrapper, std::void_t<decltype(type_traits<Wrapper>::extent)>> =
    type_traits<Wrapper>::extent;

template <typename Wrapper, typename = void> struct rebinds_element {
  static constexpr bool value = false;
};

template <typename Wrapper>
struct rebinds_element<Wrapper, std::void_t<typename type_traits<
                                    Wrapper>::template rebind_t<layer_probe>>> {
  static constexpr bool value = true;
};

// What identifies the layer of a wrapper: the same wrapper of a probe, or,
// of a wrapper that cannot rebind its element, the wrapper itself.
template <typename Wrapper, bool = rebinds_element<Wrapper>::value>
struct layer_identity {
  using type = Wrapper;
};

template <typename Wrapper> struct layer_identity<Wrapper, true> {
  using type = typename type_traits<Wrapper>::template rebind_t<layer_probe>;
};

template <typename Wrapper>
using layer_identity_t = typename layer_identity<Wrapper>::type;

template <typename Semantics, typename Of>
inline constexpr bool is_semantics_v = std::is_same_v<Semantics, Of>;

// The properties of the layer of a wrapper.
template <typename Wrapper> constexpr semantics::layer_kind wrapper_kind() {
  using traits = type_traits<Wrapper>;
  using of = typename traits::semantics;
  semantics::layer_kind kind;
  if constexpr (is_semantics_v<of, ::dingo::semantics::observer>) {
    kind.owns = semantics::owner::none;
    kind.shares_element = true;
  } else if constexpr (is_semantics_v<of, ::dingo::semantics::sole_owner>) {
    kind.owns = semantics::owner::sole;
    kind.shares_element = true;
    kind.made_from_value = true;
  } else if constexpr (is_semantics_v<of, ::dingo::semantics::shared_owner>) {
    kind.owns = semantics::owner::shared;
    kind.shares_element = true;
    kind.made_from_value = true;
  } else if constexpr (is_semantics_v<of, ::dingo::semantics::optional_value>) {
    kind.owns = semantics::owner::sole;
    kind.made_from_value = true;
    kind.has_empty_value = true;
  } else {
    static_assert(is_semantics_v<of, ::dingo::semantics::alternative_value>,
                  "a wrapper derives from one of the semantics of type_traits");
    kind.owns = semantics::owner::sole;
    kind.made_from_value = true;
    kind.holds_element_only = true;
  }
  kind.may_be_empty = true;
  if constexpr (rebinds_element<Wrapper>::value) {
    using copyable = typename traits::template rebind_t<copyable_probe>;
    using movable = typename traits::template rebind_t<movable_probe>;
    kind.copyable = std::is_copy_constructible_v<copyable>;
    kind.relocatable = std::is_move_constructible_v<movable>;
    kind.adopts = traits::is_owning_handle &&
                  std::is_constructible_v<copyable, copyable_probe *>;
  } else {
    kind.copyable = std::is_copy_constructible_v<Wrapper>;
    kind.relocatable = std::is_move_constructible_v<Wrapper>;
  }
  return kind;
}

template <typename Wrapper> constexpr semantics::layer_kind possible_kind() {
  constexpr semantics::layer_kind kind = wrapper_kind<Wrapper>();
  static_assert(semantics::possible(kind),
                "the type_traits of a wrapper describe a layer no type can "
                "be: see semantics::possible");
  return kind;
}

template <typename Wrapper>
inline constexpr semantics::layer_kind wrapper_kind_v =
    possible_kind<Wrapper>();

// The layer a sole owner hands its element over to, or none.
template <typename Wrapper, typename = void> struct shared_layer {
  using type = void;
};

template <typename Wrapper>
struct shared_layer<
    Wrapper,
    std::void_t<typename type_traits<Wrapper>::template share_t<layer_probe>>> {
  using type = layer_identity_t<
      typename type_traits<Wrapper>::template share_t<layer_probe>>;
};

// What a stack ends in: the leaf, the abstract interface of the leaf, or
// another type, which the semantics do not reach.
enum class stack_element { leaf, interface, other };

// The layers around the element of a type, outermost first, as tags.
template <typename Layers, stack_element Element> struct type_stack {
  using layers = Layers;
  static constexpr stack_element element = Element;
};

template <typename Tag, typename Stack> struct stacked;

template <typename Tag, typename... Tags, stack_element Element>
struct stacked<Tag, type_stack<type_list<Tags...>, Element>> {
  using type = type_stack<type_list<Tag, Tags...>, Element>;
};

template <typename Tag, typename Stack>
using stacked_t = typename stacked<Tag, Stack>::type;

template <typename T, typename Leaf> struct stack_of;

template <typename T, typename Leaf>
using stack_of_t = typename stack_of<std::remove_cv_t<T>, Leaf>::type;

// The stack of the first alternative of an alternative type that ends in the
// leaf or its interface, or of none.
template <typename Leaf, typename... Alternatives> struct alternative_stack {
  using type = type_stack<type_list<>, stack_element::other>;
};

template <typename Leaf, typename Alternative, typename... Alternatives>
struct alternative_stack<Leaf, Alternative, Alternatives...> {
  using candidate = stack_of_t<Alternative, Leaf>;
  using type = std::conditional_t<
      candidate::element != stack_element::other, candidate,
      typename alternative_stack<Leaf, Alternatives...>::type>;
};

template <typename Leaf, typename List> struct alternatives_stack;

template <typename Leaf, typename... Alternatives>
struct alternatives_stack<Leaf, type_list<Alternatives...>> {
  using type = typename alternative_stack<Leaf, Alternatives...>::type;
};

template <typename T, typename Leaf> struct stack_of {
  static auto make() {
    if constexpr (std::is_same_v<T, Leaf>) {
      return type_stack<type_list<>, stack_element::leaf>{};
    } else if constexpr (std::is_pointer_v<T>) {
      return stacked_t<pointer_layer_tag,
                       stack_of_t<std::remove_pointer_t<T>, Leaf>>{};
    } else if constexpr (std::is_array_v<T>) {
      return stacked_t<
          std::conditional_t<std::extent_v<T> == 0, array_layer_tag,
                             bounded_array_layer_tag>,
          stack_of_t<std::remove_extent_t<T>, Leaf>>{};
    } else if constexpr (type_traits<T>::enabled) {
      using element = typename type_traits<T>::value_type;
      // An owning handle whose element cannot be borrowed as an object holds
      // an array of it, of the bound its type traits give, if any.
      if constexpr (is_array_like_type_v<T> && !std::is_array_v<element>) {
        return stacked_t<wrapper_layer_tag<T>,
                         stacked_t<std::conditional_t<handle_extent_v<T> == 0,
                                                      array_layer_tag,
                                                      bounded_array_layer_tag>,
                                   stack_of_t<element, Leaf>>>{};
      } else {
        return stacked_t<wrapper_layer_tag<T>, stack_of_t<element, Leaf>>{};
      }
    } else if constexpr (is_alternative_type_v<T>) {
      return stacked_t<wrapper_layer_tag<T>,
                       typename alternatives_stack<
                           Leaf, alternative_type_alternatives_t<T>>::type>{};
    } else if constexpr (std::is_base_of_v<T, Leaf>) {
      return type_stack<type_list<>, stack_element::interface>{};
    } else {
      return type_stack<type_list<>, stack_element::other>{};
    }
  }
  using type = decltype(make());
};

// The distinct wrappers of tags, by identity.
template <typename Identities, typename... Tags> struct wrapper_identities {
  using type = Identities;
};

template <typename... Identities, typename Tag, typename... Tags>
struct wrapper_identities<type_list<Identities...>, Tag, Tags...> {
  using type =
      typename wrapper_identities<type_list<Identities...>, Tags...>::type;
};

template <typename... Identities, typename Wrapper, typename... Tags>
struct wrapper_identities<type_list<Identities...>, wrapper_layer_tag<Wrapper>,
                          Tags...> {
  using identity = layer_identity_t<Wrapper>;
  using type = typename wrapper_identities<
      std::conditional_t<(std::is_same_v<identity, Identities> || ...),
                         type_list<Identities...>,
                         type_list<Identities..., identity>>,
      Tags...>::type;
};

template <typename Identity, typename... Identities>
constexpr semantics::layer index_of(type_list<Identities...>) {
  constexpr bool matches[] = {std::is_same_v<Identity, Identities>..., true};
  std::size_t index = 0;
  while (!matches[index]) {
    ++index;
  }
  return index == sizeof...(Identities)
             ? semantics::no_layer
             : static_cast<semantics::layer>(4 + index);
}

// The universe of the layers of stacks: those of C++ itself, then each
// wrapper of the stacks once, with the layers an owned value of it converts
// to among them.
template <typename Tags> struct layer_universe;

template <typename... Tags> struct layer_universe<type_list<Tags...>> {
  using identities = typename wrapper_identities<type_list<>, Tags...>::type;

  template <typename Tag> static constexpr semantics::layer index() {
    if constexpr (std::is_same_v<Tag, pointer_layer_tag>) {
      return semantics::pointer_layer;
    } else if constexpr (std::is_same_v<Tag, array_layer_tag>) {
      return semantics::array_layer;
    } else if constexpr (std::is_same_v<Tag, bounded_array_layer_tag>) {
      return semantics::bounded_array_layer;
    } else {
      return index_of<layer_identity_t<typename Tag::type>>(identities{});
    }
  }

  // Adds the layer of a wrapper, with the layer its owned value converts to
  // if it is in the universe.
  template <typename Identity>
  static constexpr void add(semantics::universe &made) {
    semantics::layer_kind kind = wrapper_kind_v<Identity>;
    using target = typename shared_layer<Identity>::type;
    if constexpr (!std::is_void_v<target>) {
      const semantics::layer to = index_of<target>(identities{});
      if (to != semantics::no_layer) {
        kind.converts |= std::uint32_t{1} << to;
      }
    }
    made.add(kind);
  }

  template <typename... Identities>
  static constexpr semantics::universe make(type_list<Identities...>) {
    semantics::universe made;
    (add<Identities>(made), ...);
    return made;
  }

  static constexpr semantics::universe value = make(identities{});
};

} // namespace dingo::detail
