//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The wrapper semantics a C++ type has, as the conversion rules of the spec say
// it for C++ types. Raw pointers, references, and arrays are language
// wrappers, of the pointer, reference, and array kinds of the library. Wrappers
// are classified by wrapper_traits, which the standard wrappers specialize here
// and user wrappers through the public extension point; no wrapper gets other
// treatment. A wrapper semantics is a tag whose wrapper_semantics declare its
// properties and forms, and the library's kinds are declared as a user's are.
//
// A specialization names the wrapper semantics, the wrapper family, and the
// argument types, and provides `rebind`, the same wrapper of other argument
// types, `element`, which reaches the element of a wrapper object, and `make`,
// which makes a wrapper object from the arguments of its element. A handle that
// owns its element also adopts an element it is given the address of, and a
// sole-owner handle hands its element to the sharing handle of its family.

#include <dingo/base/mechanics.h>
#include <dingo/constructor/constructor.h>
#include <dingo/conversion/collection.h>
#include <dingo/conversion/operations.gen.h>
#include <dingo/conversion/wrapper_semantics.gen.h>
#include <dingo/diagnostics/compile_diagnostics.gen.h>
#include <dingo/primitives/type_tree.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace dingo {

// The public extension point that makes a wrapper type a wrapper of the
// conversion rules. A specialization declares
//
//   kind       the wrapper semantics of the wrapper, a tag whose
//   wrapper_semantics
//              declare it, such as ::dingo::semantics::sole_owner,
//              ::dingo::semantics::shared_owner,
//              ::dingo::semantics::optional_value,
//              ::dingo::semantics::variant_value, or a kind of the user
//   family     a type naming the family of the wrapper; a route converts a
//              wrapper only to another wrapper of its family, such as a
//              sole-owner handle to a sharing handle
//   arguments  the element types, as a type_list
//   rebind     the wrapper of other element types
//   element    the element of a wrapper object; for a variadic kind,
//              element<Index>, the alternative `Index` of it
//   index      for a variadic kind: the alternative a wrapper object holds,
//              which a route through another does not reach
//   has_element  for a nullable kind that holds its element in place: whether
//              a wrapper object holds one; a request of an address through
//              one that holds none is null, and any other is refused
//   make       a wrapper object made from the arguments of its element
//   adopt      for a kind of the adopt form: a wrapper object that takes the
//              ownership
//              of the element at an address allocated by new; a sharing
//              wrapper that a scope publishes before its element exists also
//              adopts an address with the deleter that frees it
//   share      for a kind of the share form, when its family has a wrapper of
//              the kind the form leads to: that wrapper object, which takes
//              the element of a consumed one
//
// A wrapper converts to the wrapper of a base of its element, the route to a
// declared interface, by its converting constructor. The standard wrappers
// specialize it below, as a user wrapper does.
template <typename Wrapper, typename = void> struct wrapper_traits {};

template <typename... Types> using type_list = core::type_list<Types...>;

// The family of the standard wrappers.
struct standard_family {};

template <typename Type> struct wrapper_traits<std::unique_ptr<Type>> {
  using semantics = ::dingo::semantics::sole_owner;
  using family = standard_family;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = std::unique_ptr<Element>;

  static Type &element(const std::unique_ptr<Type> &value) { return *value; }

  template <typename... Args>
  static std::unique_ptr<Type> make(Args &&...args) {
    return std::make_unique<Type>(std::forward<Args>(args)...);
  }

  static std::unique_ptr<Type> adopt(Type *element) {
    return std::unique_ptr<Type>(element);
  }

  static std::shared_ptr<Type> share(std::unique_ptr<Type> &&value) {
    return std::shared_ptr<Type>(std::move(value));
  }
};

// A handle of an array of runtime size: its element is the array.
template <typename Type> struct wrapper_traits<std::unique_ptr<Type[]>> {
  using semantics = ::dingo::semantics::sole_owner;
  using family = standard_family;
  using arguments = type_list<Type[]>;
  template <typename Element> using rebind = std::unique_ptr<Element>;
  using array_type = Type[];

  static array_type &element(const std::unique_ptr<Type[]> &value) {
    return *reinterpret_cast<array_type *>(value.get());
  }
};

