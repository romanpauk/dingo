//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace dingo;

namespace {

// Models a process composed from runtime parent containers:
//
//   base (low-level services shared between channels)
//    <- ch1 / ch2 (per-channel services and the channel itself)
//    <- system (composition routed through ch1 / ch2)
//
struct process_api {
  process_api() : marker(42) { ++instances; }
  static int instances;
  int marker;
};
int process_api::instances = 0;

struct shared_api {
  shared_api() { ++instances; }
  static int instances;
};
int shared_api::instances = 0;

struct channel_api {
  explicit channel_api(int channel_id) : id(channel_id) { ++instances; }
  static int instances;
  int id;
};
int channel_api::instances = 0;

struct channel {
  channel(std::shared_ptr<shared_api> shared_dep,
          std::shared_ptr<channel_api> local_dep,
          std::shared_ptr<process_api> process_dep)
      : shared(std::move(shared_dep)), local(std::move(local_dep)),
        process(std::move(process_dep)) {}

  std::shared_ptr<shared_api> shared;
  std::shared_ptr<channel_api> local;
  std::shared_ptr<process_api> process;
};

struct system_root {
  system_root(std::vector<std::shared_ptr<channel>> channel_list,
              std::shared_ptr<shared_api> shared_dep,
              std::shared_ptr<process_api> process_dep)
      : channels(std::move(channel_list)), shared(std::move(shared_dep)),
        process(std::move(process_dep)) {}

  std::vector<std::shared_ptr<channel>> channels;
  std::shared_ptr<shared_api> shared;
  std::shared_ptr<process_api> process;
};

struct configured_channel_consumer {
  explicit configured_channel_consumer(
      std::shared_ptr<channel> selected_channel)
      : selected(std::move(selected_channel)) {}

  std::shared_ptr<channel> selected;
};

struct system_configuration {
  explicit system_configuration(std::size_t configured_channel_id)
      : channel_id(configured_channel_id) {}

  std::size_t channel_id;
};

struct system_container_traits : dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<std::size_t, channel, dingo::one, dingo::array<2>>>;
};

struct tree_node_api {
  tree_node_api(std::string node_name, int node_weight,
                std::shared_ptr<tree_node_api> parent_node = {})
      : name(std::move(node_name)), weight(node_weight),
        parent(std::move(parent_node)) {}

  int operation(int value) const {
    if (parent == nullptr) {
      return value + weight;
    }
    return parent->operation(value) + weight;
  }

  std::string path() const {
    if (parent == nullptr) {
      return name;
    }
    return parent->path() + "/" + name;
  }

  std::string name;
  int weight;
  std::shared_ptr<tree_node_api> parent;
};

struct channel_dependency_pack {
  std::shared_ptr<shared_api> shared;
  std::shared_ptr<channel_api> local;
  std::shared_ptr<process_api> process;
};

struct auto_constructed_channel_dependency {
  std::shared_ptr<channel_api> local;
};

struct packed_channel {
  explicit packed_channel(channel_dependency_pack channel_deps)
      : deps(std::move(channel_deps)) {}

  channel_dependency_pack deps;
};

struct channel_hierarchy {
  channel_hierarchy() : system(&base), ch1(&base), ch2(&base) {
    shared_api::instances = 0;
    process_api::instances = 0;
    channel_api::instances = 0;

    base.register_type<scope<shared>, storage<std::shared_ptr<shared_api>>>();
    base.register_type<scope<shared>, storage<std::shared_ptr<process_api>>>();

    ch1.register_type<scope<shared>, storage<std::shared_ptr<channel_api>>>(
        callable([] { return std::make_shared<channel_api>(1); }));
    ch1.register_type<scope<shared>, storage<std::shared_ptr<channel>>>();

    ch2.register_type<scope<shared>, storage<std::shared_ptr<channel_api>>>(
        callable([] { return std::make_shared<channel_api>(2); }));
    ch2.register_type<scope<shared>, storage<std::shared_ptr<channel>>>();
  }

