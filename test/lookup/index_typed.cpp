//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "index_common.h"

namespace dingo {

TEST(index_test, typed_key_many_populates_custom_collections) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct first_processor : processor {
    int id() const override { return 1; }
  };
  struct second_processor : processor {
    int id() const override { return 2; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<typed<processor_key, processor, many>>;
  };

  container<traits> container;
  container
      .template register_type<scope<shared>, storage<first_processor>,
                              interfaces<processor>, key_type<processor_key>>();
  container
      .template register_type<scope<shared>, storage<second_processor>,
                              interfaces<processor>, key_type<processor_key>>();

  auto tracked = container.template resolve<tracked_collection<processor *>>(
      key_type<processor_key>{});

  ASSERT_EQ(tracked.reserve_count, 2U);
  ASSERT_EQ(tracked.values.size(), 2U);
  ASSERT_EQ(sorted_processor_ids(tracked.values), (std::vector<int>{1, 2}));
}

TEST(index_test, base_many_enumerates_implicit_typed_key_collection_entries) {
  struct processor_key {};
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct first_processor : processor {
    int id() const override { return 1; }
  };
  struct second_processor : processor {
    int id() const override { return 2; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<base<many, ordered>>;
  };

  container<traits> container;
  container
      .template register_type<scope<shared>, storage<first_processor>,
                              interfaces<processor>, key_type<processor_key>>();
  container
      .template register_type<scope<shared>, storage<second_processor>,
                              interfaces<processor>, key_type<processor_key>>();

  auto processors = container.template resolve<std::vector<processor *>>(
      key_type<processor_key>{});

  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(sorted_processor_ids(processors), (std::vector<int>{1, 2}));
  EXPECT_THROW(
      (container.template resolve<processor &>(key_type<processor_key>{})),
      type_ambiguous_exception);
}

} // namespace dingo
