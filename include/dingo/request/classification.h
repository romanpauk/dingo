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
#include <dingo/type/collection_traits.h>
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
template <typename Wrapper> struct wrapper_layer_tag;
template <typename Wrapper, typename Alternative> struct alternative_layer_tag;

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

// What identifies the layer of an alternative type, which cannot rebind its
// element: the type, and the alternative that holds the element.
template <typename Wrapper, typename Alternative> struct alternative_identity {
  using type = Wrapper;
  using alternative = Alternative;
};

template <typename Identity>
inline constexpr bool is_alternative_layer_v = false;

template <typename Wrapper, typename Alternative>
inline constexpr bool
    is_alternative_layer_v<alternative_identity<Wrapper, Alternative>> = true;

template <typename Wrapper> struct wrapper_layer_tag {
  using type = Wrapper;
  using identity = layer_identity_t<Wrapper>;
};

template <typename Wrapper, typename Alternative> struct alternative_layer_tag {
  using type = Wrapper;
  using identity = alternative_identity<Wrapper, Alternative>;
};

template <typename Semantics, typename Of>
inline constexpr bool is_semantics_v = std::is_same_v<Semantics, Of>;

// A type, named without being formed until type is asked for: the wrapper
// Traits rebinds to an element.
template <typename T> struct type_identity {
  using type = T;
};
template <typename Traits, typename Element> struct rebound {
  using type = typename Traits::template rebind_t<Element>;
};

// The properties of the layer of a wrapper.
template <typename Wrapper> constexpr semantics::layer_kind wrapper_kind() {
  using traits = type_traits<Wrapper>;
  using wrapper_semantics = typename traits::semantics;
  semantics::layer_kind kind;
  if constexpr (is_semantics_v<wrapper_semantics,
                               ::dingo::semantics::observer>) {
    kind.owns = semantics::owner::none;
    kind.shares_element = true;
  } else if constexpr (is_semantics_v<wrapper_semantics,
                                      ::dingo::semantics::sole_owner>) {
    kind.owns = semantics::owner::sole;
    kind.shares_element = true;
    kind.wraps_value = true;
  } else if constexpr (is_semantics_v<wrapper_semantics,
                                      ::dingo::semantics::shared_owner>) {
    kind.owns = semantics::owner::shared;
    kind.shares_element = true;
    kind.wraps_value = true;
  } else {
    static_assert(
        is_semantics_v<wrapper_semantics, ::dingo::semantics::optional_value>,
        "a wrapper derives from one of the semantics of "
        "type_traits; an alternative type is a layer by the "
        "alternative that holds the element");
    kind.owns = semantics::owner::sole;
    kind.wraps_value = true;
    kind.has_empty_value = true;
  }
  kind.may_be_empty = true;
  if constexpr (rebinds_element<Wrapper>::value) {
    using copyable = typename traits::template rebind_t<copyable_probe>;
    // A pointer-like wrapper copies and moves as it does whatever its
    // element is: the wrapper of a copyable element is asked about both.
    using movable =
        typename std::conditional_t<traits::is_pointer_like,
                                    type_identity<copyable>,
                                    rebound<traits, movable_probe>>::type;
    kind.copyable = std::is_copy_constructible_v<copyable>;
    kind.relocatable = std::is_move_constructible_v<movable>;
    kind.adopts = traits::is_owning_handle &&
                  std::is_constructible_v<copyable, copyable_probe *>;
    // A handle takes over a pointer to an array of a known bound when its
    // type traits say it deletes one as an array.
    kind.adopts_bounded = kind.adopts && adopts_bounded_array_v<Wrapper>;
    // A pointer-like wrapper of the leaf is rebound to one of its interface.
    kind.rebinds = traits::is_pointer_like;
    // An observer is made of a raw pointer to its element.
    kind.observes = kind.owns == semantics::owner::none &&
                    std::is_constructible_v<copyable, copyable_probe *>;
    // A handle converts to one of a more qualified element as a pointer to
    // the element does, when its converting constructor does: of a copy of
    // it, and of one moved out of.
    if constexpr (traits::is_pointer_like) {
      using const_copyable =
          typename traits::template rebind_t<const copyable_probe>;
      using volatile_copyable =
          typename traits::template rebind_t<volatile copyable_probe>;
      kind.qualifies =
          std::is_constructible_v<const_copyable, copyable &&> &&
          std::is_constructible_v<volatile_copyable, copyable &&> &&
          (!kind.copyable ||
           (std::is_constructible_v<const_copyable, const copyable &> &&
            std::is_constructible_v<volatile_copyable, const copyable &>));
    }
  } else {
    kind.copyable = std::is_copy_constructible_v<Wrapper>;
    kind.relocatable = std::is_move_constructible_v<Wrapper>;
  }
  return kind;
}

