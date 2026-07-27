//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "type_registration_common.h"

#include "support/custom_wrappers.h"
#include "support/resolution_test_types.h"

#include <array>
#include <memory>
#include <optional>
#include <variant>

namespace conversion_compatibility_test {
struct source {};
struct target {};
struct borrowed_value {};
struct non_borrowable_wrapper {};
struct declared_source {};
struct declared_target {};
struct owning_source {
  explicit owning_source(int initial_value)
      : value(std::make_unique<int>(initial_value)) {}

  std::unique_ptr<int> value;
};
struct copying_from_owner {
  explicit copying_from_owner(owning_source &source)
      : value(*source.value), used_const_source(false) {
    source.value.reset();
  }

  explicit copying_from_owner(const owning_source &source)
      : value(*source.value), used_const_source(true) {}

  int value;
  bool used_const_source;
};
struct consuming_from_owner {
  explicit consuming_from_owner(owning_source &source)
      : value(std::move(source.value)) {}

  std::unique_ptr<int> value;
};
struct pointer_like {
  pointer_like(int *);
};
struct observer_pointer {
  observer_pointer(int *);
};
struct converted_observer_pointer {
  converted_observer_pointer(int *);
};
struct safe_observer_pointer {
  explicit safe_observer_pointer(long *value) : pointer(value) {}

  long *pointer;
};
struct consuming_owner {
  explicit consuming_owner(int *);
};
struct copying_owner {
  explicit copying_owner(int *);
};
struct array_base {};
struct array_derived : array_base {};
using converted_variant =
    std::variant<std::shared_ptr<int>, converted_observer_pointer>;
using safe_converted_variant =
    std::variant<std::shared_ptr<long>, safe_observer_pointer>;
} // namespace conversion_compatibility_test

namespace dingo {
template <>
struct type_conversion_traits<conversion_compatibility_test::target,
                              conversion_compatibility_test::source> {
  template <typename> using required_access = borrow;

  static conversion_compatibility_test::target
  convert(const conversion_compatibility_test::source &) {
    return {};
  }
};

template <>
struct type_conversion_traits<conversion_compatibility_test::declared_target,
                              conversion_compatibility_test::declared_source> {
  template <typename> using required_access = consume;

  static conversion_compatibility_test::declared_target
  convert(conversion_compatibility_test::declared_source &&) {
    return {};
  }
};

template <>
struct type_traits<conversion_compatibility_test::non_borrowable_wrapper> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = false;
  static constexpr bool is_value_borrowable = false;
  static constexpr bool is_owning_handle = false;

  template <typename> static constexpr bool is_handle_rebindable = false;
  template <typename> static constexpr bool is_rebindable = false;

  static conversion_compatibility_test::borrowed_value &
  borrow(conversion_compatibility_test::non_borrowable_wrapper &);
};

template <> struct type_traits<conversion_compatibility_test::owning_source> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = false;
  static constexpr bool is_value_borrowable = false;
  static constexpr bool is_owning_handle = true;

  template <typename> static constexpr bool is_handle_rebindable = false;
  template <typename> static constexpr bool is_rebindable = false;
};

template <> struct type_traits<conversion_compatibility_test::pointer_like> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = false;
  static constexpr bool is_owning_handle = true;

  using value_type = int;

  template <typename>
  using rebind_t = conversion_compatibility_test::pointer_like;
  template <typename> static constexpr bool is_handle_rebindable = false;
  template <typename> static constexpr bool is_rebindable = false;
};

template <>
struct type_traits<conversion_compatibility_test::observer_pointer> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = false;
  static constexpr bool is_owning_handle = false;

  using value_type = int;

  template <typename>
  using rebind_t = conversion_compatibility_test::observer_pointer;
  template <typename> static constexpr bool is_handle_rebindable = false;
  template <typename> static constexpr bool is_rebindable = false;
};

template <>
struct type_traits<conversion_compatibility_test::converted_observer_pointer> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = false;
  static constexpr bool is_owning_handle = false;

  using value_type = int;

  template <typename>
  using rebind_t = conversion_compatibility_test::converted_observer_pointer;
  template <typename> static constexpr bool is_handle_rebindable = false;
  template <typename> static constexpr bool is_rebindable = false;
};

template <>
struct type_traits<conversion_compatibility_test::safe_observer_pointer> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = false;
  static constexpr bool is_owning_handle = false;

  using value_type = long;

  template <typename>
  using rebind_t = conversion_compatibility_test::safe_observer_pointer;
  template <typename> static constexpr bool is_handle_rebindable = false;
  template <typename> static constexpr bool is_rebindable = false;
};

template <> struct type_traits<conversion_compatibility_test::consuming_owner> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = false;
  static constexpr bool is_owning_handle = true;

  using value_type = int;

  template <typename>
  using rebind_t = conversion_compatibility_test::consuming_owner;
  template <typename> static constexpr bool is_handle_rebindable = false;
  template <typename> static constexpr bool is_rebindable = false;
};

template <> struct type_traits<conversion_compatibility_test::copying_owner> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = false;
  static constexpr bool is_owning_handle = true;

  using value_type = int;

  template <typename>
  using rebind_t = conversion_compatibility_test::copying_owner;
  template <typename> static constexpr bool is_handle_rebindable = false;
  template <typename> static constexpr bool is_rebindable = false;
};

template <>
struct type_conversion_traits<conversion_compatibility_test::consuming_owner,
                              int *> {
  template <typename> using required_access = consume;

  static conversion_compatibility_test::consuming_owner convert(int *);
};

template <>
struct type_conversion_traits<conversion_compatibility_test::copying_owner,
                              int *> {
  template <typename> using required_access = borrow;

  static conversion_compatibility_test::copying_owner convert(int *);
};

template <>
struct type_conversion_traits<conversion_compatibility_test::converted_variant,
                              int *> {
  template <typename> using required_access = consume;

  static std::shared_ptr<int> convert(int *);
};

template <>
struct type_conversion_traits<
    conversion_compatibility_test::safe_converted_variant, long *> {
  template <typename> using required_access = borrow;

  static conversion_compatibility_test::safe_observer_pointer
  convert(long *pointer) {
    return conversion_compatibility_test::safe_observer_pointer(pointer);
  }
};
} // namespace dingo

namespace {
template <typename Resolutions> struct resolution_targets;

template <typename... Resolutions>
struct resolution_targets<type_list<Resolutions...>> {
  using type = type_list<typename Resolutions::target_type...>;
};

template <typename Resolutions>
using resolution_targets_t = typename resolution_targets<Resolutions>::type;

template <typename Target, typename Resolutions>
inline constexpr bool has_resolution_target_v =
    type_list_contains_v<Target, resolution_targets_t<Resolutions>>;

template <typename Type> struct fresh_value_storage {
  using type = Type;

  struct conversions {
    static constexpr bool is_stable = true;

    using value_types = type_list<>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<runtime_type &&>;
    using pointer_types = type_list<>;
  };
};
} // namespace

