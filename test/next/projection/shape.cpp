//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/core/projection/layer_traits.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <variant>

namespace {

using namespace dingo::core;

struct leaf {};
struct move_only {
  move_only() = default;
  move_only(move_only &&) = default;
};

constexpr std::uint8_t kind(layer_kind value) {
  return static_cast<std::uint8_t>(value);
}

static_assert(shape_v<leaf>.size == 1);
static_assert(shape_v<leaf>.nodes[0].kind == kind(layer_kind::leaf));
static_assert(shape_v<leaf>.nodes[0].type == type_id_v<leaf>);
static_assert(shape_v<const leaf>.nodes[0].type == type_id_v<leaf>);
static_assert(shape_v<leaf>.nodes[0].copyable);
static_assert(!shape_v<move_only>.nodes[0].copyable);

using shared = std::shared_ptr<std::optional<leaf>>;
static_assert(shape_v<shared>.size == 3);
static_assert(shape_v<shared>.nodes[0].kind == kind(layer_kind::share));
static_assert(shape_v<shared>.nodes[0].family == type_id_v<standard_family>);
static_assert(shape_v<shared>.nodes[0].copyable);
static_assert(shape_v<shared>.nodes[1].kind == kind(layer_kind::opt));
static_assert(shape_v<shared>.nodes[2].type == type_id_v<leaf>);
static_assert(subtree(shape_v<shared>, 1) == shape_v<std::optional<leaf>>);

static_assert(shape_v<leaf *>.nodes[0].kind == kind(layer_kind::ptr));
static_assert(shape_v<leaf *>.nodes[0].family == nullptr);
static_assert(shape_v<leaf[2]>.nodes[0].extent == 2);
static_assert(!shape_v<leaf[2]>.nodes[0].copyable);
static_assert(shape_v<std::unique_ptr<leaf[]>>.nodes[1].kind ==
              kind(layer_kind::array_unbounded));

using alternative = std::variant<leaf, std::unique_ptr<leaf>>;
static_assert(shape_v<alternative>.nodes[0].arity == 2);
static_assert(shape_v<alternative>.size == 4);
static_assert(argument_index(shape_v<alternative>, 0, 1) == 2);
static_assert(subtree(shape_v<alternative>, 2) ==
              shape_v<std::unique_ptr<leaf>>);

// A created layer of any family serves a wrapper of every family.
constexpr shape created =
    wrap(kind(layer_kind::opt), any_family, true, shape_v<leaf>);
static_assert(shapes_match(created, shape_v<std::optional<leaf>>));
static_assert(!(created == shape_v<std::optional<leaf>>));
static_assert(!shapes_match(shape_v<std::unique_ptr<leaf>>,
                            shape_v<std::shared_ptr<leaf>>));

struct other {};
static_assert(replace_leaf(shape_v<std::shared_ptr<leaf>>, shape_v<other>)
                  .nodes[1]
                  .type == type_id_v<other>);

TEST(shape, describes_the_standard_wrappers) {
  EXPECT_EQ(shape_v<std::unique_ptr<leaf>>.size, 2u);
}

} // namespace
