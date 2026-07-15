//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/fixtures/values.h"

#include <dingo/container.h>
#include <dingo/runtime_container.h>

#include <memory>
#include <type_traits>
#include <vector>

namespace dingo::matrix {

struct lookup_route_key {};

template <typename Cardinality> struct unkeyed_lookup_route_traits;

template <>
struct unkeyed_lookup_route_traits<dingo::one>
    : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::single<element_interface>>;
};

template <>
struct unkeyed_lookup_route_traits<dingo::many>
    : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::collection<element_interface>>;
};

template <typename Cardinality>
struct typed_lookup_route_traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::typed<lookup_route_key, element_interface, Cardinality>>;
};

template <typename Cardinality>
struct key_value_lookup_route_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<int, element_interface, Cardinality>>;
};

struct unkeyed_lookup_route {
  template <typename Cardinality>
  using traits = unkeyed_lookup_route_traits<Cardinality>;

  template <typename Implementation, typename Container>
  static void register_binding(Container &container) {
    container.template register_type<dingo::scope<dingo::shared>,
                                     dingo::storage<Implementation>,
                                     dingo::interfaces<element_interface>>();
  }

  template <typename Type, typename Container>
  static decltype(auto) resolve(Container &container) {
    return container.template resolve<Type>();
  }
};

struct typed_lookup_route {
  template <typename Cardinality>
  using traits = typed_lookup_route_traits<Cardinality>;

  template <typename Implementation, typename Container>
  static void register_binding(Container &container) {
    container.template register_type<dingo::scope<dingo::shared>,
                                     dingo::storage<Implementation>,
                                     dingo::interfaces<element_interface>,
                                     dingo::key_type<lookup_route_key>>();
  }

  template <typename Type, typename Container>
  static decltype(auto) resolve(Container &container) {
    return container.template resolve<Type>(
        dingo::key_type<lookup_route_key>{});
  }
};

struct key_value_lookup_route {
  template <typename Cardinality>
  using traits = key_value_lookup_route_traits<Cardinality>;

  template <typename Implementation, typename Container>
  static void register_binding(Container &container) {
    container.template register_type<dingo::scope<dingo::shared>,
                                     dingo::storage<Implementation>,
                                     dingo::interfaces<element_interface>>(
        dingo::key_value{7});
  }

  template <typename Type, typename Container>
  static decltype(auto) resolve(Container &container) {
    return container.template resolve<Type>(7);
  }
};

template <int Id> struct lookup_route_element : element_interface {
  lookup_route_element() { ++constructions; }

  int id() const override { return Id; }

  inline static int constructions = 0;
};

template <template <typename...> typename Container, typename Route>
void exercise_single_lookup_route() {
  using traits = typename Route::template traits<dingo::one>;
  using first_type = lookup_route_element<1>;
  using second_type = lookup_route_element<2>;

  first_type::constructions = 0;
  Container<traits> container;
  ASSERT_THROW((Route::template resolve<element_interface &>(container)),
               type_not_found_exception);

  Route::template register_binding<first_type>(container);
  auto &before = Route::template resolve<element_interface &>(container);
  auto &again = Route::template resolve<element_interface &>(container);
  ASSERT_EQ(std::addressof(before), std::addressof(again));
  ASSERT_EQ(before.id(), 1);
  ASSERT_EQ(first_type::constructions, 1);
  ASSERT_THROW(
      (Route::template resolve<std::vector<element_interface *>>(container)),
      type_not_found_exception);

  ASSERT_THROW((Route::template register_binding<first_type>(container)),
               lookup_already_registered_exception);
  ASSERT_THROW((Route::template register_binding<second_type>(container)),
               lookup_already_registered_exception);
  auto &after = Route::template resolve<element_interface &>(container);
  ASSERT_EQ(std::addressof(before), std::addressof(after));
  ASSERT_EQ(first_type::constructions, 1);

  Container<traits> parent;
  Container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  Route::template register_binding<first_type>(parent);
  ASSERT_EQ(Route::template resolve<element_interface &>(child).id(), 1);
  Route::template register_binding<second_type>(child);
  ASSERT_EQ(Route::template resolve<element_interface &>(child).id(), 2);
}