TEST(type_registration_test,
     runtime_local_bindings_declare_the_registration_container) {
  using root_container = dingo::container<>;
  using empty_local_registration =
      type_registration<scope<shared>, storage<runtime_binding_local_service>,
                        dingo::bindings<>>;
  using empty_local_model = detail::binding_model<empty_local_registration>;
  static_assert(std::is_void_v<typename empty_local_model::bindings_type>);
  using empty_proxy =
      decltype(std::declval<root_container &>()
                   .template register_type<
                       scope<shared>, storage<runtime_binding_local_service>,
                       dingo::bindings<>>());
  using empty_container = typename empty_proxy::container_type;
  using empty_runtime_state = detail::runtime_binding_state_t<
      typename root_container::registry_type, empty_container,
      typename empty_local_model::storage_type,
      typename empty_local_model::bindings_type>;
  static_assert(std::is_same_v<typename empty_runtime_state::container_type,
                               empty_container>);
  static_assert(
      std::is_same_v<typename empty_runtime_state::resolution_container_type,
                     empty_container>);

  using non_empty_local_registration = type_registration<
      scope<shared>, storage<runtime_binding_local_service>,
      dingo::bindings<dingo::bind<
          scope<shared>, storage<runtime_binding_local_dependency>,
          factory<function<make_runtime_binding_local_dependency>>>>>;
  using non_empty_local_model =
      detail::binding_model<non_empty_local_registration>;
  static_assert(!std::is_void_v<typename non_empty_local_model::bindings_type>);
  using non_empty_proxy =
      decltype(std::declval<root_container &>()
                   .template register_type<
                       scope<shared>, storage<runtime_binding_local_service>,
                       dingo::bindings<dingo::bind<
                           scope<shared>,
                           storage<runtime_binding_local_dependency>,
                           factory<function<
                               make_runtime_binding_local_dependency>>>>>());
  using non_empty_container = typename non_empty_proxy::container_type;
  using non_empty_runtime_state = detail::runtime_binding_state_t<
      typename root_container::registry_type, non_empty_container,
      typename non_empty_local_model::storage_type,
      typename non_empty_local_model::bindings_type>;
  static_assert(!std::is_same_v<non_empty_container, empty_container>);
  static_assert(
      std::is_same_v<typename non_empty_container::static_bindings_type,
                     typename non_empty_local_model::bindings_type>);
  static_assert(std::is_same_v<typename non_empty_runtime_state::container_type,
                               non_empty_container>);
  static_assert(std::is_same_v<
                typename non_empty_runtime_state::resolution_container_type,
                non_empty_container>);

  dingo::container<> container;
  container.register_type<
      scope<shared>, storage<runtime_binding_local_service>,
      dependencies<runtime_binding_local_dependency &>,
      dingo::bindings<dingo::bind<
          scope<shared>, storage<runtime_binding_local_dependency>,
          factory<function<make_runtime_binding_local_dependency>>>>>();

  EXPECT_EQ(container.resolve<runtime_binding_local_service &>().value, 7);
}

TEST(type_registration_test, conversion_availability_uses_source_access) {
  static_assert(
      detail::access_satisfies<borrow,
                               detail::alternative_access<
                                   borrow, detail::unavailable_access>>::value);
  static_assert(!detail::access_satisfies<
                borrow, detail::alternative_access<borrow, consume>>::value);
  static_assert(detail::access_satisfies<
                consume, detail::alternative_access<borrow, consume>>::value);
  static_assert(detail::is_type_conversion_available_v<
                conversion_compatibility_test::target,
                const conversion_compatibility_test::source &>);
  static_assert(!detail::is_type_conversion_available_v<
                conversion_compatibility_test::borrowed_value,
                conversion_compatibility_test::non_borrowable_wrapper &>);
  static_assert(detail::is_type_conversion_available_v<
                conversion_compatibility_test::declared_target,
                conversion_compatibility_test::declared_source &&>);
  static_assert(!detail::is_type_conversion_available_v<
                conversion_compatibility_test::declared_target,
                conversion_compatibility_test::declared_source &>);
  static_assert(detail::is_type_conversion_available_v<
                conversion_compatibility_test::copying_from_owner,
                conversion_compatibility_test::owning_source &, borrow>);
  using borrowed_owner_conversion = detail::type_conversion_path_t<
      conversion_compatibility_test::copying_from_owner,
      conversion_compatibility_test::owning_source &, borrow>;
  static_assert(
      std::is_same_v<typename borrowed_owner_conversion::argument_type,
                     const conversion_compatibility_test::owning_source &>);
  static_assert(!detail::is_type_conversion_available_v<
                conversion_compatibility_test::consuming_from_owner,
                conversion_compatibility_test::owning_source &, borrow>);
  static_assert(detail::is_type_conversion_available_v<
                conversion_compatibility_test::consuming_from_owner,
                conversion_compatibility_test::owning_source &, consume>);
  static_assert(!detail::is_type_conversion_available_v<int, const void *>);
  static_assert(detail::is_type_conversion_available_v<
                conversion_compatibility_test::pointer_like, int *, consume>);
  static_assert(!detail::is_type_conversion_available_v<
                conversion_compatibility_test::pointer_like, int *, borrow>);
  static_assert(
      detail::is_type_conversion_available_v<
          conversion_compatibility_test::observer_pointer, int *, borrow>);
  static_assert(
      detail::is_type_conversion_available_v<
          conversion_compatibility_test::consuming_owner, int *, consume>);
  static_assert(!detail::is_type_conversion_available_v<
                conversion_compatibility_test::consuming_owner, int *, borrow>);
  using consuming_conversion = detail::type_conversion_path_t<
      conversion_compatibility_test::consuming_owner, int *, consume>;
  static_assert(
      std::is_same_v<typename consuming_conversion::required_access, consume>);
  static_assert(detail::is_type_conversion_available_v<
                conversion_compatibility_test::copying_owner, int *, borrow>);
  using copying_conversion = detail::type_conversion_path_t<
      conversion_compatibility_test::copying_owner, int *, borrow>;
  static_assert(
      std::is_same_v<typename copying_conversion::required_access, borrow>);
  static_assert(
      detail::is_type_conversion_available_v<
          conversion_compatibility_test::converted_variant, int *, consume>);
  static_assert(
      !detail::is_type_conversion_available_v<
          conversion_compatibility_test::converted_variant, int *, borrow>);
  static_assert(detail::is_type_conversion_available_v<
                conversion_compatibility_test::safe_converted_variant, long *,
                borrow>);
  static_assert(
      !detail::is_type_conversion_available_v<
          test_optional<std::shared_ptr<int>>, test_optional<int *> &, borrow>);
  static_assert(detail::is_type_conversion_available_v<
                test_optional<std::shared_ptr<int>>, test_optional<int *> &&,
                consume>);
  using move_optional = test_optional<std::unique_ptr<int>>;
  static_assert(
      detail::is_type_conversion_available_v<move_optional, move_optional &&,
                                             consume>);
  static_assert(
      !detail::is_type_conversion_available_v<move_optional,
                                              const move_optional &&, consume>);
  static_assert(!detail::is_type_conversion_available_v<
                move_optional, volatile move_optional &, borrow>);
  static_assert(detail::is_type_conversion_available_v<std::shared_ptr<int>,
                                                       int *, consume>);
  static_assert(!detail::is_type_conversion_available_v<std::shared_ptr<int>,
                                                        int *, borrow>);
  static_assert(!detail::is_type_conversion_available_v<std::shared_ptr<int> &,
                                                        int *, borrow>);
  static_assert(!detail::is_type_conversion_available_v<
                std::optional<std::shared_ptr<int>>, int *, borrow>);
  static_assert(!detail::is_type_conversion_available_v<
                std::optional<std::unique_ptr<int>>, int *, borrow>);
  using shared_optional = std::optional<std::shared_ptr<int>>;
  using const_shared_optional = std::optional<std::shared_ptr<const int>>;
  static_assert(
      detail::is_type_conversion_available_v<const_shared_optional,
                                             shared_optional &, borrow>);
  static_assert(detail::is_type_conversion_available_v<
                const_shared_optional, std::optional<std::unique_ptr<int>> &&,
                consume>);
  using shared_optional_conversion =
      detail::type_conversion_path_t<const_shared_optional, shared_optional &,
                                     borrow>;
  static_assert(
      std::is_same_v<typename shared_optional_conversion::required_access,
                     borrow>);
  using owning_variant =
      std::variant<conversion_compatibility_test::pointer_like, int>;
  static_assert(std::is_constructible_v<owning_variant, int *>);
  static_assert(
      detail::is_type_conversion_available_v<owning_variant, int *, consume>);
  static_assert(
      detail::is_type_conversion_available_v<owning_variant, int *, borrow>);
  static_assert(!std::is_same_v<
                detail::type_conversion_path_t<owning_variant, int *, borrow>,
                detail::traits_type_conversion<owning_variant, int *>>);
  using nested_owning_variant = std::optional<owning_variant>;
  static_assert(std::is_constructible_v<nested_owning_variant, int *>);
  static_assert(detail::is_type_conversion_available_v<nested_owning_variant,
                                                       int *, consume>);
  static_assert(detail::is_type_conversion_available_v<nested_owning_variant,
                                                       int *, borrow>);
  using nested_borrowed_conversion =
      detail::type_conversion_path_t<nested_owning_variant, int *, borrow>;
  static_assert(
      std::is_same_v<typename nested_borrowed_conversion::required_access,
                     borrow>);
  using observer_variant =
      std::variant<conversion_compatibility_test::observer_pointer, int>;
  static_assert(std::is_constructible_v<observer_variant, int *>);
  static_assert(
      detail::is_type_conversion_available_v<observer_variant, int *, borrow>);
  using mixed_variant =
      std::variant<std::shared_ptr<int>,
                   conversion_compatibility_test::observer_pointer>;
  static_assert(std::is_constructible_v<mixed_variant, int *>);
  static_assert(
      detail::is_type_conversion_available_v<mixed_variant, int *, borrow>);
  static_assert(
      detail::is_type_conversion_available_v<mixed_variant &, mixed_variant &,
                                             borrow>);
  static_assert(
      detail::is_type_conversion_available_v<const mixed_variant &,
                                             mixed_variant &, borrow>);
  using mixed_variant_conversion = detail::target_alternative_type_conversion<
      mixed_variant, int *, conversion_compatibility_test::observer_pointer,
      detail::traits_type_conversion<
          conversion_compatibility_test::observer_pointer, int *, borrow,
          int *>>;
  static_assert(std::is_same_v<
                detail::type_conversion_path_t<mixed_variant, int *, borrow>,
                detail::converted_construction<mixed_variant_conversion>>);
  using nested_mixed_variant = std::optional<mixed_variant>;
  static_assert(std::is_constructible_v<nested_mixed_variant, int *>);
  static_assert(detail::is_type_conversion_available_v<nested_mixed_variant,
                                                       int *, borrow>);
  static_assert(
      std::is_same_v<
          detail::type_conversion_path_t<nested_mixed_variant, int *, borrow>,
          detail::converted_construction<detail::wrapper_type_conversion<
              nested_mixed_variant, int *, mixed_variant_conversion, false>>>);
  static_assert(
      detail::is_type_conversion_available_v<int (*)[2][3], int (*)[3]>);
  static_assert(
      !detail::is_type_conversion_available_v<int (*)[4], int (*)[3]>);
  static_assert(
      !detail::is_type_conversion_available_v<int (*)[3], volatile int (*)[3]>);
  static_assert(
      detail::is_type_conversion_available_v<const volatile int (*)[2][3],
                                             int (*)[3]>);
  static_assert(!detail::is_type_conversion_available_v<
                conversion_compatibility_test::array_base(*)[2],
                conversion_compatibility_test::array_derived *>);
  static_assert(!detail::is_type_conversion_available_v<
                conversion_compatibility_test::array_base(&)[2],
                conversion_compatibility_test::array_derived *>);
  using volatile_variant = volatile std::variant<int, float>;
  static_assert(
      !detail::is_type_conversion_available_v<int &, volatile_variant &>);
  static_assert(detail::is_type_conversion_available_v<volatile_variant &,
                                                       volatile_variant &>);

  using volatile_model = detail::binding_model<
      type_registration<scope<external>, storage<volatile int &>>>;
  using volatile_binding = detail::binding<int, volatile_model>;
  static_assert(detail::binding_supports_request_v<int, volatile_binding>);
  static_assert(
      detail::binding_supports_request_v<volatile int &, volatile_binding>);
  static_assert(detail::binding_supports_request_v<const volatile int &,
                                                   volatile_binding>);
  static_assert(!detail::binding_supports_request_v<int &, volatile_binding>);
  static_assert(
      !detail::binding_supports_request_v<const int &, volatile_binding>);
}

