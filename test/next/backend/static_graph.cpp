//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The compile-time analysis of the static backend: the dependency graph its
// bindings declare and the cycles in it, the bindings of a key that a domain
// of one rejects, the bindings a request names and serves, and what a static
// container keeps for its requests. spec: graph_rules

#include <dingo/lookup/lookup.h>
#include <dingo/registration/constructor.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>
#include <dingo/type/annotated.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace {

using dingo::scope;
using dingo::storage;

template <typename Bind>
using binding_of = typename dingo::core::bind_registration<Bind>::type::binding;

template <typename... Binds>
using graph_of = dingo::core::detail::dependency_graph<binding_of<Binds>...>;

// The edges of a graph; the edge to `count` is of the dependencies no static
// binding answers, which the graph drops.
template <typename Graph> constexpr typename Graph::edges edges_of() {
  return Graph::make(std::make_index_sequence<Graph::count>());
}

template <typename Graph> constexpr std::size_t edge_count() {
  constexpr typename Graph::edges edges = edges_of<Graph>();
  std::size_t count = 0;
  for (std::size_t index = 0; index < Graph::count * Graph::count; ++index) {
    count += edges.from_to[index] ? 1 : 0;
  }
  return count;
}

template <typename Graph>
constexpr bool has_edge(std::size_t from, std::size_t to) {
  return edges_of<Graph>().from_to[from * Graph::count + to];
}

template <typename Graph> constexpr bool declares_any() {
  return Graph::any(std::make_index_sequence<Graph::count>());
}

template <typename Lookups>
using table_of = dingo::core::lookup_table_of<
    typename dingo::core::lookup_list<Lookups>::type>;

using default_table = table_of<std::tuple<>>;

template <typename Table, typename... Binds> constexpr bool has_duplicates() {
  return dingo::core::detail::duplicates<Table::value, binding_of<Binds>...>::
      any(std::index_sequence_for<Binds...>());
}

template <typename Table, typename... Binds>
constexpr std::size_t rejected_binding() {
  return dingo::core::detail::duplicates<Table::value,
                                         binding_of<Binds>...>::rejected();
}

template <typename Traits, typename... Binds>
using backend_of = typename dingo::static_container<dingo::bindings<Binds...>,
                                                    Traits>::static_source_type;

using dingo::core::no_key;

struct config {};
struct service_interface {
  virtual ~service_interface() = default;
};
struct service : service_interface {
  explicit service(config &) {}
};
struct controller {
  explicit controller(service_interface &) {}
};

TEST(next_static_graph,
     declared_dependencies_are_edges_to_the_bindings_they_name) {
  using graph = graph_of<dingo::bind<scope<dingo::shared>, storage<config>>,
                         dingo::bind<scope<dingo::shared>, storage<service>,
                                     dingo::interfaces<service_interface>,
                                     dingo::dependencies<config &>>,
                         dingo::bind<scope<dingo::unique>, storage<controller>,
                                     dingo::dependencies<service_interface &>>>;
  static_assert(declares_any<graph>());
  static_assert(has_edge<graph>(1, 0));
  static_assert(has_edge<graph>(2, 1));
  static_assert(edge_count<graph>() == 2);
  static_assert(graph::find().length == 0);
  SUCCEED();
}

struct config_tag {};
struct service_tag {};
struct annotated_service {
  explicit annotated_service(dingo::annotated<config &, config_tag>) {}
};

TEST(next_static_graph,
     an_annotated_dependency_is_an_edge_to_the_annotated_interface) {
  using graph = graph_of<
      dingo::bind<scope<dingo::shared>, storage<config>>,
      dingo::bind<scope<dingo::shared>, storage<config>,
                  dingo::interfaces<dingo::annotated<config, config_tag>>>,
      dingo::bind<
          scope<dingo::unique>, storage<annotated_service>,
          dingo::interfaces<dingo::annotated<annotated_service, service_tag>>,
          dingo::dependencies<dingo::annotated<config &, config_tag>>>>;
  static_assert(has_edge<graph>(2, 1));
  static_assert(!has_edge<graph>(2, 0));
  static_assert(edge_count<graph>() == 1);
  SUCCEED();
}

struct first_key : std::integral_constant<int, 0> {};
struct second_key : std::integral_constant<int, 1> {};

struct keyed_service {
  explicit keyed_service(
      dingo::dependency<config &, dingo::key_type<first_key>>) {}
};

