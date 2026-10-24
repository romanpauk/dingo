//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The requests a binding serves, as the route engine decides them from the
// descriptors of the binding and of the request when the request is
// compiled: the routes its stored type tree derives, the qualifiers a request
// may add, the leaf a request names, the objects a binding keeps, and the
// conversions the conversion traits declare. Replaces the registration traits
// tests of the current implementation, which checked the published routes of
// its binding model and its conversion paths.

#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>
#include <dingo/type/annotated.h>
#include <dingo/type/type_conversion_traits.h>

#include <dingo/conversion/route.h>
#include <dingo/registration.h>
#include <dingo/selection/request.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/wrapper_traits.h>

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace {

using dingo::external;
using dingo::shared;
using dingo::shared_cyclical;
using dingo::unique;
namespace core = dingo::core;

// The binding a registration of `Scope`, `Stored`, and `Interfaces` declares.
template <typename Scope, typename Stored, typename... Interfaces>
struct binding_of {
  using type =
      typename core::registration<dingo::scope<Scope>, dingo::storage<Stored>,
                                  dingo::interfaces<Interfaces...>>::binding;
};

template <typename Scope, typename Stored> struct binding_of<Scope, Stored> {
  using type = typename core::registration<dingo::scope<Scope>,
                                           dingo::storage<Stored>>::binding;
};

template <typename Scope, typename Stored, typename... Interfaces>
using binding_t = typename binding_of<Scope, Stored, Interfaces...>::type;

// Whether the binding is registered, names the request, and serves it.
template <typename Binding, typename Request> constexpr bool serves() {
  constexpr core::request_descriptor request =
      core::request_traits<Request>::descriptor;
  return Binding::status == core::registration_status::registered &&
         core::finds(Binding::descriptor, request) &&
         core::select_route(Binding::descriptor, Binding::published, request,
                            Binding::exposed)
                 .reachable_type != core::no_reachable_type;
}

struct I {
  virtual ~I() = default;
};
struct J {
  virtual ~J() = default;
};
struct A : I, J {};

struct move_only {
  move_only() = default;
  move_only(const move_only &) = delete;
  move_only(move_only &&) = default;
};

struct copy_only {
  copy_only() = default;
  copy_only(const copy_only &) = default;
  copy_only(copy_only &&) = delete;
};

struct incompatible_deleter {
  void operator()(A *value) const { delete value; }
};

// spec: routes
TEST(next_engine, a_pointer_serves_the_object_it_points_to_and_its_copy) {
  using pointer = binding_t<external, A *>;
  static_assert(serves<pointer, A>());
  static_assert(serves<pointer, A &>());
  static_assert(serves<pointer, A *>());
  static_assert(serves<binding_t<shared, A *>, A>());

  using optional_pointer = binding_t<external, std::optional<A> *>;
  static_assert(serves<optional_pointer, std::optional<A>>());
  static_assert(serves<optional_pointer, std::optional<A> &>());
  static_assert(serves<optional_pointer, std::optional<A> *>());
  static_assert(serves<optional_pointer, A &>());
  static_assert(serves<optional_pointer, A *>());

  using array_pointer = binding_t<external, const std::array<A, 2> *>;
  static_assert(serves<array_pointer, std::array<A, 2>>());
  static_assert(serves<array_pointer, const std::array<A, 2> &>());
  static_assert(serves<array_pointer, const std::array<A, 2> *>());
  static_assert(!serves<array_pointer, std::array<A, 2> &>());
  static_assert(!serves<array_pointer, std::array<A, 2> *>());

  // The declared interfaces of the leaf an optional holds are reached through
  // the pointer.
  using interfaces = binding_t<external, std::optional<A> *, I, J>;
  static_assert(serves<interfaces, I &>());
  static_assert(serves<interfaces, J *>());
  SUCCEED();
}

TEST(next_engine, a_move_only_object_is_never_copied) {
  static_assert(!serves<binding_t<external, move_only *>, move_only>());
  static_assert(serves<binding_t<external, move_only *>, move_only *>());
  static_assert(!serves<binding_t<shared, move_only *>, move_only>());
  static_assert(
      !serves<binding_t<external, std::unique_ptr<move_only>>, move_only>());
  static_assert(serves<binding_t<external, std::unique_ptr<A>>, A>());
  static_assert(!serves<binding_t<shared, move_only>, move_only>());
  static_assert(serves<binding_t<shared, move_only>, move_only &>());

  using nested = binding_t<external, std::optional<move_only> *>;
  static_assert(!serves<nested, std::optional<move_only>>());
  static_assert(!serves<nested, move_only>());
  static_assert(serves<nested, std::optional<move_only> *>());
  using nested_reference =
      binding_t<external, const std::optional<move_only> &>;
  static_assert(!serves<nested_reference, move_only>());
  static_assert(!serves<nested_reference, std::optional<move_only>>());
  static_assert(serves<nested_reference, const move_only &>());

  // A sharing handle held by an alternative is copied, a sole-owner one is
  // not.
  using sharing =
      binding_t<shared,
                std::variant<std::shared_ptr<move_only>, std::monostate>>;
  static_assert(serves<sharing, std::shared_ptr<move_only>>());
  using owning =
      binding_t<shared,
                std::variant<std::unique_ptr<move_only>, std::monostate>>;
  static_assert(!serves<owning, std::unique_ptr<move_only>>());
  SUCCEED();
}

TEST(next_engine, a_consumed_object_is_moved_wrapped_or_adopted) {
  using value = binding_t<unique, move_only>;
  static_assert(serves<value, move_only>());
  static_assert(serves<value, move_only &&>());
  static_assert(serves<value, std::optional<move_only>>());

  using alternatives =
      binding_t<unique,
                std::variant<std::unique_ptr<move_only>, std::monostate>>;
  static_assert(serves<alternatives, std::unique_ptr<move_only>>());
  static_assert(serves<alternatives, std::unique_ptr<move_only> &&>());

  using pointer = binding_t<unique, std::optional<A> *>;
  static_assert(serves<pointer, std::unique_ptr<std::optional<A>>>());
  static_assert(serves<pointer, std::shared_ptr<std::optional<A>>>());
  static_assert(serves<pointer, std::unique_ptr<std::optional<A>> &&>());
  static_assert(serves<pointer, std::shared_ptr<std::optional<A>> &&>());
  static_assert(serves<pointer, std::optional<A> *>());
  static_assert(serves<binding_t<unique, int *>, std::shared_ptr<int>>());
  static_assert(!serves<binding_t<shared, int *>, std::shared_ptr<int>>());
  static_assert(
      !serves<binding_t<shared, int *>, std::optional<std::shared_ptr<int>>>());

  // A sole-owner handle converts to the handle of an interface.
  using handle = binding_t<unique, std::unique_ptr<A>, I>;
  static_assert(serves<handle, std::unique_ptr<I>>());
  static_assert(serves<handle, std::unique_ptr<I> &&>());
  SUCCEED();
}

// spec: behavior_changes.interface_bindings_serve_an_interface
TEST(next_engine, a_handle_that_converts_to_no_interface_is_rejected) {
  using incompatible =
      binding_t<unique, std::unique_ptr<A, incompatible_deleter>, I>;
  static_assert(incompatible::status ==
                core::registration_status::no_interface_served);
  SUCCEED();
}

TEST(next_engine, a_borrowing_scope_does_not_move_its_object) {
  using cyclical = binding_t<shared_cyclical, std::shared_ptr<A>>;
  static_assert(serves<cyclical, std::shared_ptr<A>>());
  static_assert(!serves<cyclical, std::shared_ptr<A> &&>());
  // A registration that declares the stored type as its interface.
  using named =
      binding_t<shared_cyclical, std::shared_ptr<A>, std::shared_ptr<A>>;
  static_assert(serves<named, std::shared_ptr<A>>());
  static_assert(!serves<named, std::shared_ptr<A> &&>());
  SUCCEED();
}

TEST(next_engine, a_wrapper_of_the_requested_type_is_not_wrapped_again) {
  using exact =
      binding_t<external, std::optional<copy_only> &, std::optional<copy_only>>;
  static_assert(serves<exact, std::optional<copy_only>>());
  static_assert(!serves<exact, std::optional<std::optional<copy_only>>>());
  SUCCEED();
}

TEST(next_engine, a_qualified_object_is_served_with_its_qualifiers) {
  using constant = binding_t<external, const int &>;
  static_assert(serves<constant, int>());
  static_assert(serves<constant, const int &>());
  static_assert(serves<constant, const int *>());
  static_assert(!serves<constant, int &>());
  static_assert(!serves<constant, int *>());
  static_assert(serves<binding_t<external, const std::optional<int> &>, int>());

  using qualified = binding_t<external, volatile int &>;
  static_assert(serves<qualified, int>());
  static_assert(serves<qualified, volatile int &>());
  static_assert(serves<qualified, const volatile int &>());
  static_assert(!serves<qualified, int &>());
  static_assert(!serves<qualified, const int &>());

  using alternatives = binding_t<external, const std::variant<int, float> &>;
  static_assert(serves<alternatives, const int &>());
  static_assert(serves<alternatives, const int *>());
  static_assert(!serves<alternatives, int &>());
  static_assert(!serves<alternatives, int *>());

  using volatile_alternatives =
      binding_t<shared, volatile std::variant<int, float>>;
  static_assert(!serves<volatile_alternatives, int &>());
  static_assert(serves<volatile_alternatives, volatile int &>());
  static_assert(
      serves<volatile_alternatives, volatile std::variant<int, float> &>());
  SUCCEED();
}

TEST(next_engine, an_array_of_arrays_serves_its_rows_and_itself) {
  using rows = binding_t<shared, int[2][3]>;
  static_assert(!serves<rows, int>());
  static_assert(!serves<rows, int *>());
  static_assert(serves<rows, int (&)[2][3]>());
  static_assert(serves<rows, int (*)[3]>());
  static_assert(serves<rows, int (*)[2][3]>());
  static_assert(serves<rows, const int (*)[3]>());
  static_assert(serves<rows, const volatile int (*)[2][3]>());
  static_assert(!serves<rows, int (*)[2]>());
  SUCCEED();
}

// spec: derive.qualifies, the qualification conversions of C++
TEST(next_engine, a_request_adds_qualifiers_only_below_const_levels) {
  using pointer = binding_t<external, std::optional<A> *>;
  static_assert(serves<pointer, std::optional<A> *const>());
  static_assert(serves<pointer, volatile std::optional<A> *>());
  static_assert(serves<pointer, const volatile std::optional<A> *>());
  static_assert(serves<pointer, const std::optional<A> &>());
  static_assert(serves<pointer, volatile std::optional<A> &>());
  using const_pointer = binding_t<external, const std::optional<A> *>;
  static_assert(serves<const_pointer, const std::optional<A> *>());
  static_assert(serves<const_pointer, const volatile std::optional<A> *>());
  static_assert(!serves<const_pointer, std::optional<A> *>());
  static_assert(!serves<const_pointer, volatile std::optional<A> *>());
  static_assert(!serves<const_pointer, std::optional<A> &>());
  using volatile_reference = binding_t<external, volatile std::optional<A> &>;
  static_assert(serves<volatile_reference, volatile std::optional<A> &>());
  static_assert(
      serves<volatile_reference, const volatile std::optional<A> &>());
  static_assert(!serves<volatile_reference, std::optional<A> &>());
  static_assert(!serves<volatile_reference, const std::optional<A> &>());

  // A level may gain const below levels that are all const, as A** converts
  // to const A *const * but not to const A **.
  static_assert(core::qualifies(core::type_tree_v<A **>,
                                core::type_tree_v<const A *const *>, false));
  static_assert(!core::qualifies(core::type_tree_v<A **>,
                                 core::type_tree_v<const A **>, false));
  using double_pointer = binding_t<external, A **>;
  static_assert(serves<double_pointer, const A *const *>());
  static_assert(!serves<double_pointer, const A **>());

  // The qualifiers of a value request are those of a copy.
  using value = binding_t<shared, A>;
  static_assert(serves<value, A>());
  static_assert(serves<value, const A>());
  static_assert(serves<value, volatile A>());
  static_assert(serves<value, const volatile A>());
  SUCCEED();
}

struct annotation_tag {};

// spec: request descriptor
TEST(next_engine, a_request_names_the_leaf_through_its_wrappers) {
  using core::innermost_leaf;
  using core::request_traits;
  using core::type_id_v;
  static_assert(
      innermost_leaf(request_traits<const std::shared_ptr<std::unique_ptr<A>>
                                        &>::descriptor.value) == type_id_v<A>);
  static_assert(
      innermost_leaf(
          request_traits<const volatile A *const volatile>::descriptor.value) ==
      type_id_v<A>);
  static_assert(
      innermost_leaf(
          request_traits<const A *const *volatile>::descriptor.value) ==
      type_id_v<A>);
  static_assert(
      innermost_leaf(request_traits<volatile A(*)[3]>::descriptor.value) ==
      type_id_v<A>);
  static_assert(innermost_leaf(
                    request_traits<const volatile A(&)[3]>::descriptor.value) ==
                type_id_v<A>);
  static_assert(
      innermost_leaf(
          request_traits<std::optional<const A *>>::descriptor.value) ==
      type_id_v<A>);
  // A request annotated by a tag names the annotated interface.
  using annotated_key = core::annotated_key<annotation_tag, core::no_key>;
  static_assert(
      innermost_leaf(request_traits<I &, annotated_key>::descriptor.value) ==
      type_id_v<dingo::annotated<I, annotation_tag>>);
  static_assert(
      std::is_same_v<
          core::leaf_of_t<const std::shared_ptr<std::unique_ptr<A>> &>, A>);
  static_assert(std::is_same_v<core::leaf_of_t<std::optional<const A *>>, A>);

  // A reference request refers to an object the binding keeps; a value, an
  // rvalue reference, and a pointer request do not.
  static_assert(request_traits<A &>::kind ==
                core::request_form::lvalue_reference);
  static_assert(request_traits<const A &>::kind ==
                core::request_form::lvalue_reference);
  static_assert(request_traits<A &&>::kind ==
                core::request_form::rvalue_reference);
  static_assert(request_traits<A>::kind == core::request_form::value);
  static_assert(request_traits<A *>::kind == core::request_form::value);
  static_assert(request_traits<const A *>::kind == core::request_form::value);
  static_assert(std::is_same_v<request_traits<const A *>::made_type, A *>);
  SUCCEED();
}

// spec: behavior_changes.interfaces_do_not_rebind_storage
TEST(next_engine, interfaces_do_not_rebind_the_stored_type_tree) {
  using one = binding_t<shared, std::shared_ptr<A>, I>;
  static_assert(std::is_same_v<one::stored_type, std::shared_ptr<A>>);
  static_assert(one::descriptor.stored ==
                core::type_tree_v<std::shared_ptr<A>>);
  static_assert(serves<one, I &>());
  static_assert(serves<one, std::shared_ptr<I>>());
  static_assert(serves<one, std::shared_ptr<I> &>());

  using two = binding_t<shared, std::shared_ptr<A>, I, J>;
  static_assert(two::descriptor.stored == one::descriptor.stored);
  static_assert(two::descriptor.interface_count == 2);
  static_assert(core::serves_interface(two::descriptor, core::type_id_v<I>));
  static_assert(core::serves_interface(two::descriptor, core::type_id_v<J>));
  static_assert(!core::serves_interface(two::descriptor, core::type_id_v<A>));
  static_assert(serves<two, I &>());
  static_assert(serves<two, J &>());
  static_assert(serves<two, std::shared_ptr<J>>());

  // A sole-owner handle is not rebound to one interface either: a reference
  // to a handle of the interface is not served, with any number of them.
  using owner = binding_t<shared, std::unique_ptr<A>, I>;
  static_assert(serves<owner, I &>());
  static_assert(!serves<owner, std::unique_ptr<I> &>());
  static_assert(!serves<binding_t<shared, std::unique_ptr<A>, I, J>,
                        std::unique_ptr<I> &>());

  using nested = binding_t<shared, std::shared_ptr<std::unique_ptr<A>>, I>;
  static_assert(
      std::is_same_v<nested::stored_type, std::shared_ptr<std::unique_ptr<A>>>);
  static_assert(serves<nested, I &>());
  SUCCEED();
}

TEST(next_engine, a_binding_keeps_objects_only_for_its_cached_routes) {
  using plain = binding_t<shared, A>;
  static_assert(!plain::keeps);
  static_assert(std::is_same_v<plain::context_type, core::no_kept_objects>);
  static_assert(std::is_empty_v<core::no_kept_objects>);
  static_assert(std::is_trivially_destructible_v<core::no_kept_objects>);
  // A sharing handle converted to the handle of an interface is kept, so a
  // reference to it can be served.
  using converted = binding_t<shared, std::shared_ptr<A>, I>;
  static_assert(converted::keeps);
  static_assert(!binding_t<external, A &>::keeps);
  SUCCEED();
}

struct borrowed_source {};
struct borrowed_target {};
struct consumed_source {};
struct consumed_target {};

struct owning_source {
  explicit owning_source(int initial_value)
      : value(std::make_unique<int>(initial_value)) {}
  std::unique_ptr<int> value;
};

// Constructible from a mutable and a const source; the first steals it.
struct copying_target {
  explicit copying_target(owning_source &source)
      : value(*source.value), used_const_source(false) {
    source.value.reset();
  }
  explicit copying_target(const owning_source &source)
      : value(*source.value), used_const_source(true) {}
  int value;
  bool used_const_source;
};

struct consuming_target {
  explicit consuming_target(owning_source &&source)
      : value(std::move(source.value)) {}
  std::unique_ptr<int> value;
};

struct customized_value {
  int value;
};

struct observer {
  explicit observer(long *value) : pointer(value) {}
  long *pointer;
};

using observed = std::variant<std::shared_ptr<long>, observer>;

} // namespace

