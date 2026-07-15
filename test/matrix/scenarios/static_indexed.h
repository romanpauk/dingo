//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/common/assertions.h"

#include <dingo/container.h>

#include <cstddef>
#include <vector>

namespace dingo::matrix {

struct static_indexed_processor {
  virtual ~static_indexed_processor() = default;
  virtual int id() const = 0;
};

template <int Id>
struct static_indexed_processor_impl : static_indexed_processor {
  int id() const override { return Id; }
};

template <auto Key> struct static_indexed_consumer {
  explicit static_indexed_consumer(
      dingo::dependency<static_indexed_processor &,
                        dingo::key_type<std::size_t, Key>>
          selected)
      : processor(selected) {}

  static_indexed_processor &processor;
};

struct static_indexed_one_traits : dingo::static_container_traits<> {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, static_indexed_processor>>;
};

struct static_indexed_many_traits : dingo::static_container_traits<> {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<std::size_t, static_indexed_processor, dingo::many>>;
};

inline void assert_static_indexed_singular_resolution() {
  using source = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>,
                  dingo::storage<static_indexed_processor_impl<7>>,
                  dingo::interfaces<static_indexed_processor>,
                  dingo::key_type<std::size_t, 0>>,
      dingo::bind<dingo::scope<dingo::shared>,
                  dingo::storage<static_indexed_processor_impl<7>>,
                  dingo::interfaces<static_indexed_processor>,
                  dingo::key_type<std::size_t, 1>>,
      dingo::bind<
          dingo::scope<dingo::unique>,
          dingo::storage<static_indexed_consumer<1>>,
          dingo::dependencies<dingo::dependency<
              static_indexed_processor &, dingo::key_type<std::size_t, 1>>>>>;

  dingo::static_container<source, static_indexed_one_traits> container;

  auto &first = container.template resolve<dingo::dependency<
      static_indexed_processor &, dingo::key_type<std::size_t, 0>>>();
  auto &first_again = container.template resolve<dingo::dependency<
      static_indexed_processor &, dingo::key_type<std::size_t, 0>>>();
  auto &second = container.template resolve<dingo::dependency<
      static_indexed_processor &, dingo::key_type<std::size_t, 1>>>();
  auto consumer = container.template resolve<static_indexed_consumer<1>>();

  ASSERT_EQ(&first, &first_again);
  ASSERT_NE(&first, &second);
  ASSERT_EQ(first.id(), 7);
  ASSERT_EQ(second.id(), 7);
  ASSERT_EQ(consumer.processor.id(), 7);
}

inline void assert_static_indexed_many_resolution() {
  using source = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>,
                  dingo::storage<static_indexed_processor_impl<0>>,
                  dingo::interfaces<static_indexed_processor>,
                  dingo::key_type<std::size_t, 7>>,
      dingo::bind<dingo::scope<dingo::shared>,
                  dingo::storage<static_indexed_processor_impl<1>>,
                  dingo::interfaces<static_indexed_processor>,
                  dingo::key_type<std::size_t, 7>>,
      dingo::bind<dingo::scope<dingo::shared>,
                  dingo::storage<static_indexed_processor_impl<2>>,
                  dingo::interfaces<static_indexed_processor>,
                  dingo::key_type<std::size_t, 8>>>;

  dingo::static_container<source, static_indexed_many_traits> container;
  auto processors =
      container.template resolve<std::vector<static_indexed_processor *>>(
          dingo::key_type<std::size_t, 7>{});
  auto constructed = container.template construct_collection<
      std::vector<static_indexed_processor *>>(
      dingo::key_type<std::size_t, 7>{});

  ASSERT_EQ(
      (container
           .template count_collection<std::vector<static_indexed_processor *>,
                                      dingo::key_type<std::size_t, 7>>()),
      2U);
  ASSERT_EQ(processors.size(), 2U);
  ASSERT_EQ(processors[0]->id(), 0);
  ASSERT_EQ(processors[1]->id(), 1);
  ASSERT_EQ(constructed.size(), 2U);
  ASSERT_EQ(constructed[0]->id(), 0);
  ASSERT_EQ(constructed[1]->id(), 1);
}

inline void assert_static_indexed_parent_selection() {
  using parent_source = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>,
                  dingo::storage<static_indexed_processor_impl<1>>,
                  dingo::interfaces<static_indexed_processor>,
                  dingo::key_type<std::size_t, 7>>,
      dingo::bind<dingo::scope<dingo::shared>,
                  dingo::storage<static_indexed_processor_impl<2>>,
                  dingo::interfaces<static_indexed_processor>,
                  dingo::key_type<std::size_t, 7>>>;
  using child_source = dingo::bindings<>;
  using overriding_child_source = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>,
                  dingo::storage<static_indexed_processor_impl<3>>,
                  dingo::interfaces<static_indexed_processor>,
                  dingo::key_type<std::size_t, 7>>>;

  dingo::static_container<parent_source, static_indexed_many_traits> parent;
  dingo::static_container<child_source, decltype(parent)> child(&parent);
  dingo::static_container<overriding_child_source, decltype(parent)> override(
      &parent);

  auto inherited =
      child.template resolve<std::vector<static_indexed_processor *>>(
          dingo::key_type<std::size_t, 7>{});
  auto local =
      override.template resolve<std::vector<static_indexed_processor *>>(
          dingo::key_type<std::size_t, 7>{});

  ASSERT_EQ(inherited.size(), 2U);
  ASSERT_EQ(inherited[0]->id(), 1);
  ASSERT_EQ(inherited[1]->id(), 2);
  ASSERT_EQ(local.size(), 1U);
  ASSERT_EQ(local[0]->id(), 3);
}

inline void exercise_static_indexed_regressions() {
  assert_static_indexed_singular_resolution();
  assert_static_indexed_many_resolution();
  assert_static_indexed_parent_selection();
}

} // namespace dingo::matrix