  void register_system_composition() {
    system.register_type<scope<shared>, storage<std::shared_ptr<channel>>,
                         interfaces<channel>>(
        callable([this] { return ch1.resolve<std::shared_ptr<channel>>(); }),
        key_value{std::size_t{0}});
    system.register_type<scope<shared>, storage<std::shared_ptr<channel>>,
                         interfaces<channel>>(
        callable([this] { return ch2.resolve<std::shared_ptr<channel>>(); }),
        key_value{std::size_t{1}});
    system.register_type<
        scope<shared>, storage<std::vector<std::shared_ptr<channel>>>>(callable(
        [](dependency<std::shared_ptr<channel>, key_type<std::size_t, 0>> first,
           dependency<std::shared_ptr<channel>, key_type<std::size_t, 1>>
               second) {
          std::shared_ptr<channel> first_channel = first;
          std::shared_ptr<channel> second_channel = second;
          return std::vector<std::shared_ptr<channel>>{
              std::move(first_channel), std::move(second_channel)};
        }));
    system
        .register_type<scope<shared>, storage<std::shared_ptr<system_root>>>();
  }

  container<> base;
  container<system_container_traits, system_container_traits::allocator_type,
            container<>>
      system;
  container<> ch1;
  container<> ch2;
};

TEST(parent_container_resolution_test,
     base_shared_dependencies_are_used_by_system_and_channels) {
  channel_hierarchy h;
  h.register_system_composition();

  auto resolved_system = h.system.resolve<std::shared_ptr<system_root>>();
  auto c1 = resolved_system->channels[0];
  auto c2 = resolved_system->channels[1];
  auto base_shared = h.base.resolve<std::shared_ptr<shared_api>>();
  auto base_process = h.base.resolve<std::shared_ptr<process_api>>();

  ASSERT_EQ(resolved_system->shared, base_shared);
  ASSERT_EQ(c1->shared, c2->shared);
  ASSERT_EQ(c1->shared, base_shared);

  ASSERT_EQ(resolved_system->process, base_process);
  ASSERT_EQ(c1->process, c2->process);
  ASSERT_EQ(c1->process, base_process);

  ASSERT_EQ(shared_api::instances, 1);
  ASSERT_EQ(process_api::instances, 1);
}

TEST(parent_container_resolution_test,
     channels_use_only_their_own_local_binding_instances) {
  channel_hierarchy h;

  auto c1 = h.ch1.resolve<std::shared_ptr<channel>>();
  auto c2 = h.ch2.resolve<std::shared_ptr<channel>>();

  ASSERT_EQ(c1->local, h.ch1.resolve<std::shared_ptr<channel_api>>());
  ASSERT_EQ(c2->local, h.ch2.resolve<std::shared_ptr<channel_api>>());
  ASSERT_NE(c1->local, c2->local);
  ASSERT_EQ(c1->local->id, 1);
  ASSERT_EQ(c2->local->id, 2);
  ASSERT_EQ(channel_api::instances, 2);

  ASSERT_THROW(h.base.resolve<std::shared_ptr<channel_api>>(),
               type_not_found_exception);
  ASSERT_THROW(h.system.resolve<std::shared_ptr<channel_api>>(),
               type_not_found_exception);
}

TEST(parent_container_resolution_test, channels_use_base_parent_dependency) {
  channel_hierarchy h;

  auto c1 = h.ch1.resolve<std::shared_ptr<channel>>();
  auto c2 = h.ch2.resolve<std::shared_ptr<channel>>();

  ASSERT_EQ(c1->process, c2->process);
  ASSERT_EQ(c1->process->marker, 42);
  ASSERT_EQ(c1->process, h.base.resolve<std::shared_ptr<process_api>>());
  ASSERT_EQ(process_api::instances, 1);
}

TEST(parent_container_resolution_test,
     children_resolve_shared_dependency_from_base) {
  channel_hierarchy h;

  auto from_ch1 = h.ch1.resolve<std::shared_ptr<shared_api>>();
  auto from_ch2 = h.ch2.resolve<std::shared_ptr<shared_api>>();

  ASSERT_EQ(from_ch1, from_ch2);
  ASSERT_EQ(from_ch1, h.base.resolve<std::shared_ptr<shared_api>>());
  ASSERT_EQ(shared_api::instances, 1);
}

