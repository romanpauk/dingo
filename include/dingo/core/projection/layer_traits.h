//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The layer kind a C++ type is: the projection of the shape algebra of the spec
// onto C++ types. Raw pointers, references, and arrays are language layers, of
// the pointer, reference, and array kinds of the library. Wrappers are
// classified by layer_traits, which the standard wrappers specialize here and
// user wrappers through the public extension point; no wrapper gets other
// treatment. A layer kind is a tag whose layer_kind_traits declare its
// properties and forms, and the library's kinds are declared as a user's are.
//
// A specialization names the layer kind, the wrapper family, and the argument
// types, and provides `rebind`, the same layer of other argument types,
// `element`, which reaches the element of a layer object, and `make`, which
// makes a layer object from the arguments of its element. A handle that owns
// its element also adopts an element it is given the address of, and a
// sole-owner handle hands its element to the sharing handle of its family.

#include <dingo/core/base/mechanics.h>
#include <dingo/core/primitives/shape.h>
#include <dingo/core/vocabulary/compile_diagnostics.gen.h>
#include <dingo/core/vocabulary/layer_kinds.gen.h>
#include <dingo/core/vocabulary/route_forms.gen.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace dingo {

// The public extension point that makes a wrapper type a layer of the shape
// algebra. A specialization declares
//
//   kind       the layer kind of the wrapper, a tag whose layer_kind_traits
//              declare it, such as unique_kind, shared_kind, optional_kind,
//              alternative_kind, or a kind of the user
//   family     a type naming the family of the wrapper; a route converts a
//              wrapper only to another wrapper of its family, such as a
//              sole-owner handle to a sharing handle
//   arguments  the element types, as a type_list
//   rebind     the wrapper of other element types
//   element    the element of a wrapper object
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
  using kind = unique_kind;
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
  using kind = unique_kind;
  using family = standard_family;
  using arguments = type_list<Type[]>;
  template <typename Element> using rebind = std::unique_ptr<Element>;
  using array_type = Type[];

  static array_type &element(const std::unique_ptr<Type[]> &value) {
    return *reinterpret_cast<array_type *>(value.get());
  }
};

template <typename Type> struct wrapper_traits<std::shared_ptr<Type[]>> {
  using kind = shared_kind;
  using family = standard_family;
  using arguments = type_list<Type[]>;
  template <typename Element> using rebind = std::shared_ptr<Element>;
  using array_type = Type[];

  static array_type &element(const std::shared_ptr<Type[]> &value) {
    return *reinterpret_cast<array_type *>(value.get());
  }
};

template <typename Type> struct wrapper_traits<std::shared_ptr<Type>> {
  using kind = shared_kind;
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
  using kind = optional_kind;
  using family = standard_family;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = std::optional<Element>;

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
  using kind = alternative_kind;
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

  template <typename... Args>
  static std::variant<Types...> make(Args &&...args) {
    return std::variant<Types...>(std::in_place_index<0>,
                                  std::forward<Args>(args)...);
  }
};

} // namespace dingo

namespace dingo::core {

namespace detail {
template <typename Kind> constexpr kind_descriptor make_kind();
} // namespace detail

// The descriptor of the layer kind `Kind`, from its layer_kind_traits.
template <typename Kind>
inline constexpr kind_descriptor kind_v = detail::make_kind<Kind>();

namespace detail {
// Whether a type declares a layer kind: its layer_kind_traits give its
// properties and its forms.
template <typename Kind, typename = void>
struct declares_kind : std::false_type {};

template <typename Kind>
struct declares_kind<
    Kind, std::void_t<decltype(::dingo::layer_kind_traits<Kind>::properties),
                      typename ::dingo::layer_kind_traits<Kind>::forms>>
    : std::true_type {};

// Whether a form leads to another kind, which a kind of it names.
constexpr bool leads_to_kind(route_form form) {
  return info(form).from_layer == form_layer::target ||
         info(form).to_layer == form_layer::target;
}

template <route_form... Forms> constexpr bool distinct_forms() {
  constexpr route_form forms[] = {Forms..., route_form{}};
  for (std::size_t left = 0; left < sizeof...(Forms); ++left) {
    for (std::size_t right = left + 1; right < sizeof...(Forms); ++right) {
      if (forms[left] == forms[right]) {
        return false;
      }
    }
  }
  return true;
}

template <route_form Form, typename Target> struct kind_form_of {
  static constexpr void add(kind_descriptor &kind) {
    kind.forms |= std::uint32_t(1) << static_cast<unsigned>(Form);
    if constexpr (!std::is_void_v<Target>) {
      kind.targeted |= std::uint32_t(1) << static_cast<unsigned>(Form);
      kind.targets[static_cast<std::size_t>(Form)] = &kind_v<Target>;
    }
  }
};

// The forms of the kind `Kind`, a type_list of kind_form, which are forms of a
// kind, each named once, with a kind exactly when the form leads to one.
template <typename Kind, typename List> struct kind_forms_of {
  static_assert(
      require<dependent_false<List>, diag::kind_forms_malformed, Kind>());
  static constexpr void add(kind_descriptor & /*kind*/) {}
};

template <typename Kind, route_form... Forms, typename... Targets>
struct kind_forms_of<Kind, type_list<::dingo::kind_form<Forms, Targets>...>> {
  static_assert(
      require<(info(Forms).of_kind && ...), diag::kind_form_of_leaf, Kind>());
  static_assert(
      require<((!leads_to_kind(Forms) || !std::is_void_v<Targets>) && ...),
              diag::kind_form_needs_target, Kind>());
  static_assert(
      require<((leads_to_kind(Forms) || std::is_void_v<Targets>) && ...),
              diag::kind_form_takes_no_target, Kind>());
  static_assert(
      require<distinct_forms<Forms...>(), diag::kind_form_repeated, Kind>());