TEST(type_registration_test, inferred_borrowed_conversion_uses_const_source) {
  using source = conversion_compatibility_test::owning_source;
  using target = conversion_compatibility_test::copying_from_owner;
  using conversion = detail::type_conversion_path_t<target, source &, borrow>;
  struct conversion_context {};

  source value(17);
  conversion_context resolver;
  conversion_context context;
  auto result = detail::type_conversion<conversion>::apply(
      resolver, context, value, describe_type<target>(),
      describe_type<source>());

  EXPECT_TRUE(result.used_const_source);
  EXPECT_EQ(result.value, 17);
  EXPECT_NE(value.value, nullptr);
}

TEST(type_registration_test,
     borrowed_optional_conversion_uses_const_contained_value) {
  using source = test_optional<conversion_compatibility_test::owning_source>;
  using target =
      test_optional<conversion_compatibility_test::copying_from_owner>;
  using conversion = detail::type_conversion_path_t<target, source &, borrow>;
  struct conversion_context {};

  source value(conversion_compatibility_test::owning_source(19));
  conversion_context resolver;
  conversion_context context;
  auto result = detail::type_conversion<conversion>::apply(
      resolver, context, value, describe_type<target>(),
      describe_type<source>());

  ASSERT_NE(result.get(), nullptr);
  EXPECT_TRUE(result.get()->used_const_source);
  EXPECT_EQ(result.get()->value, 19);
  ASSERT_NE(value.get(), nullptr);
  EXPECT_NE(value.get()->value, nullptr);
}

TEST(type_registration_test,
     inferred_mutable_lvalue_conversion_consumes_source) {
  using source = conversion_compatibility_test::owning_source;
  using target = conversion_compatibility_test::consuming_from_owner;
  using conversion = detail::type_conversion_path_t<target, source &, consume>;
  struct conversion_context {};

  source value(23);
  conversion_context resolver;
  conversion_context context;
  auto result = detail::type_conversion<conversion>::apply(
      resolver, context, value, describe_type<target>(),
      describe_type<source>());

  ASSERT_NE(result.value, nullptr);
  EXPECT_EQ(*result.value, 23);
  EXPECT_EQ(value.value, nullptr);
}

TEST(type_registration_test,
     borrowed_custom_conversion_executes_non_owning_result) {
  using target = conversion_compatibility_test::safe_converted_variant;
  using conversion = detail::type_conversion_path_t<target, long *, borrow>;
  struct conversion_context {};

  long value = 17;
  long *source = &value;
  conversion_context resolver;
  conversion_context context;
  auto result = detail::type_conversion<conversion>::apply(
      resolver, context, source, describe_type<target>(),
      describe_type<long *>());

  EXPECT_EQ(
      std::get<conversion_compatibility_test::safe_observer_pointer>(result)
          .pointer,
      &value);
}

