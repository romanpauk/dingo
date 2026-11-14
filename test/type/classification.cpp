//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/request/classification.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

#include "support/custom_wrappers.h"

namespace dingo {
namespace {
struct interface {
  virtual ~interface() = default;
};
struct leaf : interface {};
struct other {};

using detail::array_layer_tag;
using detail::bounded_array_layer_tag;
using detail::pointer_layer_tag;
using detail::stack_element;
using detail::stack_of_t;
using detail::wrapper_layer_tag;

template <typename T, typename... Tags>
inline constexpr bool layers_are_v =
    std::is_same_v<typename stack_of_t<T, leaf>::layers, type_list<Tags...>>;

template <typename T>
inline constexpr stack_element element_of_v = stack_of_t<T, leaf>::element;

// The stack of a type: its layers around the leaf, its abstract interface or
// another type.
static_assert(layers_are_v<leaf> && element_of_v<leaf> == stack_element::leaf);
static_assert(layers_are_v<const leaf>);
static_assert(element_of_v<interface> == stack_element::interface);
static_assert(element_of_v<other> == stack_element::other);
static_assert(layers_are_v<interface *, pointer_layer_tag> &&
              element_of_v<interface *> == stack_element::interface);
static_assert(layers_are_v<leaf[2], bounded_array_layer_tag>);
static_assert(layers_are_v<std::shared_ptr<leaf>,
                           wrapper_layer_tag<std::shared_ptr<leaf>>>);
static_assert(
    layers_are_v<std::unique_ptr<leaf[]>,
                 wrapper_layer_tag<std::unique_ptr<leaf[]>>, array_layer_tag>);
static_assert(layers_are_v<std::shared_ptr<leaf[2]>,
                           wrapper_layer_tag<std::shared_ptr<leaf[2]>>,
                           bounded_array_layer_tag>);
static_assert(
    layers_are_v<std::optional<std::shared_ptr<interface>>,
                 wrapper_layer_tag<std::optional<std::shared_ptr<interface>>>,
                 wrapper_layer_tag<std::shared_ptr<interface>>> &&
    element_of_v<std::optional<std::shared_ptr<interface>>> ==
        stack_element::interface);
static_assert(
    layers_are_v<
        std::variant<leaf, other>,
        detail::alternative_layer_tag<std::variant<leaf, other>, leaf>> &&
    element_of_v<std::variant<leaf, other>> == stack_element::leaf);

// Two wrappers of different elements are one layer.
static_assert(std::is_same_v<detail::layer_identity_t<std::shared_ptr<leaf>>,
                             detail::layer_identity_t<std::shared_ptr<other>>>);

template <typename... Wrappers>
using universe_of =
    detail::layer_universe<type_list<detail::wrapper_layer_tag<Wrappers>...>>;

constexpr bool same_kind(const semantics::layer_kind &a,
                         const semantics::layer_kind &b) {
  return a.owns == b.owns && a.shares_element == b.shares_element &&
         a.copyable == b.copyable && a.made_from_value == b.made_from_value &&
         a.holds_element_only == b.holds_element_only &&
         a.relocatable == b.relocatable && a.may_be_empty == b.may_be_empty &&
         a.has_empty_value == b.has_empty_value;
}

// The layers of the standard wrappers, by their type traits.
using standard = detail::layer_universe<
    type_list<wrapper_layer_tag<std::shared_ptr<leaf>>,
              wrapper_layer_tag<std::unique_ptr<leaf>>,
              wrapper_layer_tag<std::optional<leaf>>,
              detail::alternative_layer_tag<std::variant<leaf, other>, leaf>>>;
static_assert(standard::value.count == 8);
static_assert(standard::value.kinds[4].owns == semantics::owner::shared &&
              standard::value.kinds[4].copyable &&
              standard::value.kinds[4].adopts);
static_assert(standard::value.kinds[5].owns == semantics::owner::sole &&
              standard::value.kinds[5].shares_element &&
              !standard::value.kinds[5].copyable &&
              standard::value.kinds[5].adopts);
static_assert(standard::value.kinds[6].has_empty_value &&
              !standard::value.kinds[6].shares_element);
static_assert(standard::value.kinds[7].holds_element_only);
// A sole owner converts to the sharing handle of its family when it is in
// the universe, and to nothing when it is not.
static_assert(standard::value.kinds[5].converts == (1u << 4));
static_assert(universe_of<std::unique_ptr<leaf>>::value.kinds[4].converts == 0);

// A wrapper a user defines is a layer by its type traits alone, as the
// standard one of the same semantics is.
using custom =
    universe_of<test_shared<leaf>, test_unique<leaf>, test_optional<leaf>>;
static_assert(same_kind(custom::value.kinds[4], standard::value.kinds[4]));
static_assert(same_kind(custom::value.kinds[5], standard::value.kinds[5]));
static_assert(same_kind(custom::value.kinds[6], standard::value.kinds[6]));
static_assert(custom::value.kinds[5].converts == (1u << 4));

// The same layer met twice is one layer of the universe.
using twice = detail::layer_universe<
    type_list<wrapper_layer_tag<std::shared_ptr<leaf>>, pointer_layer_tag,
              wrapper_layer_tag<std::shared_ptr<interface>>>>;
static_assert(twice::value.count == 5);
static_assert(twice::index<wrapper_layer_tag<std::shared_ptr<interface>>>() ==
              4);
static_assert(twice::index<pointer_layer_tag>() == semantics::pointer_layer);

TEST(classification_test, standard_layers_are_possible) {
  for (std::size_t index = 0; index < standard::value.count; ++index) {
    EXPECT_TRUE(semantics::possible(standard::value.kinds[index])) << index;
  }
}
} // namespace
} // namespace dingo