TEST(next_static_graph,
     a_keyed_dependency_is_an_edge_to_the_binding_of_its_key) {
  // A dependency without a key is its type.
  static_assert(std::is_same_v<dingo::dependency<config &>, config &>);
  using graph =
      graph_of<dingo::bind<scope<dingo::shared>, storage<config>,
                           dingo::key_type<second_key>>,
               dingo::bind<scope<dingo::shared>, storage<config>,
                           dingo::key_type<first_key>>,
               dingo::bind<scope<dingo::unique>, storage<keyed_service>,
                           dingo::dependencies<dingo::dependency<
                               config &, dingo::key_type<first_key>>>>>;
  static_assert(has_edge<graph>(2, 1));
  static_assert(!has_edge<graph>(2, 0));
  static_assert(edge_count<graph>() == 1);
  SUCCEED();
}

struct a {};
struct b {};
struct owner {};
struct runtime_only {};

TEST(next_static_graph,
     a_cycle_of_declared_dependencies_is_found_with_its_path) {
  using bind_a =
      dingo::bind<scope<dingo::shared>, storage<a>, dingo::dependencies<b &>>;
  using bind_b =
      dingo::bind<scope<dingo::shared>, storage<b>, dingo::dependencies<a &>>;
  // The bindings themselves are valid; the backend of both rejects them.
  static_assert(dingo::core::registers<binding_of<bind_a>>());
  static_assert(dingo::core::registers<binding_of<bind_b>>());
  using graph = graph_of<bind_a, bind_b>;
  constexpr graph::cycle found = graph::find();
  static_assert(found.length == 3);
  static_assert(found.nodes[0] == 0 && found.nodes[1] == 1 &&
                found.nodes[2] == 0);
  SUCCEED();
}

TEST(next_static_graph, a_cycle_of_shared_cyclical_bindings_is_legal) {
  using graph = graph_of<dingo::bind<scope<dingo::shared_cyclical>, storage<a>,
                                     dingo::dependencies<b &>>,
                         dingo::bind<scope<dingo::shared_cyclical>, storage<b>,
                                     dingo::dependencies<a &>>,
                         dingo::bind<scope<dingo::shared>, storage<owner>,
                                     dingo::dependencies<a &>>>;
  static_assert(has_edge<graph>(0, 1));
  static_assert(has_edge<graph>(1, 0));
  static_assert(has_edge<graph>(2, 0));
  static_assert(graph::find().length == 0);
  SUCCEED();
}

TEST(next_static_graph,
     a_cycle_through_a_binding_that_is_not_shared_cyclical_is_found) {
  using graph = graph_of<
      dingo::bind<scope<dingo::shared_cyclical>, storage<a>,
                  dingo::dependencies<b &>>,
      dingo::bind<scope<dingo::shared>, storage<b>, dingo::dependencies<a &>>>;
  constexpr graph::cycle found = graph::find();
  // The path starts at the binding that makes the cycle illegal.
  static_assert(found.length == 3);
  static_assert(found.nodes[0] == 1 && found.nodes[1] == 0 &&
                found.nodes[2] == 1);
  SUCCEED();
}

TEST(next_static_graph,
     a_dependency_no_static_binding_answers_adds_no_edge_and_hides_no_cycle) {
  using graph = graph_of<
      dingo::bind<scope<dingo::shared>, storage<a>,
                  dingo::dependencies<b &, runtime_only &>>,
      dingo::bind<scope<dingo::shared>, storage<b>, dingo::dependencies<a &>>>;
  static_assert(edge_count<graph>() == 2);
  constexpr graph::cycle found = graph::find();
  static_assert(found.length == 3);
  static_assert(found.nodes[0] == 0 && found.nodes[1] == 1 &&
                found.nodes[2] == 0);
  SUCCEED();
}

struct plain_service {
  plain_service(config &, owner &) {}
};

struct declared_service {
  DINGO_CONSTRUCTOR(declared_service(config &)) {}
};