TEST(type_registration_test,
     borrowed_storage_publishes_fresh_move_only_conversion_results) {
  using namespace resolution_test;
  using registration =
      type_registration<scope<external>, storage<move_source *>,
                        interfaces<move_target, secondary>>;
  using model = detail::binding_model<registration>;
  using resolutions =
      detail::binding_resolutions<move_target, typename model::storage_type>;
  using binding = detail::binding<move_target, model>;

  static_assert(
      has_resolution_target_v<move_target,
                              typename resolutions::value_resolutions>);
  static_assert(std::is_same_v<typename detail::binding_supports_request<
                                   move_target, binding>::type::target_type,
                               move_target>);
}

TEST(type_registration_test,
     borrowed_storage_publishes_copyable_custom_value_conversion) {
  using namespace resolution_test;
  static_assert(std::is_same_v<
                detail::type_conversion_path_t<copy_target, copy_source &>,
                detail::traits_type_conversion<copy_target, copy_source &>>);

  using registration =
      type_registration<scope<external>, storage<copy_source *>,
                        interfaces<copy_target, secondary>>;
  using model = detail::binding_model<registration>;
  using resolutions =
      detail::binding_resolutions<copy_target, typename model::storage_type>;

  static_assert(
      has_resolution_target_v<copy_target,
                              typename resolutions::value_resolutions>);
}

TEST(type_registration_test, consumed_binding_executes_custom_type_conversion) {
  struct service_interface {
    virtual ~service_interface() = default;
    virtual int value() const = 0;
  };
  struct implementation : service_interface {
    int value() const override { return 7; }
  };

  container<> container;
  container.register_type<scope<unique>, storage<test_shared<implementation>>,
                          interfaces<service_interface>>();

  auto resolved = container.resolve<test_shared<service_interface>>();
  ASSERT_NE(resolved.get(), nullptr);
  EXPECT_EQ(resolved.get()->value(), 7);
}

TEST(type_registration_test,
     binding_model_rewrites_single_interface_storage_leaf) {
  struct I {
    virtual ~I() = default;
  };
  struct A : I {};

  using registration =
      type_registration<scope<shared>, storage<std::shared_ptr<A>>,
                        interfaces<I>>;
  using model = detail::binding_model<registration>;

  static_assert(model::storage_tag_is_complete);
  static_assert(model::use_interface_as_stored_leaf);
  static_assert(std::is_same_v<typename model::stored_leaf_type, I>);
  static_assert(
      std::is_same_v<typename model::stored_type, std::shared_ptr<I>>);
  static_assert(std::is_same_v<typename model::storage_type::stored_type,
                               std::shared_ptr<I>>);
  static_assert(model::valid);
}

TEST(type_registration_test,
     binding_model_preserves_storage_for_multi_interface_registration) {
  struct I {
    virtual ~I() = default;
  };
  struct J {
    virtual ~J() = default;
  };
  struct A : I, J {};

  using registration =
      type_registration<scope<shared>, storage<std::shared_ptr<A>>,
                        interfaces<I, J>>;
  using model = detail::binding_model<registration>;
  using expansion = detail::binding_expansion<model>;

  static_assert(model::storage_tag_is_complete);
  static_assert(!model::use_interface_as_stored_leaf);
  static_assert(std::is_same_v<typename model::stored_leaf_type, A>);
  static_assert(
      std::is_same_v<typename model::stored_type, std::shared_ptr<A>>);
  static_assert(
      std::is_same_v<
          typename expansion::interface_bindings,
          type_list<detail::binding<I, model>, detail::binding<J, model>>>);
  static_assert(model::valid);
}