template <template <typename...> typename Container, typename Route>
void exercise_collection_lookup_route() {
  using traits = typename Route::template traits<dingo::many>;
  using first_type = lookup_route_element<1>;
  using second_type = lookup_route_element<2>;
  using third_type = lookup_route_element<3>;

  Container<traits> empty;
  ASSERT_TRUE(
      Route::template resolve<std::vector<element_interface *>>(empty).empty());
  ASSERT_THROW((Route::template resolve<element_interface &>(empty)),
               type_not_found_exception);

  Container<traits> exact;
  Route::template register_binding<first_type>(exact);
  auto exact_values =
      Route::template resolve<std::vector<element_interface *>>(exact);
  auto &exact_reference = Route::template resolve<element_interface &>(exact);
  auto *exact_pointer = Route::template resolve<element_interface *>(exact);
  ASSERT_EQ(exact_values.size(), 1u);
  ASSERT_EQ(exact_values[0], std::addressof(exact_reference));
  ASSERT_EQ(exact_pointer, std::addressof(exact_reference));

  Container<traits> duplicates;
  Route::template register_binding<first_type>(duplicates);
  Route::template register_binding<first_type>(duplicates);
  auto duplicate_values =
      Route::template resolve<std::vector<element_interface *>>(duplicates);
  ASSERT_EQ(duplicate_values.size(), 2u);
  ASSERT_EQ(duplicate_values[0]->id(), 1);
  ASSERT_EQ(duplicate_values[1]->id(), 1);
  ASSERT_NE(duplicate_values[0], duplicate_values[1]);
  ASSERT_THROW((Route::template resolve<element_interface &>(duplicates)),
               type_ambiguous_exception);

  Container<traits> parent;
  Container<traits, std::allocator<char>, decltype(parent)> child(&parent);
  Route::template register_binding<first_type>(parent);
  Route::template register_binding<second_type>(parent);

  auto inherited =
      Route::template resolve<std::vector<element_interface *>>(child);
  ASSERT_EQ(inherited.size(), 2u);
  ASSERT_EQ(inherited[0]->id(), 1);
  ASSERT_EQ(inherited[1]->id(), 2);
  ASSERT_THROW((Route::template resolve<element_interface &>(child)),
               type_ambiguous_exception);

  Route::template register_binding<second_type>(child);
  auto local = Route::template resolve<std::vector<element_interface *>>(child);
  ASSERT_EQ(local.size(), 1u);
  ASSERT_EQ(local[0]->id(), 2);
  ASSERT_EQ(Route::template resolve<element_interface &>(child).id(), 2);

  Route::template register_binding<third_type>(child);
  local = Route::template resolve<std::vector<element_interface *>>(child);
  ASSERT_EQ(local.size(), 2u);
  ASSERT_EQ(local[0]->id(), 2);
  ASSERT_EQ(local[1]->id(), 3);
  ASSERT_THROW((Route::template resolve<element_interface &>(child)),
               type_ambiguous_exception);
}

template <typename Container> struct lookup_route_dispatch;

template <typename... Params>
struct lookup_route_dispatch<dingo::container<Params...>> {
  template <typename Route> static void run() {
    exercise_single_lookup_route<dingo::container, Route>();
    exercise_collection_lookup_route<dingo::container, Route>();
  }
};

template <typename Traits, typename Allocator, typename Parent>
struct lookup_route_dispatch<
    dingo::runtime_container<Traits, Allocator, Parent>> {
  template <typename Route> static void run() {
    exercise_single_lookup_route<dingo::runtime_container, Route>();
    exercise_collection_lookup_route<dingo::runtime_container, Route>();
  }
};

template <typename Route> struct lookup_route_scenario {
  template <typename Container> static void run(Container &) {
    using dispatch = lookup_route_dispatch<
        std::remove_cv_t<std::remove_reference_t<Container>>>;
    dispatch::template run<Route>();
  }
};

using unkeyed_lookup_route_scenario =
    lookup_route_scenario<unkeyed_lookup_route>;
using typed_lookup_route_scenario = lookup_route_scenario<typed_lookup_route>;
using key_value_lookup_route_scenario =
    lookup_route_scenario<key_value_lookup_route>;

} // namespace dingo::matrix