TEST(next_static_graph,
     a_detected_constructor_declares_no_dependencies_and_a_declared_one_does) {
  using plain =
      binding_of<dingo::bind<scope<dingo::unique>, storage<plain_service>>>;
  using zero = binding_of<dingo::bind<scope<dingo::unique>, storage<config>>>;
  using declared =
      binding_of<dingo::bind<scope<dingo::unique>, storage<declared_service>>>;
  static_assert(std::is_void_v<typename dingo::core::declared_arguments<
                    typename plain::factory_type, plain_service>::type>);
  static_assert(std::is_void_v<typename dingo::core::declared_arguments<
                    typename zero::factory_type, config>::type>);
  static_assert(std::is_same_v<
                typename dingo::core::declared_arguments<
                    typename declared::factory_type, declared_service>::type,
                dingo::core::type_list<config &>>);

  using detected =
      graph_of<dingo::bind<scope<dingo::shared>, storage<config>>,
               dingo::bind<scope<dingo::shared>, storage<owner>>,
               dingo::bind<scope<dingo::unique>, storage<plain_service>>>;
  static_assert(!declares_any<detected>());
  static_assert(edge_count<detected>() == 0);
  using annotated =
      graph_of<dingo::bind<scope<dingo::shared>, storage<config>>,
               dingo::bind<scope<dingo::unique>, storage<declared_service>>>;
  static_assert(declares_any<annotated>());
  static_assert(has_edge<annotated>(1, 0));
  SUCCEED();
}

struct first_impl : service_interface {};
struct second_impl : service_interface {};

struct many_traits : dingo::static_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::collection<service_interface>,
                     dingo::typed<first_key, service_interface, dingo::many>>;
};

using many_table = table_of<many_traits::lookup_definition_type>;

// spec: behavior_changes.duplicates_rejected_in_both_domains
TEST(next_static_graph, a_second_binding_of_a_key_is_a_duplicate_unless_many) {
  using first = dingo::bind<scope<dingo::shared>, storage<first_impl>,
                            dingo::interfaces<service_interface>>;
  using second = dingo::bind<scope<dingo::shared>, storage<second_impl>,
                             dingo::interfaces<service_interface>>;
  using first_tagged = dingo::bind<scope<dingo::shared>, storage<first_impl>,
                                   dingo::interfaces<service_interface>,
                                   dingo::key_type<first_key>>;
  using second_tagged = dingo::bind<scope<dingo::shared>, storage<second_impl>,
                                    dingo::interfaces<service_interface>,
                                    dingo::key_type<first_key>>;
  using other_tagged = dingo::bind<scope<dingo::shared>, storage<second_impl>,
                                   dingo::interfaces<service_interface>,
                                   dingo::key_type<second_key>>;
  using unrelated = dingo::bind<scope<dingo::shared>, storage<config>>;

  static_assert(has_duplicates<default_table, first, second>());
  static_assert(has_duplicates<default_table, first, first>());
  static_assert(has_duplicates<default_table, first_tagged, second_tagged>());
  static_assert(rejected_binding<default_table, unrelated, first, second>() ==
                2);
  static_assert(!has_duplicates<default_table, first, first_tagged>());
  static_assert(!has_duplicates<default_table, first_tagged, other_tagged>());
  static_assert(!has_duplicates<default_table, first, unrelated>());

  static_assert(!has_duplicates<many_table, first, second>());
  static_assert(!has_duplicates<many_table, first, first>());
  static_assert(!has_duplicates<many_table, first_tagged, second_tagged>());
  // A domain of one still rejects its duplicates beside a domain of many.
  static_assert(has_duplicates<many_table, other_tagged, other_tagged>());
  SUCCEED();
}

TEST(next_static_graph, a_request_names_every_binding_of_its_key) {
  using backend =
      backend_of<many_traits,
                 dingo::bind<scope<dingo::shared>, storage<first_impl>,
                             dingo::interfaces<service_interface>,
                             dingo::key_type<first_key>>,
                 dingo::bind<scope<dingo::shared>, storage<second_impl>,
                             dingo::interfaces<service_interface>,
                             dingo::key_type<first_key>>,
                 dingo::bind<scope<dingo::shared>, storage<second_impl>,
                             dingo::interfaces<service_interface>,
                             dingo::key_type<second_key>>>;
  using first = dingo::core::key_of_t<dingo::key_type<first_key>>;
  using second = dingo::core::key_of_t<dingo::key_type<second_key>>;
  static_assert(backend::fixed_count<service_interface &, no_key>() == 0);
  static_assert(backend::fixed_count<service_interface &, first>() == 2);
  static_assert(backend::fixed_count<service_interface &, second>() == 1);
  static_assert(!backend::serves_named<service_interface &, first>());
  static_assert(backend::serves_named<service_interface &, second>());

  using duplicated = dingo::bind<scope<dingo::shared>, storage<first_impl>,
                                 dingo::interfaces<service_interface>>;
  using found = dingo::bind<scope<dingo::shared>, storage<config>>;
  using twice = backend_of<many_traits, duplicated, duplicated, found>;
  static_assert(twice::fixed_count<service_interface &, no_key>() == 2);
  static_assert(twice::fixed_count<config &, no_key>() == 1);
  static_assert(twice::fixed_count<owner &, no_key>() == 0);
  static_assert(
      dingo::core::detail::find_binding<no_key, binding_of<duplicated>,
                                        binding_of<duplicated>,
                                        binding_of<found>>(
          dingo::core::request_traits<service_interface &,
                                      no_key>::descriptor) ==
      dingo::core::detail::ambiguous);
  static_assert(
      dingo::core::detail::find_binding<no_key, binding_of<duplicated>,
                                        binding_of<duplicated>,
                                        binding_of<found>>(
          dingo::core::request_traits<config &, no_key>::descriptor) == 2);
  static_assert(
      dingo::core::detail::find_binding<no_key, binding_of<duplicated>,
                                        binding_of<duplicated>,
                                        binding_of<found>>(
          dingo::core::request_traits<owner &, no_key>::descriptor) ==
      dingo::core::detail::not_found);
  SUCCEED();
}