TEST(type_registration_test, recursive_leaf_and_rebind_traits) {
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
  struct immovable {
    immovable() = default;
    immovable(const immovable &) = delete;
    immovable(immovable &&) = delete;
  };
  struct incompatible_deleter {
    void operator()(A *value) const { delete value; }
  };

  static_assert(detail::is_type_conversion_available_v<std::unique_ptr<I>,
                                                       std::unique_ptr<A> &&>);
  static_assert(
      !detail::is_type_conversion_available_v<
          std::unique_ptr<I>, std::unique_ptr<A, incompatible_deleter> &&>);

  using nested_handle = const std::shared_ptr<std::unique_ptr<A>> &;
  using nested_list = type_list<nested_handle, std::unique_ptr<A *>>;

  static_assert(std::is_same_v<leaf_type_t<nested_handle>, A>);
  static_assert(std::is_same_v<rebind_leaf_t<nested_handle, I>,
                               std::shared_ptr<std::unique_ptr<I>> &>);
  static_assert(
      std::is_same_v<rebind_type_t<nested_handle, I>, std::shared_ptr<I> &>);
  static_assert(std::is_same_v<leaf_type_t<nested_list>, type_list<A, A>>);
  static_assert(std::is_same_v<rebind_leaf_t<nested_list, I>,
                               type_list<std::shared_ptr<std::unique_ptr<I>> &,
                                         std::unique_ptr<I *>>>);
  static_assert(std::is_same_v<leaf_type_t<std::optional<const A *>>, A>);
  static_assert(std::is_same_v<rebind_leaf_t<const A *, I>, const I *>);
  static_assert(std::is_same_v<rebind_leaf_t<std::optional<const A *>, I>,
                               std::optional<const I *>>);
  using nested_array_handle =
      std::shared_ptr<std::unique_ptr<A[], std::default_delete<A[]>>>;
  static_assert(
      detail::is_type_conversion_available_v<A *, nested_array_handle &>);
  static_assert(
      std::is_same_v<request_lookup_type_t<const A *>, runtime_type *>);
  static_assert(
      std::is_same_v<request_lookup_type_t<const volatile A *const volatile>,
                     runtime_type *>);
  static_assert(
      std::is_same_v<request_lookup_type_t<const A *const *>, runtime_type **>);
  static_assert(std::is_same_v<request_lookup_type_t<std::optional<const A *>>,
                               std::optional<const runtime_type *>>);
  using exact_const_optional_pointer = exact_lookup<const std::optional<A>> *;
  static_assert(std::is_same_v<lookup_type_t<exact_const_optional_pointer>,
                               std::optional<runtime_type> *>);
  static_assert(std::is_same_v<resolved_type_t<exact_const_optional_pointer,
                                               const std::optional<A> *>,
                               const std::optional<A> *>);
  using exact_optional_value = exact_lookup<std::optional<A>>;
  static_assert(std::is_same_v<lookup_type_t<exact_optional_value>,
                               std::optional<runtime_type>>);
  using exact_optional_reference = exact_lookup<std::optional<A>> &;
  static_assert(std::is_same_v<lookup_type_t<exact_optional_reference>,
                               std::optional<runtime_type> &>);
  using exact_optional_unique_rvalue =
      exact_lookup<std::unique_ptr<std::optional<A>>> &&;
  static_assert(
      std::is_same_v<lookup_type_t<exact_optional_unique_rvalue>,
                     std::unique_ptr<std::optional<runtime_type>> &&>);

  using value_resolution = resolution<A, void>;
  static_assert(
      std::is_same_v<typename value_resolution::request_types,
                     type_list<A, const A, volatile A, const volatile A>>);
  using optional_pointer_resolution = resolution<std::optional<A> *, void>;
  using optional_pointer_requests =
      typename optional_pointer_resolution::request_types;
  static_assert(
      type_list_contains_v<std::optional<A> *, optional_pointer_requests>);
  static_assert(
      type_list_contains_v<std::optional<A> *const, optional_pointer_requests>);
  static_assert(type_list_contains_v<volatile std::optional<A> *,
                                     optional_pointer_requests>);
  static_assert(type_list_contains_v<const volatile std::optional<A> *,
                                     optional_pointer_requests>);
  using const_optional_pointer_resolution =
      resolution<const std::optional<A> *, void>;
  using const_optional_pointer_requests =
      typename const_optional_pointer_resolution::request_types;
  static_assert(type_list_contains_v<const std::optional<A> *,
                                     const_optional_pointer_requests>);
  static_assert(type_list_contains_v<const volatile std::optional<A> *,
                                     const_optional_pointer_requests>);
  static_assert(!type_list_contains_v<std::optional<A> *,
                                      const_optional_pointer_requests>);
  static_assert(!type_list_contains_v<volatile std::optional<A> *,
                                      const_optional_pointer_requests>);
  using optional_reference_resolution = resolution<std::optional<A> &, void>;
  using optional_reference_requests =
      typename optional_reference_resolution::request_types;
  static_assert(
      type_list_contains_v<std::optional<A> &, optional_reference_requests>);
  static_assert(type_list_contains_v<const std::optional<A> &,
                                     optional_reference_requests>);
  static_assert(type_list_contains_v<volatile std::optional<A> &,
                                     optional_reference_requests>);
  static_assert(type_list_contains_v<const volatile std::optional<A> &,
                                     optional_reference_requests>);
  using volatile_reference_resolution =
      resolution<volatile std::optional<A> &, void>;
  using volatile_reference_requests =
      typename volatile_reference_resolution::request_types;
  static_assert(type_list_contains_v<volatile std::optional<A> &,
                                     volatile_reference_requests>);
  static_assert(type_list_contains_v<const volatile std::optional<A> &,
                                     volatile_reference_requests>);
  static_assert(
      !type_list_contains_v<std::optional<A> &, volatile_reference_requests>);
  static_assert(!type_list_contains_v<const std::optional<A> &,
                                      volatile_reference_requests>);
  using optional_rvalue_resolution = resolution<std::optional<A> &&, void>;
  using optional_rvalue_requests =
      typename optional_rvalue_resolution::request_types;
  static_assert(
      type_list_contains_v<std::optional<A> &&, optional_rvalue_requests>);
  static_assert(type_list_contains_v<const std::optional<A> &&,
                                     optional_rvalue_requests>);
  static_assert(type_list_contains_v<volatile std::optional<A> &&,
                                     optional_rvalue_requests>);
  static_assert(type_list_contains_v<const volatile std::optional<A> &&,
                                     optional_rvalue_requests>);
  using nested_pointer_resolution = resolution<A **, void>;
  using nested_pointer_requests =
      typename nested_pointer_resolution::request_types;
  static_assert(
      type_list_contains_v<const A *const *, nested_pointer_requests>);
  static_assert(!type_list_contains_v<const A **, nested_pointer_requests>);
  using optional_pointer_routes = type_list<optional_pointer_resolution>;
  static_assert(
      std::is_same_v<typename detail::matching_binding_resolution<
                         std::optional<A> *, optional_pointer_routes>::type,
                     optional_pointer_resolution>);
  static_assert(std::is_same_v<
                typename detail::matching_binding_resolution<
                    const std::optional<A> *, optional_pointer_routes>::type,
                optional_pointer_resolution>);
  static_assert(std::is_void_v<typename detail::matching_binding_resolution<
                    std::optional<I> *, optional_pointer_routes>::type>);

  using plain_pointer_model =
      detail::binding_model<type_registration<scope<external>, storage<A *>>>;
  using plain_pointer_resolutions =
      detail::binding_resolutions<A,
                                  typename plain_pointer_model::storage_type>;
  static_assert(
      std::is_same_v<resolution_targets_t<
                         typename plain_pointer_resolutions::value_resolutions>,
                     type_list<A>>);
  static_assert(std::is_same_v<
                resolution_targets_t<
                    typename plain_pointer_resolutions::pointer_resolutions>,
                type_list<A *>>);

  using optional_pointer = std::optional<A> *;
  static_assert(
      std::is_same_v<
          detail::type_conversion_path_t<std::optional<I>, optional_pointer>,
          detail::dereference_type_conversion<
              std::optional<I>, optional_pointer,
              detail::wrapper_type_conversion<
                  std::optional<I>, std::optional<A> &,
                  detail::reference_type_conversion<I, A &>, true>>>);
  using optional_pointer_model = detail::binding_model<
      type_registration<scope<external>, storage<optional_pointer>>>;
  using optional_pointer_resolutions = detail::binding_resolutions<
      A, typename optional_pointer_model::storage_type>;
  static_assert(std::is_same_v<
                resolution_targets_t<
                    typename optional_pointer_resolutions::value_resolutions>,
                type_list<std::optional<A>>>);
  static_assert(std::is_same_v<
                resolution_targets_t<typename optional_pointer_resolutions::
                                         lvalue_reference_resolutions>,
                type_list<std::optional<A> &>>);
  static_assert(std::is_same_v<
                resolution_targets_t<
                    typename optional_pointer_resolutions::pointer_resolutions>,
                type_list<std::optional<A> *>>);
  using optional_interface_pointer_model =
      detail::binding_model<type_registration<
          scope<external>, storage<optional_pointer>, interfaces<I, J>>>;
  using optional_interface_resolutions = detail::binding_resolutions<
      I, typename optional_interface_pointer_model::storage_type>;
  static_assert(has_resolution_target_v<
                std::optional<A>,
                typename optional_interface_resolutions::value_resolutions>);
  static_assert(has_resolution_target_v<
                std::optional<I>,
                typename optional_interface_resolutions::value_resolutions>);
  static_assert(
      std::is_same_v<
          resolution_targets_t<
              typename optional_interface_resolutions::pointer_resolutions>,
          type_list<std::optional<A> *>>);
  using optional_interface_binding =
      detail::binding<I, optional_interface_pointer_model>;
  static_assert(
      std::is_same_v<
          typename detail::binding_supports_request<
              std::optional<I>, optional_interface_binding>::type::target_type,
          std::optional<I>>);
  using const_pointer_interface_model = detail::binding_model<
      type_registration<scope<external>, storage<const A *>, interfaces<I, J>>>;
  using const_pointer_interface_resolutions = detail::binding_resolutions<
      I, typename const_pointer_interface_model::storage_type>;
  static_assert(
      std::is_same_v<
          resolution_targets_t<typename const_pointer_interface_resolutions::
                                   lvalue_reference_resolutions>,
          type_list<const I &>>);
  static_assert(
      std::is_same_v<
          resolution_targets_t<typename const_pointer_interface_resolutions::
                                   pointer_resolutions>,
          type_list<const I *>>);

  using external_move_only_pointer_model = detail::binding_model<
      type_registration<scope<external>, storage<move_only *>>>;
  using external_resolutions = detail::binding_resolutions<
      move_only, typename external_move_only_pointer_model::storage_type>;
  static_assert(std::is_same_v<typename external_resolutions::value_resolutions,
                               type_list<>>);
  static_assert(
      std::is_same_v<resolution_targets_t<
                         typename external_resolutions::pointer_resolutions>,
                     type_list<move_only *>>);

  using shared_move_only_model = detail::binding_model<
      type_registration<scope<shared>, storage<move_only>>>;
  using shared_resolutions = detail::binding_resolutions<
      move_only, typename shared_move_only_model::storage_type>;
  static_assert(std::is_same_v<typename shared_resolutions::value_resolutions,
                               type_list<>>);
  static_assert(std::is_same_v<
                resolution_targets_t<
                    typename shared_resolutions::lvalue_reference_resolutions>,
                type_list<move_only &>>);

  using shared_cyclical_handle = std::shared_ptr<A>;
  using shared_cyclical_handle_model = detail::binding_model<
      type_registration<scope<shared_cyclical>, storage<shared_cyclical_handle>,
                        interfaces<shared_cyclical_handle>>>;
  using shared_cyclical_handle_resolutions = detail::binding_resolutions<
      shared_cyclical_handle,
      typename shared_cyclical_handle_model::storage_type>;
  static_assert(
      has_resolution_target_v<
          shared_cyclical_handle,
          typename shared_cyclical_handle_resolutions::value_resolutions>);
  static_assert(
      !has_resolution_target_v<shared_cyclical_handle &&,
                               typename shared_cyclical_handle_resolutions::
                                   rvalue_reference_resolutions>);

  using unique_move_only_model = detail::binding_model<
      type_registration<scope<unique>, storage<move_only>>>;
  using unique_resolutions = detail::binding_resolutions<
      move_only, typename unique_move_only_model::storage_type>;
  static_assert(
      std::is_same_v<
          resolution_targets_t<typename unique_resolutions::value_resolutions>,
          type_list<move_only, std::optional<move_only>>>);

  using compatible_unique_handle_model =
      detail::binding_model<type_registration<
          scope<unique>, storage<std::unique_ptr<A>>, interfaces<I>>>;
  using compatible_unique_handle_resolutions = detail::binding_resolutions<
      I, typename compatible_unique_handle_model::storage_type>;
  static_assert(
      has_resolution_target_v<
          std::unique_ptr<I>,
          typename compatible_unique_handle_resolutions::value_resolutions>);

  using incompatible_unique_handle_model =
      detail::binding_model<type_registration<
          scope<unique>, storage<std::unique_ptr<A, incompatible_deleter>>,
          interfaces<I>>>;
  using incompatible_unique_handle_resolutions = detail::binding_resolutions<
      I, typename incompatible_unique_handle_model::storage_type>;
  static_assert(
      !has_resolution_target_v<
          std::unique_ptr<I>,
          typename incompatible_unique_handle_resolutions::value_resolutions>);

  using fresh_resolutions =
      detail::binding_resolutions<move_only, fresh_value_storage<move_only>>;
  static_assert(
      std::is_same_v<
          resolution_targets_t<typename fresh_resolutions::value_resolutions>,
          type_list<move_only>>);
  using immovable_fresh_resolutions =
      detail::binding_resolutions<immovable, fresh_value_storage<immovable>>;
  static_assert(
      std::is_same_v<typename immovable_fresh_resolutions::value_resolutions,
                     type_list<>>);

  using shared_variant =
      std::variant<std::shared_ptr<move_only>, std::monostate>;
  using shared_variant_model = detail::binding_model<
      type_registration<scope<shared>, storage<shared_variant>>>;
  using shared_variant_resolutions =
      detail::binding_resolutions<std::shared_ptr<move_only>,
                                  typename shared_variant_model::storage_type>;
  static_assert(has_resolution_target_v<
                std::shared_ptr<move_only>,
                typename shared_variant_resolutions::value_resolutions>);

  using shared_move_only_variant =
      std::variant<std::unique_ptr<move_only>, std::monostate>;
  using shared_move_only_variant_model = detail::binding_model<
      type_registration<scope<shared>, storage<shared_move_only_variant>>>;
  using shared_move_only_variant_resolutions = detail::binding_resolutions<
      std::unique_ptr<move_only>,
      typename shared_move_only_variant_model::storage_type>;
  static_assert(
      std::is_same_v<
          typename shared_move_only_variant_resolutions::value_resolutions,
          type_list<>>);

  using unique_variant =
      std::variant<std::unique_ptr<move_only>, std::monostate>;
  using unique_variant_model = detail::binding_model<
      type_registration<scope<unique>, storage<unique_variant>>>;
  using unique_variant_resolutions =
      detail::binding_resolutions<std::unique_ptr<move_only>,
                                  typename unique_variant_model::storage_type>;
  static_assert(has_resolution_target_v<
                std::unique_ptr<move_only>,
                typename unique_variant_resolutions::value_resolutions>);
  static_assert(
      has_resolution_target_v<
          std::unique_ptr<move_only> &&,
          typename unique_variant_resolutions::rvalue_reference_resolutions>);

  using external_nested_move_only_pointer_model = detail::binding_model<
      type_registration<scope<external>, storage<std::optional<move_only> *>>>;
  using nested_resolutions = detail::binding_resolutions<
      move_only,
      typename external_nested_move_only_pointer_model::storage_type>;
  static_assert(std::is_same_v<typename nested_resolutions::value_resolutions,
                               type_list<>>);
  static_assert(
      std::is_same_v<resolution_targets_t<
                         typename nested_resolutions::pointer_resolutions>,
                     type_list<std::optional<move_only> *>>);

  using external_nested_move_only_reference_model =
      detail::binding_model<type_registration<
          scope<external>, storage<const std::optional<move_only> &>>>;
  using nested_reference_resolutions = detail::binding_resolutions<
      move_only,
      typename external_nested_move_only_reference_model::storage_type>;
  static_assert(
      std::is_same_v<typename nested_reference_resolutions::value_resolutions,
                     type_list<>>);

  using exact_optional_model = detail::binding_model<
      type_registration<scope<external>, storage<std::optional<copy_only> &>,
                        interfaces<std::optional<copy_only>>>>;
  using exact_optional_binding =
      detail::binding<std::optional<copy_only>, exact_optional_model>;
  static_assert(detail::binding_supports_request_v<std::optional<copy_only>,
                                                   exact_optional_binding>);
  static_assert(
      !detail::binding_supports_request_v<
          std::optional<std::optional<copy_only>>, exact_optional_binding>);

  using const_array_pointer = const std::array<A, 2> *;
  using const_array_pointer_model = detail::binding_model<
      type_registration<scope<external>, storage<const_array_pointer>>>;
  using const_array_pointer_resolutions = detail::binding_resolutions<
      A, typename const_array_pointer_model::storage_type>;
  static_assert(
      std::is_same_v<
          resolution_targets_t<
              typename const_array_pointer_resolutions::value_resolutions>,
          type_list<std::array<A, 2>>>);
  static_assert(std::is_same_v<
                resolution_targets_t<typename const_array_pointer_resolutions::
                                         lvalue_reference_resolutions>,
                type_list<const std::array<A, 2> &>>);
  static_assert(
      std::is_same_v<
          resolution_targets_t<
              typename const_array_pointer_resolutions::pointer_resolutions>,
          type_list<const std::array<A, 2> *>>);

  using unique_optional_pointer_model = detail::binding_model<
      type_registration<scope<unique>, storage<optional_pointer>>>;
  using unique_optional_pointer_resolutions = detail::binding_resolutions<
      A, typename unique_optional_pointer_model::storage_type>;
  static_assert(
      has_resolution_target_v<
          std::unique_ptr<std::optional<A>>,
          typename unique_optional_pointer_resolutions::value_resolutions>);
  static_assert(
      has_resolution_target_v<
          std::shared_ptr<std::optional<A>>,
          typename unique_optional_pointer_resolutions::value_resolutions>);
  static_assert(
      has_resolution_target_v<std::unique_ptr<std::optional<A>> &&,
                              typename unique_optional_pointer_resolutions::
                                  rvalue_reference_resolutions>);
  static_assert(
      has_resolution_target_v<std::shared_ptr<std::optional<A>> &&,
                              typename unique_optional_pointer_resolutions::
                                  rvalue_reference_resolutions>);
  static_assert(
      std::is_same_v<
          resolution_targets_t<typename unique_optional_pointer_resolutions::
                                   pointer_resolutions>,
          type_list<std::optional<A> *>>);
}

