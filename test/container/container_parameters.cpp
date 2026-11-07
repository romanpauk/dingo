//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <typeinfo>

namespace {

// Bytes the counting allocators have handed out and not yet taken back.
std::size_t live_bytes = 0;

template <typename T> struct counting_allocator {
  using value_type = T;

  counting_allocator() noexcept = default;
  template <typename U>
  counting_allocator(const counting_allocator<U> &) noexcept {}

  T *allocate(std::size_t n) {
    live_bytes += n * sizeof(T);
    return static_cast<T *>(::operator new(n * sizeof(T)));
  }

  void deallocate(T *p, std::size_t n) noexcept {
    live_bytes -= n * sizeof(T);
    ::operator delete(p);
  }
};

template <typename T, typename U>
bool operator==(const counting_allocator<T> &,
                const counting_allocator<U> &) noexcept {
  return true;
}

template <typename T, typename U>
bool operator!=(const counting_allocator<T> &,
                const counting_allocator<U> &) noexcept {
  return false;
}

struct static_rtti_traits {
  using rtti_type = dingo::rtti<dingo::static_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = dingo::lookups<>;
};

using allocator_container =
    dingo::container<dingo::runtime_container_traits, counting_allocator<char>>;
using static_rtti_container = dingo::container<static_rtti_traits>;

template <int N> struct leaf {
  leaf() : value(N) {}
  int value;
};

// Registers one leaf in every stored form a container parameter can affect and
// checks that each resolves to the registered instance.
template <typename Container> void check_stored_forms(Container &container) {
  leaf<1> external_value;
  leaf<2> external_reference;
  leaf<3> external_pointer;
  auto external_shared = std::make_shared<leaf<4>>();

  container.template register_type<dingo::scope<dingo::shared>,
                                   dingo::storage<leaf<0>>>();
  container.template register_type<dingo::scope<dingo::external>,
                                   dingo::storage<leaf<1>>>(external_value);
  container.template register_type<dingo::scope<dingo::external>,
                                   dingo::storage<leaf<2> &>>(
      external_reference);
  container.template register_type<dingo::scope<dingo::external>,
                                   dingo::storage<leaf<3> *>>(
      &external_pointer);
  container.template register_type<dingo::scope<dingo::external>,
                                   dingo::storage<std::shared_ptr<leaf<4>>>>(
      external_shared);

  auto &shared = container.template resolve<leaf<0> &>();
  EXPECT_EQ(shared.value, 0);
  EXPECT_EQ(&container.template resolve<leaf<0> &>(), &shared);
  EXPECT_EQ(container.template resolve<leaf<1> &>().value, 1);
  EXPECT_EQ(&container.template resolve<leaf<2> &>(), &external_reference);
  EXPECT_EQ(&container.template resolve<leaf<3> &>(), &external_pointer);
  EXPECT_EQ(&container.template resolve<leaf<4> &>(), external_shared.get());
}

template <typename Container>
class container_parameter_test : public ::testing::Test {};

using parameter_container_types =
    ::testing::Types<allocator_container, static_rtti_container>;
TYPED_TEST_SUITE(container_parameter_test, parameter_container_types, );

TYPED_TEST(container_parameter_test, resolves_every_stored_form) {
  TypeParam container;
  check_stored_forms(container);
}

TEST(container_parameter_test, custom_allocator_backs_the_container) {
  const std::size_t before = live_bytes;
  {
    allocator_container container;
    const std::size_t empty = live_bytes;
    EXPECT_GT(empty, before);
    check_stored_forms(container);
    EXPECT_GT(live_bytes, empty);
  }
  EXPECT_EQ(live_bytes, before);
}

TEST(container_parameter_test, custom_rtti_is_the_container_rtti) {
  static_assert(std::is_same_v<static_rtti_container::rtti_type,
                               dingo::rtti<dingo::static_provider>>);
}

// Counts every way an instance comes to exist and goes away.
struct counted {
  counted() { ++constructed; }
  counted(const counted &) { ++copied; }
  counted(counted &&) noexcept { ++moved; }
  ~counted() { ++destroyed; }