struct payload {};

TEST(next_static_graph, a_binding_serves_the_requests_its_route_reaches) {
  using wrapped = backend_of<
      dingo::static_container_traits,
      dingo::bind<scope<dingo::shared>, storage<std::shared_ptr<payload>>>>;
  static_assert(wrapped::serves_named<payload &, no_key>());
  static_assert(wrapped::serves_named<const payload &, no_key>());
  static_assert(wrapped::serves_named<payload *, no_key>());
  static_assert(wrapped::serves_named<const payload *, no_key>());
  static_assert(wrapped::serves_named<payload, no_key>());
  // spec: behavior_changes.borrowing_never_moves
  static_assert(!wrapped::serves_named<payload &&, no_key>());

  using converted = backend_of<
      dingo::static_container_traits,
      dingo::bind<scope<dingo::shared>, storage<std::shared_ptr<service>>,
                  dingo::interfaces<service_interface>>>;
  static_assert(converted::serves_named<service_interface *, no_key>());
  static_assert(converted::serves_named<service_interface &, no_key>());

  using array =
      backend_of<dingo::static_container_traits,
                 dingo::bind<scope<dingo::shared>, storage<payload[2]>>>;
  static_assert(array::serves_named<payload *, no_key>());
  static_assert(!array::serves_named<payload &, no_key>());
  SUCCEED();
}

TEST(next_static_graph, a_binding_keeps_objects_only_when_a_route_derives_one) {
  static_assert(
      !binding_of<dingo::bind<scope<dingo::shared>, storage<payload>>>::keeps);
  static_assert(
      !binding_of<dingo::bind<scope<dingo::shared>,
                              storage<std::shared_ptr<payload>>>>::keeps);
  static_assert(
      !binding_of<dingo::bind<scope<dingo::unique>, storage<payload>>>::keeps);
  // spec: behavior_changes.interface_handles_are_retained
  using converted = binding_of<
      dingo::bind<scope<dingo::shared>, storage<std::shared_ptr<service>>,
                  dingo::interfaces<service_interface>>>;
  static_assert(converted::keeps);
  static_assert(std::is_same_v<typename converted::context_type,
                               dingo::core::kept_objects>);
  SUCCEED();
}

struct destructible {
  ~destructible() {}
};

template <typename Backend>
using log_of =
    std::remove_reference_t<decltype(std::declval<Backend &>().transactions())>;

TEST(next_static_graph,
     a_static_container_logs_only_the_slots_that_construct_an_object) {
  using consuming = backend_of<
      dingo::static_container_traits,
      dingo::bind<scope<dingo::unique>, storage<destructible>>,
      dingo::bind<scope<dingo::unique>, storage<std::shared_ptr<payload>>>>;
  static_assert(
      std::is_same_v<log_of<consuming>, dingo::core::no_transaction_log>);
  static_assert(!std::is_polymorphic_v<consuming>);

  using keeping =
      backend_of<dingo::static_container_traits,
                 dingo::bind<scope<dingo::shared>, storage<payload>>,
                 dingo::bind<scope<dingo::unique>, storage<destructible>>,
                 dingo::bind<scope<dingo::shared_cyclical>, storage<config>>>;
  // Each of the two slots that construct an object, and the retained objects
  // of the container, once; the log never grows.
  static_assert(std::is_same_v<log_of<keeping>,
                               dingo::core::basic_transaction_log<3, false>>);
  static_assert(!std::is_polymorphic_v<keeping>);
  SUCCEED();
}

} // namespace
