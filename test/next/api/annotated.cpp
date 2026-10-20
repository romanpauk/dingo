//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Annotated interfaces in the next implementation: bindings of one type serve
// distinct interfaces annotated by tags, which requests name.

#include <dingo/container.h>
#include <dingo/lookup/lookup.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/type/annotated.h>

#include <gtest/gtest.h>

#include <cstddef>

namespace {

using dingo::scope;
using dingo::storage;

struct primary_tag {};
struct replica_tag {};

struct database {
  int id;
};

struct repository {
  repository(dingo::annotated<database &, primary_tag> primary_db,
             dingo::annotated<database &, replica_tag> replica_db)
      : primary(primary_db), replica(replica_db) {}
  database &primary;
  database &replica;
};

TEST(next_annotated, serves_annotated_interfaces) {
  database primary{1};
  database replica{2};
  dingo::container<> container;
  container.register_type<
      scope<dingo::external>, storage<database *>,
      dingo::interfaces<dingo::annotated<database, primary_tag>>>(&primary);
  container.register_type<
      scope<dingo::external>, storage<database *>,
      dingo::interfaces<dingo::annotated<database, replica_tag>>>(&replica);
  repository resolved = container.construct<repository>();
  EXPECT_EQ(&resolved.primary, &primary);
  EXPECT_EQ(&resolved.replica, &replica);
  database &direct =
      container.resolve<dingo::annotated<database &, replica_tag>>();
  EXPECT_EQ(&direct, &replica);
  EXPECT_THROW(container.resolve<database &>(),
               dingo::type_not_found_exception);
}

TEST(next_annotated, serves_annotated_interfaces_statically) {
  using bindings = dingo::bindings<
      dingo::bind<scope<dingo::shared>, storage<database>,
                  dingo::interfaces<dingo::annotated<database, primary_tag>>,
                  dingo::factory<dingo::constructor<database(int)>>>,
      dingo::bind<scope<dingo::shared>, storage<int>>>;
  dingo::static_container<bindings> container;
  database &primary =
      container.resolve<dingo::annotated<database &, primary_tag>>();
  EXPECT_EQ(primary.id, 0);
}

struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<std::size_t, dingo::annotated<database, primary_tag>>,
      dingo::associative<std::size_t, dingo::annotated<database, replica_tag>>>;
};

TEST(next_annotated, selects_annotated_interfaces_by_key) {
  database first{11};
  database second{22};
  dingo::container<traits> container;
  container.register_type<
      scope<dingo::external>, storage<database *>,
      dingo::interfaces<dingo::annotated<database, primary_tag>>>(
      &first, dingo::key_value{std::size_t(1)});
  container.register_type<
      scope<dingo::external>, storage<database *>,
      dingo::interfaces<dingo::annotated<database, replica_tag>>>(
      &second, dingo::key_value{std::size_t(1)});
  database &primary =
      container.resolve<dingo::annotated<database &, primary_tag>>(
          std::size_t(1));
  database &replica =
      container.resolve<dingo::annotated<database &, replica_tag>>(
          std::size_t(1));
  EXPECT_EQ(primary.id, 11);
  EXPECT_EQ(replica.id, 22);
}

} // namespace