TEST(type_registration_test,
     nested_raw_pointer_storage_resolves_exact_request_shapes) {
  struct service {
    int value;
  };

  std::optional<service> external_value{service{7}};
  container<> external_container;
  external_container
      .register_type<scope<external>, storage<std::optional<service> *>>(
          std::addressof(external_value));

  auto *external_pointer =
      external_container.resolve<std::optional<service> *>();
  EXPECT_EQ(external_pointer, std::addressof(external_value));
  ASSERT_TRUE(external_pointer->has_value());
  EXPECT_EQ(external_pointer->value().value, 7);
  auto &external_reference =
      external_container.resolve<std::optional<service> &>();
  EXPECT_EQ(std::addressof(external_reference), std::addressof(external_value));
  auto external_copy = external_container.resolve<std::optional<service>>();
  ASSERT_TRUE(external_copy.has_value());
  EXPECT_EQ(external_copy->value, 7);

  const std::array<int, 2> external_array{3, 5};
  container<> external_array_container;
  external_array_container
      .register_type<scope<external>, storage<const std::array<int, 2> *>>(
          std::addressof(external_array));
  EXPECT_EQ((external_array_container.resolve<std::array<int, 2>>()),
            external_array);

  auto external_shared_value = std::make_shared<service>(service{9});
  container<> external_shared_container;
  external_shared_container
      .register_type<scope<external>, storage<std::shared_ptr<service> *>>(
          std::addressof(external_shared_value));
  EXPECT_EQ(external_shared_container.resolve<std::shared_ptr<service>>(),
            external_shared_value);

  struct move_only_service {
    explicit move_only_service(int init_value) : value(init_value) {}
    move_only_service(const move_only_service &) = delete;
    move_only_service(move_only_service &&) = default;

    int value;
  };

  move_only_service external_move_only_value{10};
  container<> external_move_only_container;
  external_move_only_container
      .register_type<scope<external>, storage<move_only_service *>>(
          std::addressof(external_move_only_value));
  EXPECT_THROW((void)external_move_only_container.resolve<move_only_service>(),
               type_not_convertible_exception);
  EXPECT_EQ(external_move_only_value.value, 10);

  std::optional<move_only_service> external_nested_move_only_value{
      std::in_place, 12};
  container<> external_nested_move_only_container;
  external_nested_move_only_container.register_type<
      scope<external>, storage<std::optional<move_only_service> *>>(
      std::addressof(external_nested_move_only_value));
  EXPECT_THROW((void)external_nested_move_only_container
                   .resolve<std::optional<move_only_service>>(),
               type_not_convertible_exception);
  ASSERT_TRUE(external_nested_move_only_value.has_value());
  EXPECT_EQ(external_nested_move_only_value->value, 12);

  using external_variant = std::variant<int, float>;
  const external_variant *null_external_variant = nullptr;
  container<> null_external_variant_container;
  null_external_variant_container
      .register_type<scope<external>, storage<const external_variant *>>(
          null_external_variant);
  EXPECT_EQ(null_external_variant_container.resolve<const external_variant *>(),
            nullptr);

  container<> shared_container;
  shared_container
      .register_type<scope<shared>, storage<const std::optional<service> *>>(
          callable([]() -> const std::optional<service> * {
            return new std::optional<service>{service{11}};
          }));

  auto *shared_pointer =
      shared_container.resolve<const std::optional<service> *>();
  ASSERT_NE(shared_pointer, nullptr);
  ASSERT_TRUE(shared_pointer->has_value());
  EXPECT_EQ(shared_pointer->value().value, 11);
  EXPECT_EQ(shared_container.resolve<const std::optional<service> *>(),
            shared_pointer);
  auto &shared_reference =
      shared_container.resolve<const std::optional<service> &>();
  EXPECT_EQ(std::addressof(shared_reference), shared_pointer);

  container<> unique_container;
  unique_container
      .register_type<scope<unique>, storage<std::optional<service> *>>(
          callable([]() { return new std::optional<service>{service{13}}; }));
  auto unique_value =
      unique_container.resolve<std::unique_ptr<std::optional<service>>>();
  ASSERT_TRUE(unique_value->has_value());
  EXPECT_EQ(unique_value->value().value, 13);
  auto unique_rvalue =
      unique_container.resolve<std::unique_ptr<std::optional<service>> &&>();
  ASSERT_TRUE(unique_rvalue->has_value());
  EXPECT_EQ(unique_rvalue->value().value, 13);
  auto shared_value =
      unique_container.resolve<std::shared_ptr<std::optional<service>>>();
  ASSERT_TRUE(shared_value->has_value());
  EXPECT_EQ(shared_value->value().value, 13);

  struct service_interface {
    int value;
  };
  struct secondary {
    virtual ~secondary() = default;
  };
  struct implementation : service_interface, secondary {
    explicit implementation(int init) { value = init; }
  };

  std::optional<implementation> converted_value{std::in_place, 17};
  container<> converted_container;
  converted_container
      .register_type<scope<external>, storage<std::optional<implementation> *>,
                     interfaces<service_interface, secondary>>(
          std::addressof(converted_value));

  auto converted =
      converted_container.resolve<std::optional<service_interface>>();
  ASSERT_TRUE(converted.has_value());
  EXPECT_EQ(converted->value, 17);

  std::optional<implementation> empty_converted_value;
  container<> empty_converted_container;
  empty_converted_container
      .register_type<scope<external>, storage<std::optional<implementation> *>,
                     interfaces<service_interface, secondary>>(
          std::addressof(empty_converted_value));
  EXPECT_FALSE(
      empty_converted_container.resolve<std::optional<service_interface>>()
          .has_value());

  test_optional<implementation> custom_value{implementation{19}};
  container<> custom_container;
  custom_container
      .register_type<scope<external>, storage<test_optional<implementation> *>,
                     interfaces<service_interface, secondary>>(
          std::addressof(custom_value));

  auto custom_converted =
      custom_container.resolve<test_optional<service_interface>>();
  ASSERT_NE(custom_converted.get(), nullptr);
  EXPECT_EQ(custom_converted.get()->value, 19);

  test_optional<implementation> empty_custom_value;
  container<> empty_custom_container;
  empty_custom_container
      .register_type<scope<external>, storage<test_optional<implementation> *>,
                     interfaces<service_interface, secondary>>(
          std::addressof(empty_custom_value));
  EXPECT_FALSE(
      empty_custom_container.resolve<test_optional<service_interface>>()
          .has_value());
}

