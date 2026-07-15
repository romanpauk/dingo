//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "index_common.h"

namespace dingo {

TEST(index_test, base_one_ordered_resolves_implicit_static_keys) {
  expect_base_one_backend_resolves_implicit_static_keys<ordered>();
}

TEST(index_test, base_one_unordered_resolves_implicit_static_keys) {
  expect_base_one_backend_resolves_implicit_static_keys<unordered>();
}

TEST(index_test, base_many_ordered_enumerates_implicit_static_keys) {
  expect_base_many_backend_enumerates_implicit_static_keys<ordered>();
}

TEST(index_test, base_many_unordered_enumerates_implicit_static_keys) {
  expect_base_many_backend_enumerates_implicit_static_keys<unordered>();
}

TEST(index_test, base_many_enumerates_implicit_unkeyed_collection_entries) {
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
  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>>();
  container.template register_type<scope<shared>, storage<second_processor>,
                                   interfaces<processor>>();

  auto processors = container.template resolve<std::vector<processor *>>();

  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(sorted_processor_ids(processors), (std::vector<int>{1, 2}));
  EXPECT_THROW((container.template resolve<processor &>()),
               type_ambiguous_exception);
}

TEST(index_test, base_uses_configured_backend_for_default_index) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct processor_impl : processor {
    int id() const override { return 3; }
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<base<one, test_ordered_base_backend>>;
  };

  using selected_base =
      detail::base_lookup_definition_t<typename traits::lookup_definition_type>;
  static_assert(std::is_same_v<typename selected_base::backend_type,
                               test_ordered_base_backend>);

  container<traits> container;
  container.template register_type<scope<shared>, storage<processor_impl>,
                                   interfaces<processor>>();

  ASSERT_EQ(container.template resolve<processor &>().id(), 3);
}

TEST(index_test, collection_alias_behaves_like_no_key_many_lookup) {
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

  static_assert(std::is_same_v<collection<processor>, collection<processor>>);

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<collection<processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<first_processor>,
                                   interfaces<processor>>();
  container.template register_type<scope<shared>, storage<second_processor>,
                                   interfaces<processor>>();

  auto processors = container.template resolve<std::vector<processor *>>();

  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 1);
  ASSERT_EQ(processors[1]->id(), 2);
  EXPECT_THROW((container.template resolve<processor &>()),
               type_ambiguous_exception);
}

} // namespace dingo
