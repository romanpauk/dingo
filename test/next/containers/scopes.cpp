//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Scopes a user adds: a tag type whose scope_traits hold its descriptor, and
// may name a slot class of its own.

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <new>

namespace {

struct service {
  service() : value(++made) {}
  static inline int made = 0;
  int value;
};

struct other {};

// A scope that keeps one object per container, as shared does.
struct per_container {};

} // namespace

template <> struct dingo::scope_traits<per_container> {
  static constexpr dingo::scope_descriptor descriptor =
      dingo::scope_traits<dingo::shared>::descriptor;
};

namespace {

TEST(next_scopes, a_user_scope_is_a_descriptor) {
  dingo::container<> runtime;
  runtime.register_type<dingo::scope<per_container>, dingo::storage<service>>();
  service &first = runtime.resolve<service &>();
  EXPECT_EQ(&runtime.resolve<service &>(), &first);

  using bindings = dingo::bindings<
      dingo::bind<dingo::scope<per_container>, dingo::storage<service>>>;
  dingo::static_container<bindings> statics;
  EXPECT_EQ(&statics.resolve<service &>(), &statics.resolve<service &>());
  EXPECT_NE(&statics.resolve<service &>(), &first);

  dingo::container<bindings> mixed;
  mixed.register_type<dingo::scope<per_container>, dingo::storage<other>>();
  EXPECT_EQ(&mixed.resolve<service &>(), &mixed.resolve<service &>());
  EXPECT_EQ(&mixed.resolve<other &>(), &mixed.resolve<other &>());
}

// The object of a stored type that a process keeps, never destroyed.
template <typename Type> struct process_object {
  alignas(Type) static inline unsigned char storage[sizeof(Type)];
  static inline Type *object = nullptr;
};

// A slot that keeps one object of a stored type for the whole process, which
// every container of a binding of it shares.
template <typename Binding>
class process_slot : public dingo::scope_slot<Binding> {
public:
  using stored_type = typename Binding::stored_type;
  using dingo::scope_slot<Binding>::scope_slot;

  template <typename Resolver> stored_type &get(Resolver &resolver) {
    using kept = process_object<stored_type>;
    if (kept::object == nullptr) {
      this->construct(kept::storage, resolver);
      kept::object =
          std::launder(reinterpret_cast<stored_type *>(kept::storage));
    }
    return *kept::object;
  }

  template <typename Resolver> bool settled(Resolver & /*resolver*/) const {
    return true;
  }
};

struct per_process {};

} // namespace

template <> struct dingo::scope_traits<per_process> {
  static constexpr dingo::scope_descriptor descriptor =
      dingo::scope_traits<dingo::shared>::descriptor;
  template <typename Binding> using slot = process_slot<Binding>;
};

namespace {

struct global_service {
  global_service() : value(7) {}
  int value;
};

TEST(next_scopes, a_user_scope_names_its_slot) {
  using bindings = dingo::bindings<
      dingo::bind<dingo::scope<per_process>, dingo::storage<global_service>>>;
  dingo::static_container<bindings> first;
  dingo::container<> second;
  second.register_type<dingo::scope<per_process>,
                       dingo::storage<global_service>>();
  global_service &object = first.resolve<global_service &>();
  EXPECT_EQ(object.value, 7);
  EXPECT_EQ(&first.resolve<global_service &>(), &object);
  EXPECT_EQ(&second.resolve<global_service &>(), &object);
}

} // namespace