TEST(type_registration_test, const_reference_storage_preserves_const_access) {
  using model = detail::binding_model<
      type_registration<scope<external>, storage<const int &>>>;
  using resolutions =
      detail::binding_resolutions<int, typename model::storage_type>;

  static_assert(std::is_same_v<
                resolution_targets_t<typename resolutions::value_resolutions>,
                type_list<int>>);
  static_assert(
      std::is_same_v<resolution_targets_t<
                         typename resolutions::lvalue_reference_resolutions>,
                     type_list<const int &>>);
  static_assert(std::is_same_v<
                resolution_targets_t<typename resolutions::pointer_resolutions>,
                type_list<const int *>>);

  const int value = 7;
  container<> value_container;
  value_container.register_type<scope<external>, storage<const int &>>(value);

  EXPECT_EQ(value_container.resolve<int>(), 7);
  EXPECT_EQ(&value_container.resolve<const int &>(), &value);
  EXPECT_EQ(value_container.resolve<const int *>(), &value);

  const std::optional<int> optional_value{11};
  using optional_model = detail::binding_model<
      type_registration<scope<external>, storage<const std::optional<int> &>>>;
  using optional_resolutions =
      detail::binding_resolutions<int, typename optional_model::storage_type>;
  static_assert(has_resolution_target_v<
                int, typename optional_resolutions::value_resolutions>);

  container<> optional_container;
  optional_container
      .register_type<scope<external>, storage<const std::optional<int> &>>(
          optional_value);

  EXPECT_EQ(optional_container.resolve<int>(), 11);
  EXPECT_EQ(&optional_container.resolve<const int &>(), &*optional_value);
  EXPECT_EQ(optional_container.resolve<const int *>(), &*optional_value);
  EXPECT_EQ(&optional_container.resolve<const std::optional<int> &>(),
            &optional_value);

  const std::optional<int> empty_optional;
  container<> empty_optional_container;
  empty_optional_container
      .register_type<scope<external>, storage<const std::optional<int> &>>(
          empty_optional);
  EXPECT_EQ(empty_optional_container.resolve<const int *>(), nullptr);

  const test_optional<int> custom_optional_value{13};
  container<> custom_optional_container;
  custom_optional_container
      .register_type<scope<external>, storage<const test_optional<int> &>>(
          custom_optional_value);

  EXPECT_EQ(custom_optional_container.resolve<int>(), 13);
  EXPECT_EQ(&custom_optional_container.resolve<const int &>(),
            custom_optional_value.get());
  EXPECT_EQ(custom_optional_container.resolve<const int *>(),
            custom_optional_value.get());

  using nested_optional_type = std::optional<std::variant<int, float>>;
  const nested_optional_type nested_optional_value{std::in_place,
                                                   std::in_place_type<int>, 17};
  container<> nested_optional_container;
  nested_optional_container.register_type<
      scope<external>, storage<const nested_optional_type &>, interfaces<int>>(
      nested_optional_value);

  EXPECT_EQ(nested_optional_container.resolve<int>(), 17);
  EXPECT_EQ(&nested_optional_container.resolve<const int &>(),
            &std::get<int>(*nested_optional_value));
  EXPECT_EQ(nested_optional_container.resolve<const int *>(),
            &std::get<int>(*nested_optional_value));

  using doubly_nested_optional_type = std::optional<nested_optional_type>;
  const doubly_nested_optional_type doubly_nested_optional_value{
      std::in_place, std::in_place, std::in_place_type<int>, 19};
  container<> doubly_nested_optional_container;
  doubly_nested_optional_container.register_type<
      scope<external>, storage<const doubly_nested_optional_type &>,
      interfaces<int>>(doubly_nested_optional_value);

  EXPECT_EQ(doubly_nested_optional_container.resolve<int>(), 19);
  EXPECT_EQ(&doubly_nested_optional_container.resolve<const int &>(),
            &std::get<int>(**doubly_nested_optional_value));
  EXPECT_EQ(doubly_nested_optional_container.resolve<const int *>(),
            &std::get<int>(**doubly_nested_optional_value));
}