  static constexpr void add(kind_descriptor &kind) {
    (kind_form_of<Forms, Targets>::add(kind), ...);
  }
};

template <typename Kind> constexpr kind_descriptor make_kind() {
  static_assert(route_form_count <= max_kind_forms);
  static_assert(
      require<declares_kind<Kind>::value, diag::layer_kind_undeclared, Kind>());
  kind_descriptor kind;
  if constexpr (declares_kind<Kind>::value) {
    using traits = ::dingo::layer_kind_traits<Kind>;
    static_assert(require<(traits::properties & ~layer_property::all) == 0,
                          diag::kind_property_unknown, Kind>());
    kind.id = type_id_v<Kind>;
    kind.layer = true;
    kind.properties = traits::properties;
    kind_forms_of<Kind, typename traits::forms>::add(kind);
  }
  return kind;
}
} // namespace detail

// A type that is not a layer is a leaf, of no kind.
template <typename Type, typename = void> struct layer_traits {
  using kind = void;
};

// A wrapper is the layer its wrapper_traits declare.
template <typename Type>
struct layer_traits<Type,
                    std::void_t<typename ::dingo::wrapper_traits<Type>::kind>>
    : ::dingo::wrapper_traits<Type> {};

template <typename Type> struct layer_traits<Type *> {
  using kind = ::dingo::pointer_kind;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = Element *;

  static Type &element(Type *value) { return *value; }

  template <typename... Args> static Type *make(Args &&...args) {
    return new Type(std::forward<Args>(args)...);
  }
};

template <typename Type> struct layer_traits<Type &> {
  using kind = ::dingo::reference_kind;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = Element &;

  static Type &element(Type &value) { return value; }
};

// An array decays to the address of its first element.
template <typename Type, std::size_t Extent> struct layer_traits<Type[Extent]> {
  using kind = ::dingo::array_kind;
  static constexpr std::size_t extent = Extent;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = Element[Extent];

  template <typename Array> static auto first(Array &value) {
    return &value[0];
  }
};

template <typename Type> struct layer_traits<Type[]> {
  using kind = ::dingo::unsized_array_kind;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = Element[];

  template <typename Array> static auto first(Array &value) {
    return &value[0];
  }
};

namespace detail {
template <typename Kind> struct kind_of_type {
  static constexpr const kind_descriptor *value = &kind_v<Kind>;
};

template <> struct kind_of_type<void> {
  static constexpr const kind_descriptor *value = &leaf_kind;
};
} // namespace detail

// The layer kind of a type, or void for a leaf.
template <typename Type>
using layer_kind_t = typename layer_traits<std::remove_cv_t<Type>>::kind;

// Whether a type is a layer rather than a leaf.
template <typename Type>
inline constexpr bool is_layer_v = !std::is_void_v<layer_kind_t<Type>>;

// The descriptor of the layer kind of a type, or the leaf kind.
template <typename Type>
inline constexpr const kind_descriptor *layer_kind_v =
    detail::kind_of_type<layer_kind_t<Type>>::value;

// The kind of the addresses of the language.
inline constexpr const kind_descriptor *address_kind_v = layer_kind_v<char *>;

// Whether a type is a layer of a kind of all of the layer_property bits
// `Properties`.
template <typename Type, std::uint16_t Properties>
inline constexpr bool has_layer_properties_v =
    is_layer_v<Type> &&
    (detail::kind_of_type<layer_kind_t<Type>>::value->properties &
     Properties) == Properties;

namespace detail {
template <typename Traits, typename = void> struct layer_family {
  static constexpr type_id value = nullptr;
};

template <typename Traits>
struct layer_family<Traits, std::void_t<typename Traits::family>> {
  static constexpr type_id value = type_id_v<typename Traits::family>;
};

template <typename Traits, typename = void> struct layer_extent {
  static constexpr std::uint32_t value = 0;
};

template <typename Traits>
struct layer_extent<Traits, std::void_t<decltype(Traits::extent)>> {
  static constexpr std::uint32_t value = Traits::extent;
};

template <typename Type> constexpr shape make_shape();

template <typename List> struct argument_shapes;

template <typename... Arguments>
struct argument_shapes<type_list<Arguments...>> {
  static constexpr void append(shape &result) {
    (append_one(result, make_shape<Arguments>()), ...);
  }