template <typename Type> struct wrapper_traits<std::shared_ptr<Type[]>> {
  using semantics = ::dingo::semantics::shared_owner;
  using family = standard_family;
  using arguments = type_list<Type[]>;
  template <typename Element> using rebind = std::shared_ptr<Element>;
  using array_type = Type[];

  static array_type &element(const std::shared_ptr<Type[]> &value) {
    return *reinterpret_cast<array_type *>(value.get());
  }
};

template <typename Type> struct wrapper_traits<std::shared_ptr<Type>> {
  using semantics = ::dingo::semantics::shared_owner;
  using family = standard_family;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = std::shared_ptr<Element>;

  static Type &element(const std::shared_ptr<Type> &value) { return *value; }

  template <typename... Args>
  static std::shared_ptr<Type> make(Args &&...args) {
    return std::make_shared<Type>(std::forward<Args>(args)...);
  }

  static std::shared_ptr<Type> adopt(Type *element) {
    return std::shared_ptr<Type>(element);
  }

  template <typename Deleter>
  static std::shared_ptr<Type> adopt(Type *element, Deleter deleter) {
    return std::shared_ptr<Type>(element, std::move(deleter));
  }
};

template <typename Type> struct wrapper_traits<std::optional<Type>> {
  using semantics = ::dingo::semantics::optional_value;
  using family = standard_family;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = std::optional<Element>;

  static bool has_element(const std::optional<Type> &value) {
    return value.has_value();
  }

  static Type &element(std::optional<Type> &value) { return *value; }
  static const Type &element(const std::optional<Type> &value) {
    return *value;
  }
  static Type &&element(std::optional<Type> &&value) {
    return std::move(*value);
  }

  template <typename... Args> static std::optional<Type> make(Args &&...args) {
    return std::optional<Type>(std::in_place, std::forward<Args>(args)...);
  }
};

template <typename... Types> struct wrapper_traits<std::variant<Types...>> {
  using semantics = ::dingo::semantics::variant_value;
  using family = standard_family;
  using arguments = type_list<Types...>;
  template <typename... Elements> using rebind = std::variant<Elements...>;

  template <std::size_t Index>
  static auto &element(std::variant<Types...> &value) {
    return std::get<Index>(value);
  }
  template <std::size_t Index>
  static auto &element(const std::variant<Types...> &value) {
    return std::get<Index>(value);
  }
  template <std::size_t Index>
  static auto &&element(std::variant<Types...> &&value) {
    return std::get<Index>(std::move(value));
  }

  static std::size_t index(const std::variant<Types...> &value) {
    return value.index();
  }

  template <typename... Args>
  static std::variant<Types...> make(Args &&...args) {
    return std::variant<Types...>(std::in_place_index<0>,
                                  std::forward<Args>(args)...);
  }
};

// The wrappers of the language are wrappers like the ones of the library: the
// extension point is the same.
template <typename Type> struct wrapper_traits<Type *> {
  using semantics = ::dingo::semantics::pointer;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = Element *;

  static Type &element(Type *value) { return *value; }

  template <typename... Args> static Type *make(Args &&...args) {
    return new Type(std::forward<Args>(args)...);
  }

  // A slot that keeps a pointer owns the element it points to, which its
  // factory made.
  static void dispose(Type *value) noexcept { delete value; }
};

template <typename Type> struct wrapper_traits<Type &> {
  using semantics = ::dingo::semantics::reference;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = Element &;

  static Type &element(Type &value) { return value; }
};

// An array decays to the address of its first element.
template <typename Type, std::size_t Extent>
struct wrapper_traits<Type[Extent]> {
  using semantics = ::dingo::semantics::fixed_array;
  static constexpr std::size_t extent = Extent;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = Element[Extent];

  template <typename Array> static auto first(Array &value) {
    return &value[0];
  }
};

template <typename Type> struct wrapper_traits<Type[]> {
  using semantics = ::dingo::semantics::dynamic_array;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = Element[];

  template <typename Array> static auto first(Array &value) {
    return &value[0];
  }
};

} // namespace dingo