namespace dingo {
template <> struct type_conversion_traits<borrowed_target, borrowed_source> {
  template <typename> using required_access = borrow;
  static borrowed_target convert(const borrowed_source &) { return {}; }
};

template <> struct type_conversion_traits<consumed_target, consumed_source> {
  template <typename> using required_access = consume;
  static consumed_target convert(consumed_source &&) { return {}; }
};

template <> struct type_conversion_traits<customized_value, customized_value> {
  template <typename> using required_access = borrow;
  static customized_value convert(const customized_value &source) {
    return {source.value + 1};
  }
};

template <> struct type_conversion_traits<observed, long *> {
  template <typename> using required_access = borrow;
  static observer convert(long *pointer) { return observer(pointer); }
};
} // namespace dingo

namespace {

// spec: public_api.conversion_traits
TEST(next_engine, a_conversion_needs_the_access_it_declares) {
  using core::converts;
  static_assert(
      core::declares_conversion<borrowed_target, borrowed_source>::value);
  static_assert(
      converts<borrowed_target, borrowed_source, const borrowed_source &>());
  static_assert(
      converts<borrowed_target, borrowed_source, borrowed_source &&>());
  static_assert(
      core::declares_conversion<consumed_target, consumed_source>::value);
  static_assert(
      converts<consumed_target, consumed_source, consumed_source &&>());
  static_assert(
      !converts<consumed_target, consumed_source, consumed_source &>());
  static_assert(
      !converts<consumed_target, consumed_source, const consumed_source &>());

  // Without a declaration, the converting constructor decides: a borrowed
  // source is const, a consumed one an rvalue.
  static_assert(
      !core::declares_conversion<copying_target, owning_source>::value);
  static_assert(
      converts<copying_target, owning_source, const owning_source &>());
  static_assert(
      !converts<consuming_target, owning_source, const owning_source &>());
  static_assert(converts<consuming_target, owning_source, owning_source &&>());
  static_assert(!converts<std::shared_ptr<int>, std::unique_ptr<int>,
                          const std::unique_ptr<int> &>());
  static_assert(converts<std::shared_ptr<int>, std::unique_ptr<int>,
                         std::unique_ptr<int> &&>());
  static_assert(
      !converts<std::unique_ptr<I>, std::unique_ptr<A, incompatible_deleter>,
                std::unique_ptr<A, incompatible_deleter> &&>());
  static_assert(converts<std::unique_ptr<I>, std::unique_ptr<A>,
                         std::unique_ptr<A> &&>());
  SUCCEED();
}

TEST(next_engine, a_borrowed_conversion_leaves_its_source) {
  owning_source source(17);
  copying_target borrowed =
      core::convert_to<copying_target>(std::as_const(source));
  EXPECT_TRUE(borrowed.used_const_source);
  EXPECT_EQ(borrowed.value, 17);
  EXPECT_NE(source.value, nullptr);

  consuming_target consumed =
      core::convert_to<consuming_target>(std::move(source));
  ASSERT_NE(consumed.value, nullptr);
  EXPECT_EQ(*consumed.value, 17);
  EXPECT_EQ(source.value, nullptr);
}

TEST(next_engine, a_declared_conversion_is_used_for_its_target) {
  // Also when the target is the source type.
  customized_value value{17};
  EXPECT_EQ(core::convert_to<customized_value>(std::as_const(value)).value, 18);

  // A conversion may return an object the target is constructed from.
  long target = 17;
  long *pointer = &target;
  observed converted = core::convert_to<observed>(pointer);
  ASSERT_TRUE(std::holds_alternative<observer>(converted));
  EXPECT_EQ(std::get<observer>(converted).pointer, &target);
}

} // namespace
