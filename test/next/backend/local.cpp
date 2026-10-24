//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The local bindings of a runtime registration: its runtime bindings are
// created by the first registration through its proxy, once, so a
// registration without any has none.

#include <dingo/backend/local.h>
#include <dingo/container.h>

#include <gtest/gtest.h>

namespace {

using host = dingo::container<>;
using locals = dingo::core::local_bindings<host, dingo::core::type_list<>>;

TEST(next_local_bindings, creates_the_runtime_bindings_on_demand_once) {
  locals bindings;
  EXPECT_EQ(bindings.runtime(), nullptr);
  const auto &table =
      dingo::core::lookup_table_v<dingo::dynamic_container_traits>;
  auto &created = bindings.runtime_or_create(table);
  EXPECT_EQ(bindings.runtime(), &created);
  EXPECT_EQ(&bindings.runtime_or_create(table), &created);
}

} // namespace
