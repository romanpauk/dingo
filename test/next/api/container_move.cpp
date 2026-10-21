//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// A runtime container moves with its bindings, its instances, the child
// containers of its registrations, and its collections; a child follows its
// moved parent. A container of compile-time bindings is not movable.

#include <dingo/container.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/runtime_container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <new>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

struct static_container_with_dynamic_rtti_traits {
  using rtti_type = dingo::rtti<dingo::typeid_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = std::tuple<>;
};

struct dynamic_container_with_static_rtti_traits {
  using rtti_type = dingo::rtti<dingo::static_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = std::tuple<>;
};

struct move_dependency {
  move_dependency() : value(7) {}

  int value;
};

struct move_service {
  explicit move_service(move_dependency &value) : dependency(value) {}

  move_dependency &dependency;
};

struct move_child_dependency {
  move_child_dependency() : value(11) {}

  int value;
};

struct move_child_service {
  move_child_service(move_child_dependency &child, move_dependency &root)
      : value(child.value + root.value) {}

  int value;
};

struct move_static_dependency {
  move_static_dependency() : value(13) {}

  int value;
};

struct move_static_service {
  move_static_service(move_static_dependency &child, move_dependency &root)
      : value(child.value + root.value) {}

  int value;
};

struct move_processor {
  virtual ~move_processor() = default;
  virtual int id() const = 0;
};

template <int Id> struct move_processor_impl : move_processor {
  int id() const override { return Id; }
};

struct move_collection_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::collection<move_processor>>;
};

template <typename T> struct move_test_allocator {
  using value_type = T;

  move_test_allocator() noexcept = default;
  template <typename U>
  move_test_allocator(const move_test_allocator<U> &) noexcept {}

  T *allocate(std::size_t n) {
    return static_cast<T *>(::operator new(n * sizeof(T)));
  }

  void deallocate(T *ptr, std::size_t) noexcept { ::operator delete(ptr); }
};

template <typename T, typename U>
bool operator==(const move_test_allocator<T> &,
                const move_test_allocator<U> &) noexcept {
  return true;
}

template <typename T, typename U>
bool operator!=(const move_test_allocator<T> &,
                const move_test_allocator<U> &) noexcept {
  return false;
}

using move_parent_container = dingo::container<>;

template <typename Container> struct move_container_factory {
  static std::optional<Container> make(move_parent_container &) {
    return std::optional<Container>(std::in_place);
  }
};

template <typename Traits, typename Allocator>
struct move_container_factory<
    dingo::container<Traits, Allocator, move_parent_container>> {
  using container_type =
      dingo::container<Traits, Allocator, move_parent_container>;

  static std::optional<container_type> make(move_parent_container &parent) {
    return std::optional<container_type>(std::in_place, &parent);
  }
};

template <typename Container>
class next_container_move : public ::testing::Test {
protected:
  std::optional<Container> make() {
    return move_container_factory<Container>::make(parent_);
  }

  // Builds a new container where the moved-from one lived, so anything still
  // pointing at the old container sees different bindings.
  void replace(std::optional<Container> &container) {
    container.reset();
    container = make();
  }

private:
  move_parent_container parent_;
};

using movable_container_types = ::testing::Types<
    dingo::container<>, dingo::container<dingo::static_container_traits>,
    dingo::container<static_container_with_dynamic_rtti_traits>,
    dingo::container<dynamic_container_with_static_rtti_traits>,
    dingo::container<dingo::dynamic_container_traits,
                     move_test_allocator<char>>,
    dingo::container<dingo::dynamic_container_traits, std::allocator<char>,
                     move_parent_container>,
    dingo::runtime_container<>>;

TYPED_TEST_SUITE(next_container_move, movable_container_types, );

TYPED_TEST(next_container_move, is_movable_but_not_copyable) {
  static_assert(std::is_move_constructible_v<TypeParam>);
  static_assert(std::is_move_assignable_v<TypeParam>);
  static_assert(!std::is_copy_constructible_v<TypeParam>);
  static_assert(!std::is_copy_assignable_v<TypeParam>);
}

