//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The reduction of a registration to a binding type: the parts it takes, the
// interfaces and the factory it defaults, the factory its dependencies
// declare, the arguments a factory declares for the static graph, and its
// local bindings. Replaces the type registration tests of the current
// implementation, which checked its type_registration and factory_traits.

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/function.h>
#include <dingo/registration/constructor.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <dingo/conversion/route.h>
#include <dingo/factory/construct.h>
#include <dingo/registration.h>
#include <dingo/selection/request.h>

#include <gtest/gtest.h>

#include <type_traits>

namespace {

using dingo::bind;
using dingo::bindings;
using dingo::constructor;
using dingo::dependencies;
using dingo::factory;
using dingo::function;
using dingo::interfaces;
using dingo::scope;
using dingo::storage;
using dingo::type_list;

// The factory type of the next implementation a factory<> part declares.
template <typename Factory>
using factory_of_t = typename dingo::factory_of<Factory>::type;

struct first_leaf {};
struct second_leaf {};

int build_from_double_and_cstr(double, const char *) { return 0; }

// spec: registration parts
TEST(next_engine, a_registration_takes_the_first_part_of_each_kind) {
  using registration =
      dingo::registration<scope<dingo::shared>, scope<dingo::unique>,
                          storage<first_leaf>, storage<second_leaf>,
                          factory<constructor<first_leaf>>,
                          factory<function<&build_from_double_and_cstr>>,
                          interfaces<first_leaf>, interfaces<second_leaf>>;
  static_assert(std::is_same_v<registration::scope_part, scope<dingo::shared>>);
  static_assert(
      std::is_same_v<registration::storage_part, storage<first_leaf>>);
  static_assert(std::is_same_v<registration::factory_part,
                               factory<constructor<first_leaf>>>);
  static_assert(
      std::is_same_v<registration::interfaces_part, interfaces<first_leaf>>);
  using declaring =
      dingo::registration<scope<dingo::unique>, storage<first_leaf>,
                          dependencies<long>, dependencies<short>>;
  static_assert(
      std::is_same_v<declaring::dependencies_part, dependencies<long>>);

  // The parts may come in any order.
  using reordered =
      dingo::registration<interfaces<double, float>, factory<constructor<int>>,
                          storage<int>, scope<dingo::unique>>;
  static_assert(std::is_same_v<reordered::scope_part, scope<dingo::unique>>);
  static_assert(std::is_same_v<reordered::storage_part, storage<int>>);
  static_assert(
      std::is_same_v<reordered::interfaces_part, interfaces<double, float>>);
  static_assert(
      std::is_same_v<reordered::binding::interfaces, type_list<double, float>>);
  SUCCEED();
}

TEST(next_engine,
     a_registration_without_interfaces_is_named_by_its_stored_leaf) {
  using plain = dingo::registration<scope<dingo::shared>, storage<int>>;
  static_assert(std::is_same_v<plain::interfaces_part, interfaces<>>);
  static_assert(std::is_same_v<plain::binding::interfaces, type_list<>>);
  static_assert(
      std::is_same_v<plain::binding::factory_type, dingo::detected_factory>);
  static_assert(dingo::serves_interface(plain::binding::descriptor,
                                        dingo::type_id_v<int>));
  static_assert(!dingo::serves_interface(plain::binding::descriptor,
                                         dingo::type_id_v<double>));

  // Declaring the stored type as the only interface names the binding by its
  // own type, as declaring none does.
  using named = dingo::registration<scope<dingo::shared>, storage<const int &>,
                                    interfaces<int>>;
  static_assert(std::is_same_v<named::binding::interfaces, type_list<>>);
  static_assert(dingo::serves_interface(named::binding::descriptor,
                                        dingo::type_id_v<int>));

  // A factory part makes the leaf; the storage part names the stored type.
  using made = dingo::registration<scope<dingo::shared>, storage<int>,
                                   factory<constructor<int>>>;
  static_assert(std::is_same_v<made::binding::stored_type, int>);
  static_assert(std::is_same_v<made::binding::interfaces, type_list<>>);
  SUCCEED();
}

struct two_constructors {
  two_constructors(int, float) {}
  two_constructors(double, bool) {}
};

TEST(next_engine, dependencies_declare_the_constructor_of_a_registration) {
  using registration =
      dingo::registration<scope<dingo::unique>, storage<two_constructors>,
                          dependencies<double, bool>>;
  using declared =
      dingo::constructor_factory<dingo::declared_leaf<double, bool>>;
  static_assert(std::is_same_v<registration::declared_factory, declared>);
  static_assert(std::is_same_v<registration::binding::factory_type, declared>);
  static_assert(std::is_same_v<
                dingo::declared_arguments<registration::binding::factory_type,
                                          two_constructors>::type,
                type_list<double, bool>>);
  SUCCEED();
}

// A registration that declares dependencies<> and a factory, which declares
// its own, is rejected: the generated diagnostic test
// registration_dependencies_twice.

struct service {
  explicit service(int, float) {}
};

struct annotated_service {
  DINGO_CONSTRUCTOR(annotated_service(int, float)) {}
};

struct defaulted_service {};

TEST(next_engine, a_factory_declares_the_arguments_of_its_leaf) {
  using dingo::declared_arguments;

  // A declared constructor and a function declare their arguments.
  static_assert(
      std::is_same_v<
          declared_arguments<factory_of_t<constructor<service(int, float)>>,
                             service>::type,
          type_list<int, float>>);
  static_assert(
      std::is_same_v<
          declared_arguments<
              factory_of_t<function<&build_from_double_and_cstr>>, int>::type,
          type_list<double, const char *>>);
  // The annotated constructor of a type declares its arguments, whether the
  // factory names the type or the registration detects it.
  static_assert(std::is_same_v<
                declared_arguments<factory_of_t<constructor<annotated_service>>,
                                   annotated_service>::type,
                type_list<int, float>>);
  static_assert(
      std::is_same_v<
          declared_arguments<dingo::detected_factory, annotated_service>::type,
          type_list<int, float>>);
  using detected =
      dingo::registration<scope<dingo::unique>, storage<annotated_service>>;
  static_assert(
      std::is_same_v<declared_arguments<detected::binding::factory_type,
                                        annotated_service>::type,
                     type_list<int, float>>);
  // A detected constructor declares none: its arguments are known only when
  // the object is constructed.
  static_assert(std::is_void_v<
                declared_arguments<factory_of_t<constructor<defaulted_service>>,
                                   defaulted_service>::type>);
  static_assert(std::is_void_v<
                declared_arguments<dingo::detected_factory, service>::type>);

  // A callable declares the arguments of its signature.
  auto callable_factory =
      dingo::callable<int(short, long)>([](short, long) { return 0; });
  static_assert(
      std::is_same_v<declared_arguments<decltype(callable_factory), int>::type,
                     type_list<short, long>>);
  (void)callable_factory;
  SUCCEED();
}

struct local_dependency {};
local_dependency make_local_dependency() { return {}; }
struct local_service {
  explicit local_service(local_dependency &) {}
};

TEST(next_engine, local_bindings_are_binding_types_of_the_registration) {
  using empty = dingo::registration<scope<dingo::shared>,
                                    storage<local_service>, bindings<>>;
  static_assert(std::is_same_v<empty::binding::locals, type_list<>>);
  using without =
      dingo::registration<scope<dingo::shared>, storage<local_service>>;
  static_assert(std::is_same_v<without::binding::locals, type_list<>>);

  using local = bind<scope<dingo::shared>, storage<local_dependency>,
                     factory<function<&make_local_dependency>>>;
  using with =
      dingo::registration<scope<dingo::shared>, storage<local_service>,
                          dependencies<local_dependency &>, bindings<local>>;
  using local_binding = dingo::bind_registration<local>::type::binding;
  static_assert(
      std::is_same_v<with::binding::locals, type_list<local_binding>>);
  static_assert(std::is_same_v<local_binding::stored_type, local_dependency>);
  static_assert(
      std::is_same_v<local_binding::factory_type,
                     dingo::function_factory<&make_local_dependency>>);
  SUCCEED();
}

} // namespace