// A factory registered in one container may delegate to another container.
// The delegating resolution is not a cycle even when both resolutions run
// on the same thread and request the same type: recursion is tracked per
// binding, not per type.
TEST(parent_container_resolution_test,
     delegating_factory_returns_the_instance_shared_by_children) {
  channel_hierarchy h;

  h.system.register_type<scope<shared>, storage<std::shared_ptr<shared_api>>>(
      callable([&base = h.base] {
        return base.resolve<std::shared_ptr<shared_api>>();
      }));

  // Resolve through the delegating factory first, while nothing is cached.
  auto from_system = h.system.resolve<std::shared_ptr<shared_api>>();
  auto c1 = h.ch1.resolve<std::shared_ptr<channel>>();
  auto c2 = h.ch2.resolve<std::shared_ptr<channel>>();

  ASSERT_EQ(from_system, c1->shared);
  ASSERT_EQ(from_system, c2->shared);
  ASSERT_EQ(shared_api::instances, 1);
}

TEST(parent_container_resolution_test,
     child_tree_node_binding_delegates_to_base_tree_node) {
  channel_hierarchy h;

  h.base.register_type<scope<shared>, storage<std::shared_ptr<tree_node_api>>>(
      callable([] { return std::make_shared<tree_node_api>("base", 100); }));
  h.ch1.register_type<scope<shared>, storage<std::shared_ptr<tree_node_api>>>(
      callable([&base = h.base] {
        return std::make_shared<tree_node_api>(
            "ch1", 1, base.resolve<std::shared_ptr<tree_node_api>>());
      }));

  auto base_node = h.base.resolve<std::shared_ptr<tree_node_api>>();
  auto ch1_node = h.ch1.resolve<std::shared_ptr<tree_node_api>>();
  auto ch2_node = h.ch2.resolve<std::shared_ptr<tree_node_api>>();

  ASSERT_NE(ch1_node, base_node);
  ASSERT_EQ(ch1_node->parent, base_node);
  ASSERT_EQ(ch2_node, base_node);
  ASSERT_EQ(ch1_node, h.ch1.resolve<std::shared_ptr<tree_node_api>>());

  ASSERT_EQ(base_node->path(), "base");
  ASSERT_EQ(ch1_node->path(), "base/ch1");
  ASSERT_EQ(base_node->operation(7), 107);
  ASSERT_EQ(ch1_node->operation(7), 108);
}

TEST(parent_container_resolution_test,
     auto_constructed_dependency_pack_uses_child_container) {
  channel_hierarchy h;

  h.ch1
      .register_type<scope<shared>, storage<std::shared_ptr<packed_channel>>>();

  auto resolved = h.ch1.resolve<std::shared_ptr<packed_channel>>();

  ASSERT_EQ(resolved->deps.shared,
            h.base.resolve<std::shared_ptr<shared_api>>());
  ASSERT_EQ(resolved->deps.process,
            h.base.resolve<std::shared_ptr<process_api>>());
  ASSERT_EQ(resolved->deps.local,
            h.ch1.resolve<std::shared_ptr<channel_api>>());
  ASSERT_THROW(h.base.resolve<std::shared_ptr<channel_api>>(),
               type_not_found_exception);
}

TEST(parent_container_resolution_test,
     missing_binding_auto_construction_uses_original_child_container) {
  channel_hierarchy h;

  auto resolved = h.ch1.resolve<auto_constructed_channel_dependency>();

  ASSERT_EQ(resolved.local, h.ch1.resolve<std::shared_ptr<channel_api>>());
  ASSERT_THROW(h.base.resolve<std::shared_ptr<channel_api>>(),
               type_not_found_exception);
}

TEST(parent_container_resolution_test,
     system_bundles_channels_resolved_from_children) {
  channel_hierarchy h;
  h.register_system_composition();

  auto resolved_system = h.system.resolve<std::shared_ptr<system_root>>();

  ASSERT_EQ(resolved_system->channels.size(), 2u);
  ASSERT_EQ(h.system.resolve<std::shared_ptr<channel>>(std::size_t{0}),
            h.ch1.resolve<std::shared_ptr<channel>>());
  ASSERT_EQ(h.system.resolve<std::shared_ptr<channel>>(std::size_t{1}),
            h.ch2.resolve<std::shared_ptr<channel>>());
  ASSERT_EQ(resolved_system->channels[0],
            h.ch1.resolve<std::shared_ptr<channel>>());
  ASSERT_EQ(resolved_system->channels[1],
            h.ch2.resolve<std::shared_ptr<channel>>());
  ASSERT_EQ(resolved_system->channels[0]->shared,
            resolved_system->channels[1]->shared);
  ASSERT_EQ(resolved_system->shared,
            h.base.resolve<std::shared_ptr<shared_api>>());
  ASSERT_EQ(resolved_system->channels[0]->shared, resolved_system->shared);
  ASSERT_EQ(resolved_system->process,
            h.base.resolve<std::shared_ptr<process_api>>());
  ASSERT_EQ(resolved_system->channels[0]->process, resolved_system->process);
  ASSERT_NE(resolved_system->channels[0]->local,
            resolved_system->channels[1]->local);
}