TEST(type_registration_test,
     const_alternative_storage_only_publishes_const_borrows) {
  using variant_type = std::variant<int, float>;

  static_assert(
      !detail::is_type_conversion_available_v<int &, const variant_type &>);
  static_assert(detail::is_type_conversion_available_v<const int &,
                                                       const variant_type &>);
  static_assert(
      !detail::is_type_conversion_available_v<int *, const variant_type &>);
  static_assert(detail::is_type_conversion_available_v<const int *,
                                                       const variant_type &>);

  const variant_type value{std::in_place_type<int>, 13};
  container<> variant_container;
  variant_container.register_type<
      scope<external>, storage<const variant_type &>, interfaces<int>>(value);

  EXPECT_EQ(variant_container.resolve<int>(), 13);
  EXPECT_EQ(&variant_container.resolve<const int &>(), &std::get<int>(value));
  EXPECT_EQ(variant_container.resolve<const int *>(), &std::get<int>(value));
}

TEST(type_registration_test,
     multidimensional_array_storage_preserves_exact_and_row_shapes) {
  using model = detail::binding_model<
      type_registration<scope<shared>, storage<int[2][3]>>>;
  using resolutions =
      detail::binding_resolutions<int, typename model::storage_type>;
  static_assert(
      std::is_same_v<typename resolutions::value_resolutions, type_list<>>);
  static_assert(
      std::is_same_v<resolution_targets_t<
                         typename resolutions::lvalue_reference_resolutions>,
                     type_list<int (&)[2][3]>>);
  static_assert(std::is_same_v<
                resolution_targets_t<typename resolutions::pointer_resolutions>,
                type_list<int (*)[3], int (*)[2][3]>>);

  container<> container;
  container.register_type<scope<shared>, storage<int[2][3]>>();

  auto *rows = container.resolve<int (*)[3]>();
  auto *whole = container.resolve<int (*)[2][3]>();
  auto *const_rows = container.resolve<const int (*)[3]>();
  auto *volatile_rows = container.resolve<volatile int (*)[3]>();
  auto *cv_whole = container.resolve<const volatile int (*)[2][3]>();

  ASSERT_EQ(&(*whole)[0][0], &rows[0][0]);
  ASSERT_EQ(rows, const_rows);
  ASSERT_EQ(rows, volatile_rows);
  ASSERT_EQ(whole, cv_whole);
  (*whole)[1][1] = 7;
  EXPECT_EQ(rows[1][1], 7);
}

TEST(type_registration_test, normalized_type_trait) {
  struct I {
    virtual ~I() = default;
  };
  struct A : I {};
  struct annotation_tag {};
  using selected_key = detail::type_selector<annotation_tag>;

  static_assert(
      std::is_same_v<
          normalized_type_t<const std::shared_ptr<std::unique_ptr<A>> &>, A>);
  static_assert(
      std::is_same_v<normalized_type_t<const volatile A *const volatile>, A>);
  static_assert(std::is_same_v<normalized_type_t<const A *const *volatile>, A>);
  static_assert(std::is_same_v<normalized_type_t<volatile A(*)[3]>, A[3]>);
  static_assert(
      std::is_same_v<normalized_type_t<const volatile A(*)[3]>, A[3]>);
  static_assert(std::is_same_v<normalized_type_t<volatile A(&)[3]>, A[3]>);
  static_assert(
      std::is_same_v<normalized_type_t<annotated<I &, annotation_tag>>,
                     annotated<I, annotation_tag>>);
  static_assert(!request_may_escape_v<A>);
  static_assert(!request_may_escape_v<A &&>);
  static_assert(request_may_escape_v<A &>);
  static_assert(request_may_escape_v<const A &>);
  static_assert(request_may_escape_v<A *>);
  static_assert(request_may_escape_v<annotated<A &, annotation_tag>>);
  static_assert(request_may_escape_v<const A *>);
  static_assert(
      request_may_escape_v<dependency<A &, key_type<annotation_tag>>>);
  static_assert(
      request_may_escape_v<dependency<const A &, key_type<annotation_tag>>>);
  static_assert(
      request_may_escape_v<dependency<A *, key_type<annotation_tag>>>);
  static_assert(
      request_may_escape_v<dependency<const A *, key_type<annotation_tag>>>);
  static_assert(!request_may_escape_v<dependency<A, key_type<annotation_tag>>>);
  static_assert(
      !request_may_escape_v<dependency<A &&, key_type<annotation_tag>>>);
  static_assert(request_may_escape_v<detail::selected<A &, selected_key>>);
  static_assert(
      request_may_escape_v<detail::selected<const A &, selected_key>>);
  static_assert(request_may_escape_v<detail::selected<A *, selected_key>>);
  static_assert(
      request_may_escape_v<detail::selected<const A *, selected_key>>);
  static_assert(!request_may_escape_v<detail::selected<A, selected_key>>);
  static_assert(!request_may_escape_v<detail::selected<A &&, selected_key>>);
}