TYPED_TEST(next_container_move,
           move_construction_keeps_bindings_and_instances) {
  auto source = this->make();
  source->template register_type<dingo::scope<dingo::shared>,
                                 dingo::storage<move_dependency>>();
  source->template register_type<dingo::scope<dingo::shared>,
                                 dingo::storage<move_service>>();
  auto &dependency = source->template resolve<move_dependency &>();

  TypeParam target(std::move(*source));
  this->replace(source);

  EXPECT_EQ(std::addressof(target.template resolve<move_dependency &>()),
            std::addressof(dependency));
  auto &service = target.template resolve<move_service &>();
  EXPECT_EQ(std::addressof(service.dependency), std::addressof(dependency));
}

TYPED_TEST(next_container_move, move_assignment_replaces_bindings) {
  auto source = this->make();
  source->template register_type<dingo::scope<dingo::shared>,
                                 dingo::storage<move_dependency>>();
  source->template register_type<dingo::scope<dingo::shared>,
                                 dingo::storage<move_service>>();
  auto &dependency = source->template resolve<move_dependency &>();

  auto target = this->make();
  target->template register_type<dingo::scope<dingo::shared>,
                                 dingo::storage<move_child_dependency>>();
  (void)target->template resolve<move_child_dependency &>();

  *target = std::move(*source);
  this->replace(source);

  EXPECT_EQ(std::addressof(target->template resolve<move_dependency &>()),
            std::addressof(dependency));
  auto &service = target->template resolve<move_service &>();
  EXPECT_EQ(std::addressof(service.dependency), std::addressof(dependency));
  EXPECT_THROW((void)target->template resolve<move_child_dependency &>(),
               dingo::type_not_found_exception);
}

TYPED_TEST(next_container_move, moved_container_keeps_registration_containers) {
  auto source = this->make();
  source->template register_type<dingo::scope<dingo::shared>,
                                 dingo::storage<move_dependency>>();
  auto child = source->template register_type<
      dingo::scope<dingo::shared>, dingo::storage<move_child_service>,
      dingo::dependencies<move_child_dependency &, move_dependency &>>();
  auto *child_container = std::addressof(child.get());

  TypeParam target(std::move(*source));
  this->replace(source);

  EXPECT_EQ(std::addressof(child.get()), child_container);
  child->template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<move_child_dependency>>();
  EXPECT_EQ(target.template resolve<move_child_service &>().value, 18);
}

TYPED_TEST(next_container_move,
           moved_container_keeps_static_registration_containers) {
  auto source = this->make();
  source->template register_type<dingo::scope<dingo::shared>,
                                 dingo::storage<move_dependency>>();
  auto child = source->template register_type<
      dingo::scope<dingo::shared>, dingo::storage<move_static_service>,
      dingo::dependencies<move_static_dependency &, move_dependency &>,
      dingo::bindings<dingo::bind<dingo::scope<dingo::shared>,
                                  dingo::storage<move_static_dependency>>>>();
  auto *child_container = std::addressof(child.get());

  TypeParam target(std::move(*source));
  this->replace(source);

  EXPECT_EQ(std::addressof(child.get()), child_container);
  EXPECT_EQ(target.template resolve<move_static_service &>().value, 20);
}

TEST(next_container_move_untyped, moved_container_keeps_collection_factories) {
  std::optional<dingo::container<move_collection_traits>> source(std::in_place);
  source->register_type<dingo::scope<dingo::shared>,
                        dingo::storage<move_processor_impl<1>>,
                        dingo::interfaces<move_processor>>();
  source->register_type<dingo::scope<dingo::shared>,
                        dingo::storage<move_processor_impl<2>>,
                        dingo::interfaces<move_processor>>();
  source
      ->register_type_collection<dingo::scope<dingo::unique>,
                                 dingo::storage<std::vector<move_processor *>>>(
          [](auto &values, move_processor *value) { values.push_back(value); });

  dingo::container<move_collection_traits> target(std::move(*source));
  source.reset();
  source.emplace();

  auto values = target.resolve<std::vector<move_processor *>>();
  ASSERT_EQ(values.size(), 2u);
  EXPECT_EQ(values[0]->id(), 1);
  EXPECT_EQ(values[1]->id(), 2);
}