TEST(parent_container_resolution_test,
     system_registered_lambda_dynamically_resolves_configured_channel) {
  auto assert_configured_channel = [](std::size_t configured_channel_id,
                                      int expected_local_id) {
    channel_hierarchy h;
    h.register_system_composition();
    h.system.register_type<scope<shared>,
                           storage<std::shared_ptr<system_configuration>>>(
        callable([configured_channel_id] {
          return std::make_shared<system_configuration>(configured_channel_id);
        }));
    h.system.register_type<
        scope<shared>, storage<std::shared_ptr<configured_channel_consumer>>>(
        callable([system = &h.system](
                     std::shared_ptr<system_configuration> configuration) {
          return std::make_shared<configured_channel_consumer>(
              system->resolve<std::shared_ptr<channel>>(
                  configuration->channel_id));
        }));

    auto consumer =
        h.system.resolve<std::shared_ptr<configured_channel_consumer>>();

    ASSERT_EQ(consumer->selected, h.system.resolve<std::shared_ptr<channel>>(
                                      configured_channel_id));
    ASSERT_EQ(consumer->selected->local->id, expected_local_id);
  };

  assert_configured_channel(std::size_t{0}, 1);
  assert_configured_channel(std::size_t{1}, 2);
}

struct value_api {
  explicit value_api(int initial_value) : value(initial_value) {}
  int value;
};

// A child container may override a parent binding with a factory that
// decorates the parent's instance of the same type.
TEST(parent_container_resolution_test,
     child_binding_can_decorate_parent_binding) {
  container<> parent;
  parent.register_type<scope<shared>, storage<std::shared_ptr<value_api>>>(
      callable([] { return std::make_shared<value_api>(10); }));

  container<> child(&parent);
  child.register_type<scope<shared>, storage<std::shared_ptr<value_api>>>(
      callable([&parent] {
        auto inner = parent.resolve<std::shared_ptr<value_api>>();
        return std::make_shared<value_api>(inner->value + 1);
      }));

  ASSERT_EQ(child.resolve<std::shared_ptr<value_api>>()->value, 11);
  ASSERT_EQ(parent.resolve<std::shared_ptr<value_api>>()->value, 10);
}

struct cyclic_b;
struct cyclic_a {
  explicit cyclic_a(std::shared_ptr<cyclic_b> dependency)
      : b(std::move(dependency)) {}
  std::shared_ptr<cyclic_b> b;
};
struct cyclic_b {
  explicit cyclic_b(std::shared_ptr<cyclic_a> dependency)
      : a(std::move(dependency)) {}
  std::shared_ptr<cyclic_a> a;
};

TEST(parent_container_resolution_test,
     recursion_within_a_container_is_detected) {
  container<> c;
  c.register_type<scope<shared>, storage<std::shared_ptr<cyclic_a>>>();
  c.register_type<scope<shared>, storage<std::shared_ptr<cyclic_b>>>();

  ASSERT_THROW(c.resolve<std::shared_ptr<cyclic_a>>(),
               type_recursion_exception);
}

TEST(parent_container_resolution_test,
     recursion_across_containers_is_detected) {
  container<> first;
  container<> second;
  first.register_type<scope<shared>, storage<std::shared_ptr<value_api>>>(
      callable(
          [&second] { return second.resolve<std::shared_ptr<value_api>>(); }));
  second.register_type<scope<shared>, storage<std::shared_ptr<value_api>>>(
      callable(
          [&first] { return first.resolve<std::shared_ptr<value_api>>(); }));

  ASSERT_THROW(first.resolve<std::shared_ptr<value_api>>(),
               type_recursion_exception);
}

} // namespace
