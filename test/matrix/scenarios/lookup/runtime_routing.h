//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>
#include <dingo/runtime_container.h>

#include <cstddef>
#include <string>
#include <type_traits>

namespace dingo::matrix {

struct routing_processor {
  virtual ~routing_processor() = default;
  virtual int processor_id() const = 0;
};

struct routing_animal {
  virtual ~routing_animal() = default;
  virtual int animal_id() const = 0;
};

struct routing_source {
  virtual ~routing_source() = default;
  virtual int source_id() const = 0;
};

template <int Id> struct routing_processor_implementation : routing_processor {
  int processor_id() const override { return Id; }
};

template <int Id> struct routing_animal_implementation : routing_animal {
  int animal_id() const override { return Id; }
};

template <int Id> struct routing_source_implementation : routing_source {
  int source_id() const override { return Id; }
};

struct routing_processor_animal : routing_processor, routing_animal {
  int processor_id() const override { return 9; }
  int animal_id() const override { return 4; }
};

struct routing_pipeline {
  routing_pipeline(
      dingo::dependency<routing_source &, dingo::key_type<int, 1>> init_source,
      dingo::dependency<routing_processor &, dingo::key_type<int, 0>>
          init_processor)
      : source(init_source), processor(init_processor) {}

  routing_source &source;
  routing_processor &processor;
};

struct dual_interface_routing_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<int, routing_processor>,
                     dingo::associative<int, routing_animal>>;
};

struct pipeline_routing_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<int, routing_source>,
                     dingo::associative<int, routing_processor>>;
};

struct multiple_key_routing_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<int, routing_processor>,
                     dingo::associative<std::string, routing_processor>>;
};

struct indexed_and_normal_routing_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<int, routing_processor>>;
};

template <typename Container> struct runtime_lookup_container_family;

template <typename... Params>
struct runtime_lookup_container_family<dingo::container<Params...>> {
  template <typename Traits> using type = dingo::container<Traits>;
};

template <typename Traits, typename Allocator, typename Parent>
struct runtime_lookup_container_family<
    dingo::runtime_container<Traits, Allocator, Parent>> {
  template <typename NewTraits>
  using type = dingo::runtime_container<NewTraits>;
};

template <typename Family> void exercise_dual_interface_routing() {
  using container_type =
      typename Family::template type<dual_interface_routing_traits>;

  container_type separate_interfaces;
  separate_interfaces.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<routing_processor_implementation<2>>,
      dingo::interfaces<routing_processor>>(dingo::key_value<int>{2});
  separate_interfaces
      .template register_type<dingo::scope<dingo::shared>,
                              dingo::storage<routing_animal_implementation<15>>,
                              dingo::interfaces<routing_animal>>(
          dingo::key_value<int>{15});
  ASSERT_EQ(separate_interfaces.template resolve<routing_processor &>(2)
                .processor_id(),
            2);
  ASSERT_EQ(
      separate_interfaces.template resolve<routing_animal &>(15).animal_id(),
      15);

  container_type broadcast;
  broadcast.template register_type<
      dingo::scope<dingo::shared>, dingo::storage<routing_processor_animal>,
      dingo::interfaces<routing_processor, routing_animal>>(
      dingo::key_value<int>{7});
  ASSERT_EQ(broadcast.template resolve<routing_processor &>(7).processor_id(),
            9);
  ASSERT_EQ(broadcast.template resolve<routing_animal &>(7).animal_id(), 4);

  container_type qualified;
  qualified.template register_type<
      dingo::scope<dingo::shared>, dingo::storage<routing_processor_animal>,
      dingo::interfaces<routing_processor, routing_animal>>(
      dingo::key_value<int, routing_processor>{7},
      dingo::key_value<int, routing_animal>{9});
  ASSERT_EQ(qualified.template resolve<routing_processor &>(7).processor_id(),
            9);
  ASSERT_EQ(qualified.template resolve<routing_animal &>(9).animal_id(), 4);
  ASSERT_THROW((qualified.template resolve<routing_processor &>(9)),
               dingo::type_not_found_exception);
  ASSERT_THROW((qualified.template resolve<routing_animal &>(7)),
               dingo::type_not_found_exception);

  container_type overridden;
  overridden.template register_type<
      dingo::scope<dingo::shared>, dingo::storage<routing_processor_animal>,
      dingo::interfaces<routing_processor, routing_animal>>(
      dingo::key_value<int>{7}, dingo::key_value<int, routing_animal>{9});
  ASSERT_EQ(overridden.template resolve<routing_processor &>(7).processor_id(),
            9);
  ASSERT_EQ(overridden.template resolve<routing_animal &>(9).animal_id(), 4);
  ASSERT_THROW((overridden.template resolve<routing_animal &>(7)),
               dingo::type_not_found_exception);
}