class move_host {
public:
  explicit move_host(dingo::container<> container)
      : container_(std::move(container)) {}

  move_service &service() { return container_.resolve<move_service &>(); }

private:
  dingo::container<> container_;
};

class move_host_builder {
public:
  move_host_builder &add_dependency() {
    container_.register_type<dingo::scope<dingo::shared>,
                             dingo::storage<move_dependency>>();
    return *this;
  }

  move_host_builder &add_service() {
    container_.register_type<dingo::scope<dingo::shared>,
                             dingo::storage<move_service>>();
    return *this;
  }

  move_dependency &dependency() {
    return container_.resolve<move_dependency &>();
  }

  move_host build() && { return move_host(std::move(container_)); }

private:
  dingo::container<> container_;
};

TEST(next_container_move_untyped, builder_hands_container_to_host) {
  std::optional<move_host_builder> builder(std::in_place);
  builder->add_dependency().add_service();
  auto &dependency = builder->dependency();

  move_host host = std::move(*builder).build();
  builder.reset();
  builder.emplace();

  EXPECT_EQ(std::addressof(host.service().dependency),
            std::addressof(dependency));
  EXPECT_EQ(host.service().dependency.value, 7);
}

using move_service_bindings = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<move_service>>>;

using move_child_container_types = ::testing::Types<
    dingo::container<dingo::dynamic_container_traits, std::allocator<char>,
                     move_parent_container>,
    dingo::container<move_service_bindings, move_parent_container>>;

template <typename Child>
class next_container_move_parent : public ::testing::Test {};

// Whether a child declares no compile-time binding of the service, so it
// registers one.
template <typename Child> constexpr bool registers_service = true;
template <>
constexpr bool registers_service<
    dingo::container<move_service_bindings, move_parent_container>> = false;

TYPED_TEST_SUITE(next_container_move_parent, move_child_container_types, );

// Disabled: a child keeps the address of its parent, so it does not follow the
// parent when it moves, and finds the container constructed at the old
// address instead.
TYPED_TEST(next_container_move_parent, DISABLED_child_follows_moved_parent) {
  std::optional<move_parent_container> source(std::in_place);
  source->register_type<dingo::scope<dingo::shared>,
                        dingo::storage<move_dependency>>();
  auto &dependency = source->resolve<move_dependency &>();

  TypeParam child(std::addressof(*source));
  if constexpr (registers_service<TypeParam>) {
    child.template register_type<dingo::scope<dingo::shared>,
                                 dingo::storage<move_service>>();
  }

  move_parent_container parent(std::move(*source));
  source.reset();
  source.emplace();

  auto &service = child.template resolve<move_service &>();
  EXPECT_EQ(std::addressof(service.dependency), std::addressof(dependency));
}

// Disabled: the static and the mixed container declare a defaulted move
// constructor, so the traits report them movable, but moving one does not
// compile: the constructor that takes the instances of the external bindings
// takes the moved container as an instance.
TEST(next_container_move_untyped,
     DISABLED_compile_time_binding_containers_are_not_movable) {
  using bindings =
      dingo::bindings<dingo::bind<dingo::scope<dingo::shared>,
                                  dingo::storage<move_dependency>>>;
  using container_type = dingo::container<bindings>;
  using static_container_type = dingo::static_container<bindings>;

  EXPECT_FALSE(std::is_move_constructible_v<container_type>);
  EXPECT_FALSE(std::is_move_assignable_v<container_type>);
  EXPECT_FALSE(std::is_copy_constructible_v<container_type>);
  EXPECT_FALSE(std::is_copy_assignable_v<container_type>);
  EXPECT_FALSE(std::is_move_constructible_v<static_container_type>);
  EXPECT_FALSE(std::is_move_assignable_v<static_container_type>);
  EXPECT_FALSE(std::is_copy_constructible_v<static_container_type>);
  EXPECT_FALSE(std::is_copy_assignable_v<static_container_type>);
}

} // namespace