namespace dingo::core {

namespace detail {
template <typename Kind> constexpr semantics_descriptor make_semantics();
} // namespace detail

// The descriptor of the wrapper semantics `Kind`, from its wrapper_semantics.
template <typename Kind>
inline constexpr semantics_descriptor semantics_v =
    detail::make_semantics<Kind>();

namespace detail {
// Whether a type declares a wrapper semantics: its wrapper_semantics give its
// properties and its forms.
template <typename Kind, typename = void>
struct declares_semantics : std::false_type {};

template <typename Kind>
struct declares_semantics<
    Kind, std::void_t<decltype(::dingo::wrapper_semantics<Kind>::properties),
                      typename ::dingo::wrapper_semantics<Kind>::operations>>
    : std::true_type {};

// Whether a form leads to another kind, which a kind of it names.
constexpr bool leads_to_semantics(operation form) {
  return info(form).from_position == type_position::target ||
         info(form).to_position == type_position::target;
}

template <operation... Forms> constexpr bool distinct_forms() {
  constexpr operation forms[] = {Forms..., operation{}};
  for (std::size_t left = 0; left < sizeof...(Forms); ++left) {
    for (std::size_t right = left + 1; right < sizeof...(Forms); ++right) {
      if (forms[left] == forms[right]) {
        return false;
      }
    }
  }
  return true;
}

template <operation Form, typename Target> struct semantics_operation_of {
  static constexpr void add(semantics_descriptor &kind) {
    kind.forms |= std::uint32_t(1) << static_cast<unsigned>(Form);
    if constexpr (!std::is_void_v<Target>) {
      kind.targeted |= std::uint32_t(1) << static_cast<unsigned>(Form);
      kind.targets[static_cast<std::size_t>(Form)] = &semantics_v<Target>;
    }
  }
};

// The forms of the kind `Kind`, a type_list of op, which are forms of a
// kind, each named once, with a kind exactly when the form leads to one.
template <typename Kind, typename List> struct semantics_operations_of {
  static_assert(
      require<dependent_false<List>, diag::operations_malformed, Kind>());
  static constexpr void add(semantics_descriptor & /*kind*/) {}
};

template <typename Kind, operation... Forms, typename... Targets>
struct semantics_operations_of<Kind,
                               type_list<::dingo::op<Forms, Targets>...>> {
  static_assert(require<(info(Forms).of_semantics && ...),
                        diag::operation_of_leaf, Kind>());
  static_assert(
      require<((!leads_to_semantics(Forms) || !std::is_void_v<Targets>) && ...),
              diag::operation_needs_target, Kind>());
  static_assert(
      require<((leads_to_semantics(Forms) || std::is_void_v<Targets>) && ...),
              diag::operation_takes_no_target, Kind>());
  static_assert(
      require<distinct_forms<Forms...>(), diag::operation_repeated, Kind>());
  static_assert(require<(((::dingo::wrapper_semantics<Kind>::properties &
                           info(Forms).semantics_properties) ==
                          info(Forms).semantics_properties) &&
                         ...),
                        diag::operation_needs_property, Kind>());

  static constexpr void add(semantics_descriptor &kind) {
    (semantics_operation_of<Forms, Targets>::add(kind), ...);
  }
};

template <typename Kind> constexpr semantics_descriptor make_semantics() {
  static_assert(operation_count <= max_semantics_operations);
  static_assert(require<declares_semantics<Kind>::value,
                        diag::semantics_undeclared, Kind>());
  semantics_descriptor kind;
  if constexpr (declares_semantics<Kind>::value) {
    using traits = ::dingo::wrapper_semantics<Kind>;
    static_assert(require<(traits::properties & ~property::all) == 0,
                          diag::property_unknown, Kind>());
    kind.id = type_id_v<Kind>;
    kind.wrapper = true;
    kind.properties = traits::properties;
    semantics_operations_of<Kind, typename traits::operations>::add(kind);
  }
  return kind;
}
} // namespace detail

namespace detail {
template <typename Kind> struct semantics_of_type {
  static constexpr const semantics_descriptor *value = &semantics_v<Kind>;
};

template <> struct semantics_of_type<void> {
  static constexpr const semantics_descriptor *value = &leaf_semantics;
};
} // namespace detail

namespace detail {
template <typename Type, typename = void> struct wrapper_semantics_of {
  using type = void;
};

template <typename Type>
struct wrapper_semantics_of<
    Type, std::void_t<typename ::dingo::wrapper_traits<Type>::semantics>> {
  using type = typename ::dingo::wrapper_traits<Type>::semantics;
};
} // namespace detail

// The wrapper semantics of a type, or void for a leaf.
template <typename Type>
using wrapper_semantics_t =
    typename detail::wrapper_semantics_of<std::remove_cv_t<Type>>::type;

// Whether a type is a wrapper rather than a leaf.
template <typename Type>
inline constexpr bool is_wrapper_v = !std::is_void_v<wrapper_semantics_t<Type>>;

// Whether the wrapper traits of `Type` release what an object of it owns beyond
// itself when the slot that keeps it destroys it.
template <typename Type, typename = void>
inline constexpr bool has_dispose_v = false;

template <typename Type>
inline constexpr bool
    has_dispose_v<Type, std::void_t<decltype(wrapper_traits<Type>::dispose(
                            std::declval<Type &>()))>> = true;

// The descriptor of the wrapper semantics of a type, or the leaf kind.
template <typename Type>
inline constexpr const semantics_descriptor *wrapper_semantics_v =
    detail::semantics_of_type<wrapper_semantics_t<Type>>::value;

// The kind of the addresses of the language.
inline constexpr const semantics_descriptor *address_semantics_v =
    wrapper_semantics_v<char *>;

// Whether a type is a wrapper of a kind of all of the property bits
// `Properties`.
template <typename Type, std::uint16_t Properties>
inline constexpr bool has_properties_v =
    is_wrapper_v<Type> &&
    (detail::semantics_of_type<wrapper_semantics_t<Type>>::value->properties &
     Properties) == Properties;

namespace detail {
template <typename Traits, typename = void> struct wrapper_family {
  static constexpr type_id value = nullptr;
};

template <typename Traits>
struct wrapper_family<Traits, std::void_t<typename Traits::family>> {
  static constexpr type_id value = type_id_v<typename Traits::family>;
};

template <typename Traits, typename = void> struct wrapper_extent {
  static constexpr std::uint32_t value = 0;
};

template <typename Traits>
struct wrapper_extent<Traits, std::void_t<decltype(Traits::extent)>> {
  static constexpr std::uint32_t value = Traits::extent;
};

template <typename Type> constexpr type_tree make_type_tree();

template <typename List> struct argument_type_trees;

template <typename... Arguments>
struct argument_type_trees<type_list<Arguments...>> {
  static constexpr void append(type_tree &result) {
    (append_one(result, make_type_tree<Arguments>()), ...);
  }