template <typename Family> void exercise_pipeline_routing() {
  using container_type =
      typename Family::template type<pipeline_routing_traits>;

  container_type container;
  container
      .template register_type<dingo::scope<dingo::shared>,
                              dingo::storage<routing_source_implementation<1>>,
                              dingo::interfaces<routing_source>>(
          dingo::key_value<int>{0});
  container
      .template register_type<dingo::scope<dingo::shared>,
                              dingo::storage<routing_source_implementation<2>>,
                              dingo::interfaces<routing_source>>(
          dingo::key_value<int>{1});
  container.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<routing_processor_implementation<10>>,
      dingo::interfaces<routing_processor>>(dingo::key_value<int>{0});
  container.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<routing_processor_implementation<20>>,
      dingo::interfaces<routing_processor>>(dingo::key_value<int>{1});

  auto pipeline = container.template construct<routing_pipeline>();
  ASSERT_EQ(pipeline.source.source_id(), 2);
  ASSERT_EQ(pipeline.processor.processor_id(), 10);
}

template <typename Family> void exercise_multiple_key_routing() {
  using container_type =
      typename Family::template type<multiple_key_routing_traits>;

  container_type aliases;
  aliases.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<routing_processor_implementation<1>>,
      dingo::interfaces<routing_processor>>(
      dingo::key_value<int>{1},
      dingo::key_value<std::string>{std::string{"first"}});
  aliases.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<routing_processor_implementation<2>>,
      dingo::interfaces<routing_processor>>(
      dingo::key_value<int>{2},
      dingo::key_value<std::string>{std::string{"second"}});
  auto &first_by_id = aliases.template resolve<routing_processor &>(1);
  auto &first_by_name =
      aliases.template resolve<routing_processor &>(std::string{"first"});
  auto &second_by_id = aliases.template resolve<routing_processor &>(2);
  auto &second_by_name =
      aliases.template resolve<routing_processor &>(std::string{"second"});
  ASSERT_EQ(&first_by_id, &first_by_name);
  ASSERT_EQ(first_by_id.processor_id(), 1);
  ASSERT_EQ(&second_by_id, &second_by_name);
  ASSERT_EQ(second_by_id.processor_id(), 2);

  container_type rollback;
  rollback.template register_type<
      dingo::scope<dingo::shared>,
      dingo::storage<routing_processor_implementation<1>>,
      dingo::interfaces<routing_processor>>(
      dingo::key_value<int>{1},
      dingo::key_value<std::string>{std::string{"main"}});
  auto &original = rollback.template resolve<routing_processor &>(1);
  ASSERT_THROW((rollback.template register_type<
                   dingo::scope<dingo::shared>,
                   dingo::storage<routing_processor_implementation<2>>,
                   dingo::interfaces<routing_processor>>(
                   dingo::key_value<int>{2},
                   dingo::key_value<std::string>{std::string{"main"}})),
               dingo::lookup_already_registered_exception);
  ASSERT_THROW((rollback.template resolve<routing_processor &>(2)),
               dingo::type_not_found_exception);
  ASSERT_EQ(&rollback.template resolve<routing_processor &>(1), &original);
  ASSERT_EQ(
      &rollback.template resolve<routing_processor &>(std::string{"main"}),
      &original);
}

template <typename Family> void exercise_indexed_and_normal_routing() {
  using container_type =
      typename Family::template type<indexed_and_normal_routing_traits>;

  container_type container;
  container.template register_type<
      dingo::scope<dingo::shared>, dingo::storage<routing_processor_animal>,
      dingo::interfaces<routing_processor, routing_animal>>(
      dingo::key_value<int>{7});
  ASSERT_EQ(container.template resolve<routing_processor &>(7).processor_id(),
            9);
  ASSERT_EQ(container.template resolve<routing_animal &>().animal_id(), 4);
}

struct runtime_lookup_routing_scenario {
  template <typename Container> static void run(Container &) {
    using family = runtime_lookup_container_family<
        std::remove_cv_t<std::remove_reference_t<Container>>>;
    exercise_dual_interface_routing<family>();
    exercise_pipeline_routing<family>();
    exercise_multiple_key_routing<family>();
    exercise_indexed_and_normal_routing<family>();
  }
};

} // namespace dingo::matrix