  static void reset() { constructed = copied = moved = destroyed = 0; }

  inline static std::size_t constructed = 0;
  inline static std::size_t copied = 0;
  inline static std::size_t moved = 0;
  inline static std::size_t destroyed = 0;
};

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

template <typename Kind> class lifetime_count_test : public ::testing::Test {};

using container_kinds = ::testing::Types<runtime_kind, static_kind>;
TYPED_TEST_SUITE(lifetime_count_test, container_kinds, );

struct count_bounds {
  std::size_t copy_min, copy_max, move_min, move_max;
};

// Resolves Request once from a container holding Storage in Scope, then checks
// that one instance was constructed, copies and moves stayed within bounds and
// every instance was destroyed with the container.
template <typename Kind, typename Scope, typename Storage, typename Request>
void check_counts(count_bounds bounds) {
  counted::reset();
  Kind::template with<
      dingo::bind<dingo::scope<Scope>, dingo::storage<Storage>>>(
      [](auto &container) {
        decltype(auto) instance = container.template resolve<Request>();
        (void)instance;
      });
  SCOPED_TRACE(::testing::Message()
               << typeid(Storage).name() << " as " << typeid(Request).name());
  EXPECT_EQ(counted::constructed, 1u);
  EXPECT_GE(counted::copied, bounds.copy_min);
  EXPECT_LE(counted::copied, bounds.copy_max);
  EXPECT_GE(counted::moved, bounds.move_min);
  EXPECT_LE(counted::moved, bounds.move_max);
  EXPECT_EQ(counted::destroyed,
            counted::constructed + counted::copied + counted::moved);
}

TYPED_TEST(lifetime_count_test, shared_scope_neither_copies_nor_leaks) {
  using Kind = TypeParam;
  check_counts<Kind, dingo::shared, counted, counted &>({0, 0, 0, 0});
  check_counts<Kind, dingo::shared, std::shared_ptr<counted>,
               std::shared_ptr<counted>>({0, 0, 0, 0});
  check_counts<Kind, dingo::shared, std::optional<counted>,
               std::optional<counted> &>({0, 0, 1, 3});
}

TYPED_TEST(lifetime_count_test, unique_scope_moves_without_copying) {
  using Kind = TypeParam;
  check_counts<Kind, dingo::unique, counted, counted &&>({0, 0, 1, 3});
  check_counts<Kind, dingo::unique, std::unique_ptr<counted>,
               std::unique_ptr<counted> &&>({0, 0, 0, 0});
  check_counts<Kind, dingo::unique, std::optional<counted>,
               std::optional<counted>>({0, 1, 1, 3});
}

struct parent_dependent {
  explicit parent_dependent(int dependency) : value(dependency) {}
  int value;
};

struct shadowed_dependent {
  explicit shadowed_dependent(int dependency) : value(dependency) {}
  int value;
};

// A child of the container's own type finds the parent's bindings, and what it
// registers itself shadows the parent for the child only.
TEST(container_parameter_test, native_child_resolves_through_parent) {
  using container_type = dingo::runtime_container<>;
  container_type::allocator_type allocator;
  container_type parent(allocator);
  parent.register_type<dingo::scope<dingo::external>, dingo::storage<int>>(1);
  parent.register_type<dingo::scope<dingo::unique>,
                       dingo::storage<shadowed_dependent>>();
  parent.register_type<dingo::scope<dingo::unique>,
                       dingo::storage<parent_dependent>>();

  container_type child(&parent, allocator);
  EXPECT_EQ(child.resolve<parent_dependent>().value, 1);

  child.register_type<dingo::scope<dingo::external>, dingo::storage<int>>(2);
  child.register_type<dingo::scope<dingo::unique>,
                      dingo::storage<shadowed_dependent>>();
  EXPECT_EQ(parent.resolve<shadowed_dependent>().value, 1);
  EXPECT_EQ(child.resolve<shadowed_dependent>().value, 2);
}

} // namespace
