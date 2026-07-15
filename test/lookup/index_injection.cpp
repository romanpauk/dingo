//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "index_common.h"

namespace dingo {

#if __cplusplus >= 202002L
TEST(index_test, external_lookup_constructs_string_key_for_indexed_injection) {
  struct processor {
    virtual ~processor() = default;
    virtual int id() const = 0;
  };
  struct json_processor : processor {
    int id() const override { return 7; }
  };
  struct consumer {
    explicit consumer(
        dependency<processor &, external_key_string<std::string, "json">>
            selected)
        : selected_processor(selected) {}

    processor &selected_processor;
  };

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<associative<std::string, processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>, storage<json_processor>,
                                   interfaces<processor>>(
      dingo::key_value{std::string{"json"}});

  auto selected = container.template construct<consumer>();

  ASSERT_EQ(selected.selected_processor.id(), 7);
}
#endif

TEST(index_test, constructor_injects_fixed_indexed_enum_key) {
  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<
        associative<fixed_injection_processor_id, fixed_injection_processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>,
                                   storage<fixed_injection_processor_impl<1>>,
                                   interfaces<fixed_injection_processor>>(
      dingo::key_value{fixed_injection_processor_id::first});

  auto consumer = container.template construct<
      fixed_enum_consumer<fixed_injection_processor_id::first>>();

  ASSERT_EQ(consumer.processor.id(), 1);
}

TEST(index_test, selected_type_selector_registers_and_injects_keyed_binding) {
  container<> container;
  container.template register_type<
      scope<shared>, storage<fixed_injection_processor_impl<3>>,
      interfaces<detail::selected<
          fixed_injection_processor,
          detail::type_selector<selected_type_processor_tag>>>>();
  container
      .template register_type<scope<shared>, storage<selected_type_consumer>>();

  auto &consumer = container.template resolve<selected_type_consumer &>();

  ASSERT_EQ(consumer.processor.id(), 3);
}

TEST(index_test, selected_value_selector_injects_indexed_binding) {
  struct traits : dynamic_container_traits {
    using lookup_definition_type =
        lookups<associative<std::size_t, fixed_injection_processor>>;
  };

  container<traits> container;
  container.template register_type<scope<shared>,
                                   storage<fixed_injection_processor_impl<5>>,
                                   interfaces<fixed_injection_processor>>(
      dingo::key_value{std::size_t(1)});
  container.template register_type<scope<shared>,
                                   storage<selected_value_consumer>>();

  auto &consumer = container.template resolve<selected_value_consumer &>();

  ASSERT_EQ(consumer.processor.id(), 5);
}

TEST(index_test, annotated_interfaces_keep_independent_indexes) {
  using primary_processor =
      annotated<fixed_injection_processor &, annotated_index_primary_tag>;
  using replica_processor =
      annotated<fixed_injection_processor &, annotated_index_replica_tag>;

  struct traits : dynamic_container_traits {
    using lookup_definition_type = lookups<
        associative<std::size_t, annotated<fixed_injection_processor,
                                           annotated_index_primary_tag>>,
        associative<std::size_t, annotated<fixed_injection_processor,
                                           annotated_index_replica_tag>>>;
  };

  container<traits> container;
  container.template register_type<
      scope<shared>, storage<fixed_injection_processor_impl<11>>,
      interfaces<
          annotated<fixed_injection_processor, annotated_index_primary_tag>>>(
      dingo::key_value{std::size_t(1)});
  container.template register_type<
      scope<shared>, storage<fixed_injection_processor_impl<22>>,
      interfaces<
          annotated<fixed_injection_processor, annotated_index_replica_tag>>>(
      dingo::key_value{std::size_t(3)});

  primary_processor primary =
      container.template resolve<primary_processor>(std::size_t(1));
  replica_processor replica =
      container.template resolve<replica_processor>(std::size_t(3));

  fixed_injection_processor &primary_instance = primary;
  fixed_injection_processor &replica_instance = replica;

  ASSERT_EQ(primary_instance.id(), 11);
  ASSERT_EQ(replica_instance.id(), 22);
}

#ifdef DINGO_ENABLE_FUTURE_KEY_STRING_TESTS
TEST(index_test,
     DISABLED_constructor_injects_string_literal_key_into_string_index) {
  struct traits : dingo::dynamic_container_traits {
    using lookup_definition_type = dingo::lookups<
        dingo::associative<std::string, dingo::interfaces<string_processor>>>;
  };

  dingo::container<traits> container;
  container.template register_type<dingo::scope<dingo::shared>,
                                   dingo::storage<string_processor_impl<7>>,
                                   dingo::interfaces<string_processor>>(
      dingo::key_value{std::string{"json"}});

  auto consumer = container.template construct<string_literal_consumer>();

  ASSERT_EQ(consumer.processor.id(), 7);
}
#endif

} // namespace dingo
