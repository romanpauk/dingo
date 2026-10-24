//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/type/wrapper_traits.h>

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

template <typename Kind> constexpr const semantics_descriptor *kind() {
  return &semantics_v<Kind>;
}

static_assert(type_tree_v<leaf>.size == 1);
static_assert(!is_wrapper(type_tree_v<leaf>.nodes[0].kind));
static_assert(type_tree_v<leaf>.nodes[0].type == type_id_v<leaf>);
static_assert(type_tree_v<const leaf>.nodes[0].type == type_id_v<leaf>);
static_assert(type_tree_v<leaf>.nodes[0].copyable);
static_assert(!type_tree_v<move_only>.nodes[0].copyable);

using shared = std::shared_ptr<std::optional<leaf>>;
static_assert(type_tree_v<shared>.size == 3);
static_assert(same_semantics(type_tree_v<shared>.nodes[0].kind,
                             kind<dingo::semantics::shared_owner>()));
static_assert(type_tree_v<shared>.nodes[0].family ==
              type_id_v<dingo::standard_family>);
static_assert(type_tree_v<shared>.nodes[0].copyable);
static_assert(same_semantics(type_tree_v<shared>.nodes[1].kind,
                             kind<dingo::semantics::optional_value>()));
static_assert(type_tree_v<shared>.nodes[2].type == type_id_v<leaf>);
static_assert(subtree(type_tree_v<shared>, 1) ==
              type_tree_v<std::optional<leaf>>);

static_assert(same_semantics(type_tree_v<leaf *>.nodes[0].kind,
                             kind<dingo::semantics::pointer>()));
static_assert(type_tree_v<leaf *>.nodes[0].family == nullptr);
static_assert(type_tree_v<leaf[2]>.nodes[0].extent == 2);
static_assert(!type_tree_v<leaf[2]>.nodes[0].copyable);
static_assert(same_semantics(type_tree_v<std::unique_ptr<leaf[]>>.nodes[1].kind,
                             kind<dingo::semantics::dynamic_array>()));

using alternative = std::variant<leaf, std::unique_ptr<leaf>>;
static_assert(type_tree_v<alternative>.nodes[0].arity == 2);
static_assert(type_tree_v<alternative>.size == 4);
static_assert(argument_index(type_tree_v<alternative>, 0, 1) == 2);
static_assert(subtree(type_tree_v<alternative>, 2) ==
              type_tree_v<std::unique_ptr<leaf>>);

// A created wrapper of any family serves a wrapper of every family.
constexpr type_tree created = wrap(kind<dingo::semantics::optional_value>(),
                                   any_family, true, type_tree_v<leaf>);
static_assert(type_trees_match(created, type_tree_v<std::optional<leaf>>));
static_assert(!(created == type_tree_v<std::optional<leaf>>));
static_assert(!type_trees_match(type_tree_v<std::unique_ptr<leaf>>,
                                type_tree_v<std::shared_ptr<leaf>>));

struct other {};
static_assert(replace_leaf(type_tree_v<std::shared_ptr<leaf>>,
                           type_tree_v<other>)
                  .nodes[1]
                  .type == type_id_v<other>);

TEST(type_tree, describes_the_standard_wrappers) {
  EXPECT_EQ(type_tree_v<std::unique_ptr<leaf>>.size, 2u);
}

} // namespace