  static constexpr void append_one(shape &result, const shape &argument) {
    for (std::size_t index = 0; index < argument.size; ++index) {
      result.nodes[result.size + index] = argument.nodes[index];
    }
    result.size += argument.size;
  }
};

template <typename Type> constexpr shape make_shape() {
  using value_type = std::remove_cv_t<Type>;
  using traits = layer_traits<value_type>;
  shape result;
  shape_node &node = result.nodes[0];
  node.kind = layer_kind_v<value_type>;
  // Only a leaf has an identity of its own; a layer is its kind and family.
  if constexpr (!is_layer_v<value_type>) {
    node.type = type_id_v<value_type>;
  }
  if constexpr (std::is_reference_v<value_type>) {
    node.copyable = false;
  } else {
    node.copyable = std::is_copy_constructible_v<value_type>;
  }
  node.constructible = !std::is_abstract_v<value_type>;
  node.qualifiers = static_cast<std::uint8_t>(
      (std::is_const_v<Type> ? const_qualified : 0) |
      (std::is_volatile_v<Type> ? volatile_qualified : 0));
  result.size = 1;
  if constexpr (is_layer_v<value_type>) {
    node.family = layer_family<traits>::value;
    node.extent = layer_extent<traits>::value;
    node.arity = static_cast<std::uint8_t>(traits::arguments::size);
    argument_shapes<typename traits::arguments>::append(result);
    // The elements of an array have the qualifiers of the array.
    if constexpr (has_layer_properties_v<value_type, layer_property::extent> ||
                  has_layer_properties_v<value_type, layer_property::unsized>) {
      result.nodes[1].qualifiers |= node.qualifiers;
    }
  }
  return result;
}
} // namespace detail

// The shape of a C++ type, with the cv-qualifiers of its objects.
template <typename Type>
inline constexpr shape shape_v = detail::make_shape<Type>();

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
struct canonical<Type, std::enable_if_t<is_layer_v<Type>>> {
  using traits = layer_traits<std::remove_cv_t<Type>>;
  using type =
      typename canonical_rebind<traits, typename traits::arguments>::type;
};
} // namespace detail

namespace detail {
template <typename Type, typename = void> struct leaf_of {
  using type = Type;
};

// The leaf a layer holds, through its first argument; a layer of a variadic
// kind holds several, and is its own leaf.
template <typename Type>
struct leaf_of<
    Type,
    std::enable_if_t<is_layer_v<Type> &&
                     !has_layer_properties_v<Type, layer_property::variadic>>> {
  using type = typename leaf_of<std::remove_cv_t<
      type_list_at_t<0, typename layer_traits<Type>::arguments>>>::type;
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
    std::enable_if_t<is_layer_v<Type> &&
                     !has_layer_properties_v<Type, layer_property::variadic>>> {
  using type =
      typename layer_traits<Type>::template rebind<typename replace_leaf<
          type_list_at_t<0, typename layer_traits<Type>::arguments>,
          Leaf>::type>;
};
} // namespace detail

// The type with its leaf, through the first argument of every layer, replaced
// by `Leaf`.
template <typename Type, typename Leaf>
using replace_leaf_t = typename detail::replace_leaf<Type, Leaf>::type;

// The type a shape describes: the type with cv-qualification removed at every
// layer. Two types of the same shape have the same canonical type.
template <typename Type>
using canonical_t = typename detail::canonical<Type>::type;

} // namespace dingo::core
