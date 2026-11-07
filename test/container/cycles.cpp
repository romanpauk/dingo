//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared_cyclical.h>

#include <gtest/gtest.h>

#include <memory>
#include <typeinfo>

namespace {

// How one node of a two-node cycle receives the other in its constructor.
struct by_reference {};
struct by_pointer {};
struct by_shared_ptr {};
struct by_shared_ptr_reference {};
struct by_shared_ptr_pointer {};

template <typename Edge, typename Target> struct edge;

template <typename Target> struct edge<by_reference, Target> {
  using type = Target &;
  static Target *address(Target &target) { return &target; }
};

template <typename Target> struct edge<by_pointer, Target> {
  using type = Target *;
  static Target *address(Target *target) { return target; }
};

template <typename Target> struct edge<by_shared_ptr, Target> {
  using type = std::shared_ptr<Target>;
  static Target *address(const type &target) { return target.get(); }
};

template <typename Target> struct edge<by_shared_ptr_reference, Target> {
  using type = std::shared_ptr<Target> &;
  static Target *address(type target) { return target.get(); }
};

template <typename Target> struct edge<by_shared_ptr_pointer, Target> {
  using type = std::shared_ptr<Target> *;
  static Target *address(type target) { return target->get(); }
};

template <typename AToB, typename BToA> struct node_b;

template <typename AToB, typename BToA> struct node_a {
  using edge_type = edge<AToB, node_b<AToB, BToA>>;
  explicit node_a(typename edge_type::type target)
      : other(edge_type::address(target)) {}
  node_b<AToB, BToA> *other;
};

template <typename AToB, typename BToA> struct node_b {
  using edge_type = edge<BToA, node_a<AToB, BToA>>;
  explicit node_b(typename edge_type::type target)
      : other(edge_type::address(target)) {}
  node_a<AToB, BToA> *other;
};

// How a node is stored: by value or behind a shared_ptr.
struct by_value {
  template <typename T> using type = T;
  static constexpr bool shared_ownership = false;
};

struct by_shared_ptr_storage {
  template <typename T> using type = std::shared_ptr<T>;
  static constexpr bool shared_ownership = true;
};

template <typename Storage, typename Node>
using cycle_binding =
    dingo::bind<dingo::scope<dingo::shared_cyclical>,
                dingo::storage<typename Storage::template type<Node>>,
                dingo::dependencies<typename Node::edge_type::type>>;

struct runtime_kind {
  template <typename... Args, typename Container>
  static void add(Container &container, dingo::bind<Args...> *) {
    container.template register_type<Args...>();
  }

  template <typename... Bindings, typename Function>
  static void with(Function &&function) {
    dingo::runtime_container<> container;
    (add(container, static_cast<Bindings *>(nullptr)), ...);
    function(container);
  }
};

struct static_kind {
  template <typename... Bindings, typename Function>
  static void with(Function &&function) {
    dingo::static_container<dingo::bindings<Bindings...>> container;
    function(container);
  }
};

// Every request a node's storage publishes reaches the one instance.
template <typename Storage, typename Node, typename Container>
void check_node(Container &container, Node &node) {
  EXPECT_EQ(&container.template resolve<Node &>(), &node);
  EXPECT_EQ(container.template resolve<Node *>(), &node);
  if constexpr (Storage::shared_ownership) {
    auto first = container.template resolve<std::shared_ptr<Node>>();
    auto second = container.template resolve<std::shared_ptr<Node>>();
    EXPECT_EQ(first.get(), &node);
    EXPECT_FALSE(first.owner_before(second) || second.owner_before(first));
    EXPECT_EQ(container.template resolve<std::shared_ptr<Node> &>().get(),
              &node);
    EXPECT_EQ(container.template resolve<std::shared_ptr<Node> *>()->get(),
              &node);
  }
}

// Registers a and b in the shared_cyclical scope, each depending on the other,
// and checks that resolving them closes the cycle over the kept instances.
template <typename Kind, typename AStorage, typename BStorage, typename AToB,
          typename BToA>
void check_cycle() {
  using a = node_a<AToB, BToA>;
  using b = node_b<AToB, BToA>;
  SCOPED_TRACE(::testing::Message() << typeid(a).name());
  Kind::template with<cycle_binding<AStorage, a>, cycle_binding<BStorage, b>>(
      [](auto &container) {
        auto &first = container.template resolve<a &>();
        auto &second = container.template resolve<b &>();
        EXPECT_EQ(first.other, &second);
        EXPECT_EQ(second.other, &first);
        check_node<AStorage>(container, first);
        check_node<BStorage>(container, second);
      });
}

template <typename... Edges> struct edges {};

using plain_edges = edges<by_reference, by_pointer>;
using all_edges = edges<by_reference, by_pointer, by_shared_ptr,
                        by_shared_ptr_reference, by_shared_ptr_pointer>;

template <typename Kind, typename AStorage, typename BStorage, typename AToB,
          typename... BToA>
void check_cycles_from(edges<BToA...>) {
  (check_cycle<Kind, AStorage, BStorage, AToB, BToA>(), ...);
}

// Checks the cycle for every pair of edges, a reaching b over one of AToB and b
// reaching a over one of BToA.
template <typename Kind, typename AStorage, typename BStorage, typename... AToB,
          typename BToAEdges>
void check_cycles(edges<AToB...>, BToAEdges b_to_a) {
  (check_cycles_from<Kind, AStorage, BStorage, AToB>(b_to_a), ...);
}

template <typename Kind> class cycle_test : public ::testing::Test {};

using container_kinds = ::testing::Types<runtime_kind, static_kind>;
TYPED_TEST_SUITE(cycle_test, container_kinds, );

// A node stored by value publishes only a reference and a pointer, so only
// those can close a cycle into it.
TYPED_TEST(cycle_test, value_nodes_close_over_references_and_pointers) {
  check_cycles<TypeParam, by_value, by_value>(plain_edges{}, plain_edges{});
}

TYPED_TEST(cycle_test, shared_ptr_nodes_close_over_every_edge) {
  check_cycles<TypeParam, by_shared_ptr_storage, by_shared_ptr_storage>(
      all_edges{}, all_edges{});
}

TYPED_TEST(cycle_test, mixed_nodes_close_over_what_each_publishes) {
  check_cycles<TypeParam, by_value, by_shared_ptr_storage>(all_edges{},
                                                           plain_edges{});
  check_cycles<TypeParam, by_shared_ptr_storage, by_value>(plain_edges{},
                                                           all_edges{});
}

} // namespace