// The properties of the layer of an alternative type: it can be copied, or
// moved, when every alternative but the one that holds the element can.
template <typename Alternative, typename... Alternatives>
constexpr semantics::layer_kind alternative_kind(type_list<Alternatives...>) {
  semantics::layer_kind kind;
  kind.wraps_value = true;
  kind.holds_element_only = true;
  kind.may_be_empty = true;
  kind.copyable = ((std::is_same_v<Alternatives, Alternative> ||
                    std::is_copy_constructible_v<Alternatives>) &&
                   ...);
  kind.relocatable = ((std::is_same_v<Alternatives, Alternative> ||
                       std::is_move_constructible_v<Alternatives>) &&
                      ...);
  return kind;
}

template <typename Identity> struct identity_kind {
  static constexpr semantics::layer_kind value = wrapper_kind<Identity>();
};

template <typename Wrapper, typename Alternative>
struct identity_kind<alternative_identity<Wrapper, Alternative>> {
  static constexpr semantics::layer_kind value =
      alternative_kind<Alternative>(alternative_type_alternatives_t<Wrapper>{});
};

template <typename Wrapper> constexpr semantics::layer_kind possible_kind() {
  constexpr semantics::layer_kind kind = identity_kind<Wrapper>::value;
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

// The layer a handle of an array of a known bound converts to, holding the
// same array as one of unknown bound, or none.
template <typename Wrapper, typename = void> struct unknown_bound_layer {
  using type = void;
};

template <typename Wrapper>
struct unknown_bound_layer<
    Wrapper, std::void_t<typename type_traits<
                 Wrapper>::template unknown_bound_t<layer_probe>>> {
  using type = layer_identity_t<
      typename type_traits<Wrapper>::template unknown_bound_t<layer_probe>>;
};

// Whether a handle of an array of a known bound converts to the handle its
// type traits name: moved, and copied too when it can be copied.
template <typename Wrapper> constexpr bool converts_unknown_bound() {
  if constexpr (std::is_void_v<typename unknown_bound_layer<Wrapper>::type>) {
    return false;
  } else {
    using traits = type_traits<Wrapper>;
    using copyable = typename traits::template rebind_t<copyable_probe>;
    using movable = typename traits::template rebind_t<movable_probe>;
    return std::is_constructible_v<
               typename traits::template unknown_bound_t<movable_probe>,
               movable &&> &&
           (!std::is_copy_constructible_v<copyable> ||
            std::is_constructible_v<
                typename traits::template unknown_bound_t<copyable_probe>,
                const copyable &>);
  }
}

// What a stack ends in: the leaf, the abstract interface of the leaf, or
// another type, which the semantics do not reach.
enum class stack_element { leaf, abstract, other };

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

template <typename T, typename Leaf> struct stack_type;

template <typename T, typename Leaf>
using stack_t = typename stack_type<std::remove_cv_t<T>, Leaf>::type;

// The stack of the first alternative of an alternative type that ends in the
// leaf or its interface, or of none.
template <typename Leaf, typename... Alternatives> struct alternative_stack {
  using type = type_stack<type_list<>, stack_element::other>;
  using alternative = void;
};

template <typename Leaf, typename Alternative, typename... Alternatives>
struct alternative_stack<Leaf, Alternative, Alternatives...> {
  using candidate = stack_t<Alternative, Leaf>;
  using next = alternative_stack<Leaf, Alternatives...>;
  static constexpr bool holds = candidate::element != stack_element::other;
  using type = std::conditional_t<holds, candidate, typename next::type>;
  using alternative =
      std::conditional_t<holds, Alternative, typename next::alternative>;
};

template <typename Leaf, typename List> struct alternatives_stack;

template <typename Leaf, typename... Alternatives>
struct alternatives_stack<Leaf, type_list<Alternatives...>>
    : alternative_stack<Leaf, Alternatives...> {};

template <typename T, typename Leaf> struct stack_type {
  static auto make() {
    if constexpr (std::is_same_v<T, Leaf>) {
      return type_stack<type_list<>, stack_element::leaf>{};
    } else if constexpr (std::is_pointer_v<T>) {
      return stacked_t<pointer_layer_tag,
                       stack_t<std::remove_pointer_t<T>, Leaf>>{};
    } else if constexpr (std::is_array_v<T>) {
      return stacked_t<
          std::conditional_t<std::extent_v<T> == 0, array_layer_tag,
                             bounded_array_layer_tag>,
          stack_t<std::remove_extent_t<T>, Leaf>>{};
    } else if constexpr (type_traits<T>::enabled) {
      using element = typename type_traits<T>::value_type;
      // An owning handle whose element cannot be borrowed as an object holds
      // an array of it, of the bound its type traits give, if any.
      if constexpr (is_array_like_type_v<T>) {
        return stacked_t<wrapper_layer_tag<T>,
                         stacked_t<std::conditional_t<handle_extent_v<T> == 0,
                                                      array_layer_tag,
                                                      bounded_array_layer_tag>,
                                   stack_t<element, Leaf>>>{};
      } else {
        return stacked_t<wrapper_layer_tag<T>, stack_t<element, Leaf>>{};
      }
    } else if constexpr (is_alternative_type_v<T>) {
      using held = alternatives_stack<Leaf, alternative_type_alternatives_t<T>>;
      return stacked_t<alternative_layer_tag<T, typename held::alternative>,
                       typename held::type>{};
    } else if constexpr (std::is_base_of_v<T, Leaf>) {
      return type_stack<type_list<>, stack_element::abstract>{};
    } else {
      return type_stack<type_list<>, stack_element::other>{};
    }
  }
  using type = decltype(make());
};

// Whether a tag is a layer of C++ itself.
template <typename Tag>
inline constexpr bool is_language_tag_v =
    std::is_same_v<Tag, pointer_layer_tag> ||
    std::is_same_v<Tag, array_layer_tag> ||
    std::is_same_v<Tag, bounded_array_layer_tag>;

// The distinct identities of the tags of wrappers.
template <typename Identities, typename... Tags> struct wrapper_identities {
  using type = Identities;
};

template <typename... Identities, typename Tag, typename... Tags>
struct wrapper_identities<type_list<Identities...>, Tag, Tags...> {
  template <typename Of> static auto next() {
    if constexpr (is_language_tag_v<Of>) {
      return type_list<Identities...>{};
    } else if constexpr ((std::is_same_v<typename Of::identity, Identities> ||
                          ...)) {
      return type_list<Identities...>{};
    } else {
      return type_list<Identities..., typename Of::identity>{};
    }
  }
  using type =
      typename wrapper_identities<decltype(next<Tag>()), Tags...>::type;
};

template <typename Identity, typename... Identities>
constexpr semantics::layer layer_index(type_list<Identities...>) {
  constexpr bool matches[] = {std::is_same_v<Identity, Identities>..., true};
  std::size_t index = 0;
  while (!matches[index]) {
    ++index;
  }
  return index == sizeof...(Identities)
             ? semantics::no_layer
             : static_cast<semantics::layer>(4 + index);
}

// The properties of the layer of a tag.
template <typename Tag> constexpr semantics::layer_kind tag_kind() {
  constexpr semantics::universe language;
  if constexpr (std::is_same_v<Tag, pointer_layer_tag>) {
    return language.kinds[semantics::pointer_layer];
  } else if constexpr (std::is_same_v<Tag, array_layer_tag>) {
    return language.kinds[semantics::array_layer];
  } else if constexpr (std::is_same_v<Tag, bounded_array_layer_tag>) {
    return language.kinds[semantics::bounded_array_layer];
  } else {
    return wrapper_kind_v<typename Tag::identity>;
  }
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
      return layer_index<typename Tag::identity>(identities{});
    }
  }

  // Adds the layer of a wrapper, with the layer its owned value converts to,
  // and the layer it converts to of an array of unknown bound, if they are in
  // the universe.
  template <typename Identity>
  static constexpr void add(semantics::universe &made) {
    semantics::layer_kind kind = wrapper_kind_v<Identity>;
    using target = typename shared_layer<Identity>::type;
    if constexpr (!std::is_void_v<target>) {
      constexpr semantics::layer shared_index =
          layer_index<target>(identities{});
      if constexpr (shared_index != semantics::no_layer) {
        kind.converts |= std::uint32_t{1} << shared_index;
      }
    }
    if constexpr (converts_unknown_bound<Identity>()) {
      constexpr semantics::layer unknown_bound_index =
          layer_index<typename unknown_bound_layer<Identity>::type>(
              identities{});
      if constexpr (unknown_bound_index != semantics::no_layer) {
        kind.converts_unknown_bound |= std::uint32_t{1} << unknown_bound_index;
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

// What the leaf can do, as C++ says of it. A leaf that cannot be constructed
// of nothing is made by the factory its registration names.
// A leaf is copied as the library copies it: a collection of what cannot be
// copied cannot be either.
template <typename Leaf> constexpr semantics::leaf_traits leaf_traits_v() {
  return {is_copy_constructible_v<Leaf>, std::is_move_constructible_v<Leaf>,
          std::is_default_constructible_v<Leaf>,
          !std::is_default_constructible_v<Leaf>,
          std::is_constructible_v<Leaf, const volatile Leaf &>};
}

// The qualifiers of a type, as the semantics know them.
template <typename T>
inline constexpr std::uint8_t quals_v = static_cast<std::uint8_t>(
    (std::is_const_v<T> ? semantics::const_qual : 0) |
    (std::is_volatile_v<T> ? semantics::volatile_qual : 0));

// The qualifiers of each level of the stack of T toward Leaf, as its type
// declares them, outermost first: of T itself, then of each element.
template <typename T, typename Leaf> struct stack_quals {
  static constexpr void fill(std::uint8_t *out) {
    using type = std::remove_cv_t<T>;
    out[0] = quals_v<T>;
    if constexpr (std::is_same_v<type, Leaf>) {
    } else if constexpr (std::is_pointer_v<type>) {
      stack_quals<std::remove_pointer_t<type>, Leaf>::fill(out + 1);
    } else if constexpr (std::is_array_v<type>) {
      stack_quals<std::remove_extent_t<type>, Leaf>::fill(out + 1);
    } else if constexpr (type_traits<type>::enabled) {
      using element = typename type_traits<type>::value_type;
      if constexpr (is_array_like_type_v<type>) {
        // The array a handle holds is of the qualifiers of its element.
        out[1] = quals_v<element>;
        stack_quals<element, Leaf>::fill(out + 2);
      } else {
        stack_quals<element, Leaf>::fill(out + 1);
      }
    } else if constexpr (is_alternative_type_v<type>) {
      using held = typename alternatives_stack<
          Leaf, alternative_type_alternatives_t<type>>::alternative;
      if constexpr (!std::is_void_v<held>) {
        stack_quals<held, Leaf>::fill(out + 1);
      }
    }
  }
};

// How a request takes the shape it asks for: its outer reference or pointer,
// and whether that is const. A pointer is always the category of a request,
// as no request shape begins with one; a registration that binds a pointer
// type itself is told apart by the binding a request selects. A request the
// semantics do not model, an rvalue reference to const, is not modeled.
template <typename Request> struct requested_category {
  static auto make() {
    using referred = std::remove_reference_t<Request>;
    using pointee = std::remove_pointer_t<std::remove_cv_t<Request>>;
    if constexpr (std::is_lvalue_reference_v<Request>) {
      return request_category<std::is_const_v<referred>
                                  ? semantics::category::const_lvalue_reference
                                  : semantics::category::lvalue_reference,
                              std::remove_const_t<referred>>{};
    } else if constexpr (std::is_rvalue_reference_v<Request>) {
      return request_category<semantics::category::rvalue_reference, referred,
                              !std::is_const_v<referred>>{};
    } else if constexpr (std::is_pointer_v<std::remove_cv_t<Request>>) {
      return request_category<std::is_const_v<pointee>
                                  ? semantics::category::const_pointer
                                  : semantics::category::pointer,
                              std::remove_const_t<pointee>>{};
    } else {
      return request_category<std::is_const_v<Request>
                                  ? semantics::category::const_value
                                  : semantics::category::value,
                              std::remove_const_t<Request>>{};
    }
  }

  template <semantics::category Category, typename Shape, bool Modeled = true>
  struct request_category {
    static constexpr semantics::category value = Category;
    using shape = Shape;
    static constexpr bool modeled = Modeled;
  };

  using type = decltype(make());
  static constexpr semantics::category value = type::value;
  using shape = typename type::shape;
  static constexpr bool modeled = type::modeled;
};
} // namespace dingo::detail