  static constexpr void append_one(type_tree &result,
                                   const type_tree &argument) {
    for (std::size_t index = 0; index < argument.size; ++index) {
      result.nodes[result.size + index] = argument.nodes[index];
    }
    result.size += argument.size;
  }
};

// Whether the wrapper `value` is copied: a wrapper of a kind copied as a
// handle, or one that holds its elements in place and is not immovable, when
// every element is copied.
constexpr bool copies_wrapper(const type_tree &value) {
  const std::uint16_t properties = value.nodes[0].kind->properties;
  if ((properties & property::copyable) != 0) {
    return true;
  }
  if ((properties & property::in_place) == 0 ||
      (properties & property::immovable) != 0) {
    return false;
  }
  std::size_t index = 1;
  for (std::size_t argument = 0; argument < value.nodes[0].arity; ++argument) {
    if (!value.nodes[index].copyable) {
      return false;
    }
    index += subtree_size(value, index);
  }
  return true;
}

template <typename Type> constexpr type_tree make_type_tree() {
  using value_type = std::remove_cv_t<Type>;
  using traits = wrapper_traits<value_type>;
  type_tree result;
  type_tree_node &node = result.nodes[0];
  node.kind = wrapper_semantics_v<value_type>;
  // Only a leaf has an identity of its own; a wrapper is its kind and family.
  if constexpr (!is_wrapper_v<value_type>) {
    node.type = type_id_v<value_type>;
  }
  // A leaf is copyable as its type is; a wrapper as its kind declares. A leaf
  // that is incomplete where it is requested is neither copied nor
  // constructed there, as only a reference or a pointer to it is requested.
  if constexpr (!is_wrapper_v<value_type> &&
                !core::detail::is_complete<value_type>::value &&
                !std::is_void_v<value_type>) {
    node.copyable = false;
    node.constructible = false;
  } else {
    if constexpr (!is_wrapper_v<value_type>) {
      node.copyable = is_copied_v<value_type>;
    }
    node.constructible = !std::is_abstract_v<value_type>;
  }
  node.qualifiers = static_cast<std::uint8_t>(
      (std::is_const_v<Type> ? const_qualified : 0) |
      (std::is_volatile_v<Type> ? volatile_qualified : 0));
  result.size = 1;
  if constexpr (is_wrapper_v<value_type>) {
    node.family = wrapper_family<traits>::value;
    node.extent = wrapper_extent<traits>::value;
    node.arity = static_cast<std::uint8_t>(traits::arguments::size);
    argument_type_trees<typename traits::arguments>::append(result);
    node.copyable = copies_wrapper(result);
    // The elements of an array have the qualifiers of the array.
    if constexpr (has_properties_v<value_type, property::extent> ||
                  has_properties_v<value_type, property::unsized>) {
      result.nodes[1].qualifiers |= node.qualifiers;
    }
  }
  return result;
}
} // namespace detail

// The type tree of a C++ type, with the cv-qualifiers of its objects.
template <typename Type>
inline constexpr type_tree type_tree_v = detail::make_type_tree<Type>();

namespace detail {
template <typename Type, typename = void> struct canonical {
  using type = std::remove_cv_t<Type>;
};

template <typename Traits, typename List> struct canonical_rebind;

template <typename Traits, typename... Arguments>
struct canonical_rebind<Traits, type_list<Arguments...>> {
  using type =
      typename Traits::template rebind<typename canonical<Arguments>::type...>;
};

template <typename Type>
struct canonical<Type, std::enable_if_t<is_wrapper_v<Type>>> {
  using traits = wrapper_traits<std::remove_cv_t<Type>>;
  using type =
      typename canonical_rebind<traits, typename traits::arguments>::type;
};
} // namespace detail

namespace detail {
template <typename Type, typename = void> struct leaf_of {
  using type = Type;
};

// The leaf a wrapper holds, through its first argument; a wrapper of a variadic
// kind holds several, and is its own leaf.
template <typename Type>
struct leaf_of<Type,
               std::enable_if_t<is_wrapper_v<Type> &&
                                !has_properties_v<Type, property::variadic>>> {
  using type = typename leaf_of<std::remove_cv_t<
      type_list_at_t<0, typename wrapper_traits<Type>::arguments>>>::type;
};
} // namespace detail

// The leaf of a type, which diagnostics name as the type being constructed.
template <typename Type>
using leaf_of_t = typename detail::leaf_of<
    std::remove_cv_t<std::remove_reference_t<Type>>>::type;

namespace detail {
template <typename Type, typename Leaf, typename = void> struct replace_leaf {
  using type = Leaf;
};

template <typename Type, typename Leaf>
struct replace_leaf<
    Type, Leaf,
    std::enable_if_t<is_wrapper_v<Type> &&
                     !has_properties_v<Type, property::variadic>>> {
  using type =
      typename wrapper_traits<Type>::template rebind<typename replace_leaf<
          type_list_at_t<0, typename wrapper_traits<Type>::arguments>,
          Leaf>::type>;
};
} // namespace detail

// The type with its leaf, through the first argument of every wrapper, replaced
// by `Leaf`.
template <typename Type, typename Leaf>
using replace_leaf_t = typename detail::replace_leaf<Type, Leaf>::type;

// The type a type tree describes: the type with cv-qualification removed at
// every wrapper. Two types of the same type tree have the same canonical type.
template <typename Type>
using canonical_t = typename detail::canonical<Type>::type;

} // namespace dingo::core
