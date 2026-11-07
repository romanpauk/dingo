//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "type_registration_common.h"

#include <dingo/static/binding.h>

#include "support/custom_wrappers.h"
#include "support/resolution_test_types.h"

#include <array>
#include <memory>
#include <optional>
#include <variant>

namespace conversion_compatibility_test {
struct source {};
struct target {};
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
struct customized_value {
  int value;
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
struct type_conversion_traits<conversion_compatibility_test::customized_value,
                              conversion_compatibility_test::customized_value> {
  template <typename> using required_access = borrow;

  static conversion_compatibility_test::customized_value
  convert(const conversion_compatibility_test::customized_value &source) {
    return {source.value + 1};
  }
};

template <> struct type_traits<conversion_compatibility_test::owning_source> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = false;
  static constexpr bool is_owning_handle = true;
};

template <> struct type_traits<conversion_compatibility_test::pointer_like> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_owning_handle = true;

  using value_type = int;

  template <typename>
  using rebind_t = conversion_compatibility_test::pointer_like;
};

template <>
struct type_traits<conversion_compatibility_test::observer_pointer> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_owning_handle = false;

  using value_type = int;

  template <typename>
  using rebind_t = conversion_compatibility_test::observer_pointer;
};

template <>
struct type_traits<conversion_compatibility_test::converted_observer_pointer> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_owning_handle = false;

  using value_type = int;

  template <typename>
  using rebind_t = conversion_compatibility_test::converted_observer_pointer;
};

template <>
struct type_traits<conversion_compatibility_test::safe_observer_pointer> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_owning_handle = false;

  using value_type = long;

  template <typename>
  using rebind_t = conversion_compatibility_test::safe_observer_pointer;
};

template <> struct type_traits<conversion_compatibility_test::consuming_owner> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_owning_handle = true;

  using value_type = int;

  template <typename>
  using rebind_t = conversion_compatibility_test::consuming_owner;
};

template <> struct type_traits<conversion_compatibility_test::copying_owner> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_owning_handle = true;

  using value_type = int;

  template <typename>
  using rebind_t = conversion_compatibility_test::copying_owner;
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

// The targets that accept the value form of a request, as a binding publishes
// them.
template <typename Target> struct accepted_target {
  using target_type = Target;
};

template <typename Resolutions> struct accepted_value_targets;

template <typename... Resolutions>
struct accepted_value_targets<type_list<Resolutions...>> {
  template <typename Resolution>
  using targets = std::conditional_t<
      std::is_reference_v<typename Resolution::target_type> ||
          std::is_pointer_v<typename Resolution::target_type>,
      type_list<>,
      type_list<accepted_target<typename Resolution::target_type>>>;

  using type = type_list_cat_t<targets<Resolutions>...>;
};

template <typename BindingResolutions>
struct accepted_values
    : accepted_value_targets<typename BindingResolutions::type> {};

template <typename Resolutions> struct reference_resolution_targets;

template <typename... Resolutions>
struct reference_resolution_targets<type_list<Resolutions...>> {
  template <typename Resolution>
  using targets = std::conditional_t<
      std::is_lvalue_reference_v<typename Resolution::target_type>,
      type_list<typename Resolution::target_type>, type_list<>>;

  using type = type_list_cat_t<targets<Resolutions>...>;
};

template <typename Resolutions>
using reference_resolution_targets_t =
    typename reference_resolution_targets<Resolutions>::type;

template <typename Resolutions> struct pointer_resolution_targets;

template <typename... Resolutions>
struct pointer_resolution_targets<type_list<Resolutions...>> {
  template <typename Resolution>
  using targets =
      std::conditional_t<std::is_pointer_v<typename Resolution::target_type>,
                         type_list<typename Resolution::target_type>,
                         type_list<>>;

  using type = type_list_cat_t<targets<Resolutions>...>;
};

template <typename Resolutions>
using pointer_resolution_targets_t =
    typename pointer_resolution_targets<Resolutions>::type;

template <typename Type> struct fresh_value_storage {
  using type = Type;
  using resolved_type = Type;

  struct conversions {
    static constexpr bool is_stable = true;

    using value_types = type_list<>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<runtime_type &&>;
    using pointer_types = type_list<>;
  };
};
} // namespace

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
  // Taking the address of a compatible lvalue is selected directly, also when
  // the source is itself an alternative type. A pointer that merely converts
  // to the target keeps its direct conversion, and a variant lvalue still
  // resolves a base pointer target through its alternatives.
  using address_variant = std::variant<int, long>;
  static_assert(
      std::is_same_v<detail::type_conversion_path_t<int *, int &, borrow>,
                     detail::address_type_conversion<int *, int &>>);
  static_assert(
      std::is_same_v<detail::type_conversion_path_t<const address_variant *,
                                                    address_variant &, borrow>,
                     detail::address_type_conversion<const address_variant *,
                                                     address_variant &>>);
  static_assert(
      std::is_same_v<detail::type_conversion_path_t<address_variant *,
                                                    address_variant &, consume>,
                     detail::address_type_conversion<address_variant *,
                                                     address_variant &>>);
  static_assert(
      !std::is_same_v<detail::type_conversion_path_t<void *, int *&, borrow>,
                      detail::address_type_conversion<void *, int *&>>);
  static_assert(
      detail::is_type_conversion_available_v<void *, address_variant &,
                                             borrow>);
  static_assert(
      !std::is_same_v<
          detail::type_conversion_path_t<void *, address_variant &, borrow>,
          detail::address_type_conversion<void *, address_variant &>>);
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
  auto result =
      conversion::apply(resolver, context, value, describe_type<target>(),
                        describe_type<source>());

  EXPECT_TRUE(result.used_const_source);
  EXPECT_EQ(result.value, 17);
  EXPECT_NE(value.value, nullptr);
}

TEST(type_registration_test, same_type_custom_conversion_is_preserved) {
  using value_type = conversion_compatibility_test::customized_value;
  using conversion =
      detail::type_conversion_path_t<value_type, value_type &, borrow>;
  struct conversion_context {};

  value_type value{17};
  conversion_context resolver;
  conversion_context context;
  auto result =
      conversion::apply(resolver, context, value, describe_type<value_type>(),
                        describe_type<value_type>());

  EXPECT_EQ(result.value, 18);
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
  auto result =
      conversion::apply(resolver, context, value, describe_type<target>(),
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
  auto result =
      conversion::apply(resolver, context, value, describe_type<target>(),
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
  auto result =
      conversion::apply(resolver, context, source, describe_type<target>(),
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
                              typename accepted_values<resolutions>::type>);
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
                              typename accepted_values<resolutions>::type>);
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
  static_assert(std::is_same_v<leaf_type_t<nested_list>, type_list<A, A>>);
  static_assert(std::is_same_v<rebind_leaf_t<nested_list, I>,
                               type_list<std::shared_ptr<std::unique_ptr<I>> &,
                                         std::unique_ptr<I *>>>);
  static_assert(std::is_same_v<leaf_type_t<std::optional<const A *>>, A>);
  static_assert(std::is_same_v<rebind_leaf_t<const A *, I>, const I *>);
  static_assert(std::is_same_v<rebind_leaf_t<std::optional<const A *>, I>,
                               std::optional<const I *>>);
  static_assert(std::is_same_v<rebind_leaf_t<std::shared_ptr<const A>, I>,
                               std::shared_ptr<const I>>);
  static_assert(std::is_same_v<rebind_leaf_t<std::unique_ptr<const A>, I>,
                               std::unique_ptr<const I>>);
  static_assert(std::is_same_v<rebind_leaf_t<std::shared_ptr<volatile A>, I>,
                               std::shared_ptr<volatile I>>);
  static_assert(
      std::is_same_v<rebind_leaf_t<std::unique_ptr<const volatile A>, I>,
                     std::unique_ptr<const volatile I>>);
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
  static_assert(std::is_same_v<resolved_type_t<exact_const_optional_pointer,
                                               const std::optional<A> *>,
                               const std::optional<A> *>);

  static_assert(detail::is_resolution_request_v<A, A>);
  static_assert(detail::is_resolution_request_v<A, const A>);
  static_assert(detail::is_resolution_request_v<A, volatile A>);
  static_assert(detail::is_resolution_request_v<A, const volatile A>);
  using optional_pointer_resolution = resolution<std::optional<A> *, void>;
  static_assert(
      detail::is_resolution_request_v<std::optional<A> *, std::optional<A> *>);
  static_assert(detail::is_resolution_request_v<std::optional<A> *,
                                                std::optional<A> *const>);
  static_assert(detail::is_resolution_request_v<std::optional<A> *,
                                                volatile std::optional<A> *>);
  static_assert(
      detail::is_resolution_request_v<std::optional<A> *,
                                      const volatile std::optional<A> *>);
  static_assert(detail::is_resolution_request_v<const std::optional<A> *,
                                                const std::optional<A> *>);
  static_assert(
      detail::is_resolution_request_v<const std::optional<A> *,
                                      const volatile std::optional<A> *>);
  static_assert(!detail::is_resolution_request_v<const std::optional<A> *,
                                                 std::optional<A> *>);
  static_assert(!detail::is_resolution_request_v<const std::optional<A> *,
                                                 volatile std::optional<A> *>);
  using function_type = void();
  static_assert(
      detail::is_resolution_request_v<function_type *, function_type *const>);
  static_assert(
      detail::is_resolution_request_v<std::optional<A> &, std::optional<A> &>);
  static_assert(detail::is_resolution_request_v<std::optional<A> &,
                                                const std::optional<A> &>);
  static_assert(detail::is_resolution_request_v<std::optional<A> &,
                                                volatile std::optional<A> &>);
  static_assert(
      detail::is_resolution_request_v<std::optional<A> &,
                                      const volatile std::optional<A> &>);
  static_assert(detail::is_resolution_request_v<volatile std::optional<A> &,
                                                volatile std::optional<A> &>);
  static_assert(
      detail::is_resolution_request_v<volatile std::optional<A> &,
                                      const volatile std::optional<A> &>);
  static_assert(!detail::is_resolution_request_v<volatile std::optional<A> &,
                                                 std::optional<A> &>);
  static_assert(!detail::is_resolution_request_v<volatile std::optional<A> &,
                                                 const std::optional<A> &>);
  static_assert(detail::is_resolution_request_v<std::optional<A> &&,
                                                std::optional<A> &&>);
  static_assert(detail::is_resolution_request_v<std::optional<A> &&,
                                                const std::optional<A> &&>);
  static_assert(detail::is_resolution_request_v<std::optional<A> &&,
                                                volatile std::optional<A> &&>);
  static_assert(
      detail::is_resolution_request_v<std::optional<A> &&,
                                      const volatile std::optional<A> &&>);
  static_assert(detail::is_resolution_request_v<A **, const A *const *>);
  static_assert(!detail::is_resolution_request_v<A **, const A **>);

  EXPECT_TRUE(detail::matches_qualification_conversion(
      describe_type<A **>(), describe_type<const A *const *>()));
  EXPECT_FALSE(detail::matches_qualification_conversion(
      describe_type<A **>(), describe_type<const A **>()));
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
  static_assert(std::is_same_v<resolution_targets_t<typename accepted_values<
                                   plain_pointer_resolutions>::type>,
                               type_list<A>>);
  static_assert(std::is_same_v<
                pointer_resolution_targets_t<
                    typename plain_pointer_resolutions::object_resolutions>,
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
  static_assert(std::is_same_v<resolution_targets_t<typename accepted_values<
                                   optional_pointer_resolutions>::type>,
                               type_list<std::optional<A>>>);
  static_assert(std::is_same_v<
                reference_resolution_targets_t<
                    typename optional_pointer_resolutions::object_resolutions>,
                type_list<std::optional<A> &>>);
  static_assert(std::is_same_v<
                pointer_resolution_targets_t<
                    typename optional_pointer_resolutions::object_resolutions>,
                type_list<std::optional<A> *>>);
  using optional_interface_pointer_model =
      detail::binding_model<type_registration<
          scope<external>, storage<optional_pointer>, interfaces<I, J>>>;
  using optional_interface_resolutions = detail::binding_resolutions<
      I, typename optional_interface_pointer_model::storage_type>;
  // The optional of the leaf selects the leaf, which is not bound; the
  // interface publishes its own optional only.
  static_assert(
      !has_resolution_target_v<
          std::optional<A>,
          typename accepted_values<optional_interface_resolutions>::type>);
  static_assert(
      has_resolution_target_v<
          std::optional<I>,
          typename accepted_values<optional_interface_resolutions>::type>);
  static_assert(
      std::is_same_v<
          pointer_resolution_targets_t<
              typename optional_interface_resolutions::object_resolutions>,
          type_list<>>);
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
          reference_resolution_targets_t<
              typename const_pointer_interface_resolutions::object_resolutions>,
          type_list<const I &>>);
  static_assert(
      std::is_same_v<
          pointer_resolution_targets_t<
              typename const_pointer_interface_resolutions::object_resolutions>,
          type_list<const I *>>);

  using external_move_only_pointer_model = detail::binding_model<
      type_registration<scope<external>, storage<move_only *>>>;
  using external_resolutions = detail::binding_resolutions<
      move_only, typename external_move_only_pointer_model::storage_type>;
  static_assert(
      std::is_same_v<typename accepted_values<external_resolutions>::type,
                     type_list<>>);
  static_assert(
      std::is_same_v<pointer_resolution_targets_t<
                         typename external_resolutions::object_resolutions>,
                     type_list<move_only *>>);

  // A raw pointer and a unique_ptr publish a copy of the leaf in shared and
  // external storage alike, a move-only leaf publishes none.
  using shared_pointer_model =
      detail::binding_model<type_registration<scope<shared>, storage<A *>>>;
  using shared_pointer_resolutions =
      detail::binding_resolutions<A,
                                  typename shared_pointer_model::storage_type>;
  static_assert(std::is_same_v<resolution_targets_t<typename accepted_values<
                                   shared_pointer_resolutions>::type>,
                               type_list<A>>);
  using shared_move_only_pointer_model = detail::binding_model<
      type_registration<scope<shared>, storage<move_only *>>>;
  using shared_move_only_pointer_resolutions = detail::binding_resolutions<
      move_only, typename shared_move_only_pointer_model::storage_type>;
  static_assert(
      std::is_same_v<
          typename accepted_values<shared_move_only_pointer_resolutions>::type,
          type_list<>>);
  using external_unique_model = detail::binding_model<
      type_registration<scope<external>, storage<std::unique_ptr<A>>>>;
  using external_unique_resolutions =
      detail::binding_resolutions<A,
                                  typename external_unique_model::storage_type>;
  static_assert(std::is_same_v<resolution_targets_t<typename accepted_values<
                                   external_unique_resolutions>::type>,
                               type_list<A>>);
  using external_move_only_unique_model = detail::binding_model<
      type_registration<scope<external>, storage<std::unique_ptr<move_only>>>>;
  using external_move_only_unique_resolutions = detail::binding_resolutions<
      move_only, typename external_move_only_unique_model::storage_type>;
  static_assert(
      std::is_same_v<
          typename accepted_values<external_move_only_unique_resolutions>::type,
          type_list<>>);

  using shared_move_only_model = detail::binding_model<
      type_registration<scope<shared>, storage<move_only>>>;
  using shared_resolutions = detail::binding_resolutions<
      move_only, typename shared_move_only_model::storage_type>;
  static_assert(
      std::is_same_v<typename accepted_values<shared_resolutions>::type,
                     type_list<>>);
  static_assert(
      std::is_same_v<reference_resolution_targets_t<
                         typename shared_resolutions::object_resolutions>,
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
          typename accepted_values<shared_cyclical_handle_resolutions>::type>);
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
          resolution_targets_t<
              typename accepted_values<unique_resolutions>::type>,
          type_list<move_only, std::optional<move_only>,
                    std::unique_ptr<move_only>, std::shared_ptr<move_only>>>);

  using compatible_unique_handle_model =
      detail::binding_model<type_registration<
          scope<unique>, storage<std::unique_ptr<A>>, interfaces<I>>>;
  using compatible_unique_handle_resolutions = detail::binding_resolutions<
      I, typename compatible_unique_handle_model::storage_type>;
  static_assert(
      has_resolution_target_v<std::unique_ptr<I>,
                              typename accepted_values<
                                  compatible_unique_handle_resolutions>::type>);

  using incompatible_unique_handle_model =
      detail::binding_model<type_registration<
          scope<unique>, storage<std::unique_ptr<A, incompatible_deleter>>,
          interfaces<I>>>;
  using incompatible_unique_handle_resolutions = detail::binding_resolutions<
      I, typename incompatible_unique_handle_model::storage_type>;
  static_assert(!has_resolution_target_v<
                std::unique_ptr<I>,
                typename accepted_values<
                    incompatible_unique_handle_resolutions>::type>);

  using fresh_resolutions =
      detail::binding_resolutions<move_only, fresh_value_storage<move_only>>;
  static_assert(
      std::is_same_v<resolution_targets_t<
                         typename accepted_values<fresh_resolutions>::type>,
                     type_list<move_only>>);
  using immovable_fresh_resolutions =
      detail::binding_resolutions<immovable, fresh_value_storage<immovable>>;
  static_assert(std::is_same_v<
                typename accepted_values<immovable_fresh_resolutions>::type,
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
                typename accepted_values<shared_variant_resolutions>::type>);

  using shared_move_only_variant =
      std::variant<std::unique_ptr<move_only>, std::monostate>;
  using shared_move_only_variant_model = detail::binding_model<
      type_registration<scope<shared>, storage<shared_move_only_variant>>>;
  using shared_move_only_variant_resolutions = detail::binding_resolutions<
      std::unique_ptr<move_only>,
      typename shared_move_only_variant_model::storage_type>;
  static_assert(
      std::is_same_v<
          typename accepted_values<shared_move_only_variant_resolutions>::type,
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
                typename accepted_values<unique_variant_resolutions>::type>);
  static_assert(
      has_resolution_target_v<
          std::unique_ptr<move_only> &&,
          typename unique_variant_resolutions::rvalue_reference_resolutions>);

  using external_nested_move_only_pointer_model = detail::binding_model<
      type_registration<scope<external>, storage<std::optional<move_only> *>>>;
  using nested_resolutions = detail::binding_resolutions<
      move_only,
      typename external_nested_move_only_pointer_model::storage_type>;
  static_assert(
      std::is_same_v<typename accepted_values<nested_resolutions>::type,
                     type_list<>>);
  static_assert(
      std::is_same_v<pointer_resolution_targets_t<
                         typename nested_resolutions::object_resolutions>,
                     type_list<std::optional<move_only> *>>);

  using external_nested_move_only_reference_model =
      detail::binding_model<type_registration<
          scope<external>, storage<const std::optional<move_only> &>>>;
  using nested_reference_resolutions = detail::binding_resolutions<
      move_only,
      typename external_nested_move_only_reference_model::storage_type>;
  static_assert(std::is_same_v<
                typename accepted_values<nested_reference_resolutions>::type,
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
      std::array<A, 2>, typename const_array_pointer_model::storage_type>;
  static_assert(std::is_same_v<resolution_targets_t<typename accepted_values<
                                   const_array_pointer_resolutions>::type>,
                               type_list<std::array<A, 2>>>);
  static_assert(
      std::is_same_v<
          reference_resolution_targets_t<
              typename const_array_pointer_resolutions::object_resolutions>,
          type_list<const std::array<A, 2> &>>);
  static_assert(
      std::is_same_v<
          pointer_resolution_targets_t<
              typename const_array_pointer_resolutions::object_resolutions>,
          type_list<const std::array<A, 2> *>>);

  using unique_optional_pointer_model = detail::binding_model<
      type_registration<scope<unique>, storage<optional_pointer>>>;
  using unique_optional_pointer_resolutions = detail::binding_resolutions<
      A, typename unique_optional_pointer_model::storage_type>;
  static_assert(
      has_resolution_target_v<
          std::unique_ptr<std::optional<A>>,
          typename accepted_values<unique_optional_pointer_resolutions>::type>);
  static_assert(
      has_resolution_target_v<
          std::shared_ptr<std::optional<A>>,
          typename accepted_values<unique_optional_pointer_resolutions>::type>);
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
          pointer_resolution_targets_t<
              typename unique_optional_pointer_resolutions::object_resolutions>,
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

  static_assert(
      std::is_same_v<
          resolution_targets_t<typename accepted_values<resolutions>::type>,
          type_list<int>>);
  static_assert(std::is_same_v<reference_resolution_targets_t<
                                   typename resolutions::object_resolutions>,
                               type_list<const int &>>);
  static_assert(std::is_same_v<pointer_resolution_targets_t<
                                   typename resolutions::object_resolutions>,
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
                int, typename accepted_values<optional_resolutions>::type>);

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
     const_external_smart_pointer_storage_preserves_const_access) {
  struct service_interface {
    virtual ~service_interface() = default;
    virtual int get_value() const = 0;
  };
  struct service {
    int value;
  };
  struct interface_service : service_interface {
    explicit interface_service(int init_value) : value(init_value) {}

    int get_value() const override { return value; }

    int value;
  };

  const std::shared_ptr<const service> shared_value =
      std::make_shared<const service>(service{23});
  container<> shared_container;
  shared_container.register_type<
      scope<external>, storage<const std::shared_ptr<const service> &>>(
      shared_value);

  auto shared_copy = shared_container.resolve<std::shared_ptr<const service>>();
  EXPECT_EQ(shared_copy, shared_value);
  EXPECT_EQ(&shared_container.resolve<const std::shared_ptr<const service> &>(),
            &shared_value);
  EXPECT_EQ(&shared_container.resolve<const service &>(), shared_value.get());
  EXPECT_EQ(shared_container.resolve<const service *>(), shared_value.get());
  EXPECT_THROW((void)shared_container.resolve<service &>(),
               type_not_convertible_exception);
  EXPECT_THROW((void)shared_container.resolve<service *>(),
               type_not_convertible_exception);

  const std::shared_ptr<const interface_service> shared_interface_value =
      std::make_shared<const interface_service>(31);
  container<> shared_interface_container;
  shared_interface_container
      .register_type<scope<external>,
                     storage<const std::shared_ptr<const interface_service> &>,
                     interfaces<service_interface>>(shared_interface_value);

  auto shared_interface_copy =
      shared_interface_container
          .resolve<std::shared_ptr<const service_interface>>();
  EXPECT_EQ(shared_interface_copy.get(), shared_interface_value.get());
  EXPECT_EQ(&shared_interface_container.resolve<const service_interface &>(),
            shared_interface_value.get());
  EXPECT_EQ(shared_interface_container.resolve<const service_interface *>(),
            shared_interface_value.get());
  EXPECT_THROW((void)shared_interface_container.resolve<service_interface &>(),
               type_not_convertible_exception);
  EXPECT_THROW((void)shared_interface_container.resolve<service_interface *>(),
               type_not_convertible_exception);

  const std::unique_ptr<const service> unique_value =
      std::make_unique<const service>(service{29});
  container<> unique_container;
  unique_container.register_type<
      scope<external>, storage<const std::unique_ptr<const service> &>>(
      unique_value);

  EXPECT_EQ(&unique_container.resolve<const std::unique_ptr<const service> &>(),
            &unique_value);
  EXPECT_EQ(&unique_container.resolve<const service &>(), unique_value.get());
  EXPECT_EQ(unique_container.resolve<const service *>(), unique_value.get());
  EXPECT_THROW((void)unique_container.resolve<service &>(),
               type_not_convertible_exception);
  EXPECT_THROW((void)unique_container.resolve<service *>(),
               type_not_convertible_exception);
  EXPECT_THROW((void)unique_container.resolve<std::unique_ptr<const service>>(),
               type_not_convertible_exception);

  const std::unique_ptr<const interface_service> unique_interface_value =
      std::make_unique<const interface_service>(37);
  container<> unique_interface_container;
  unique_interface_container
      .register_type<scope<external>,
                     storage<const std::unique_ptr<const interface_service> &>,
                     interfaces<service_interface>>(unique_interface_value);

  EXPECT_EQ(&unique_interface_container.resolve<const service_interface &>(),
            unique_interface_value.get());
  EXPECT_EQ(unique_interface_container.resolve<const service_interface *>(),
            unique_interface_value.get());
  EXPECT_THROW((void)unique_interface_container.resolve<service_interface &>(),
               type_not_convertible_exception);
  EXPECT_THROW((void)unique_interface_container.resolve<service_interface *>(),
               type_not_convertible_exception);
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
  // Each shape is published by the interface it selects: the whole array by
  // itself, its rows by the row type.
  using whole_resolutions =
      detail::binding_resolutions<int[2][3], typename model::storage_type>;
  using row_resolutions =
      detail::binding_resolutions<int[3], typename model::storage_type>;
  static_assert(
      std::is_same_v<typename accepted_values<whole_resolutions>::type,
                     type_list<>>);
  static_assert(
      std::is_same_v<reference_resolution_targets_t<
                         typename whole_resolutions::object_resolutions>,
                     type_list<int (&)[2][3]>>);
  static_assert(
      std::is_same_v<pointer_resolution_targets_t<
                         typename whole_resolutions::object_resolutions>,
                     type_list<int (*)[2][3]>>);
  static_assert(
      std::is_same_v<pointer_resolution_targets_t<
                         typename row_resolutions::object_resolutions>,
                     type_list<int (*)[3]>>);

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

namespace abstract_interface_test {
struct I {
  virtual ~I() = default;
  virtual int value() const = 0;
};
struct A : I {
  int value() const override { return 7; }
};
// Reads the instance it is constructed with.
struct reader {
  explicit reader(I &instance) : value(instance.value()) {}
  int value;
};
} // namespace abstract_interface_test

TEST(type_registration_test, value_of_abstract_type_is_not_formable) {
  using abstract_interface_test::A;
  using abstract_interface_test::I;

  static_assert(!detail::is_formable_v<I>);
  static_assert(!detail::is_formable_v<const I>);
  static_assert(!detail::is_formable_v<std::optional<I>>);
  static_assert(!detail::is_formable_v<std::variant<A, I>>);
  static_assert(detail::is_formable_v<I &>);
  static_assert(detail::is_formable_v<I *>);
  static_assert(detail::is_formable_v<std::shared_ptr<I>>);
  static_assert(detail::is_formable_v<std::optional<A>>);
}

TEST(type_registration_test, scope_stores_reference_only_when_given_it) {
  struct A {};

  static_assert(detail::has_storage_cell_v<external, A &>);
  static_assert(!detail::has_storage_cell_v<unique, A &>);
  static_assert(!detail::has_storage_cell_v<shared, A &>);
  static_assert(!detail::has_storage_cell_v<shared_cyclical, A &>);
  static_assert(!detail::binding_model<
                type_registration<scope<shared>, storage<A &>>>::valid);
  static_assert(detail::binding_model<
                type_registration<scope<external>, storage<A &>>>::valid);
}

TEST(type_registration_test, cyclical_scope_does_not_store_dynamic_arrays) {
  struct A {};

  static_assert(!detail::has_storage_cell_v<shared_cyclical, A[]>);
  static_assert(
      !detail::has_storage_cell_v<shared_cyclical, std::shared_ptr<A[]>>);
  static_assert(detail::has_storage_cell_v<shared_cyclical, A[2]>);
  static_assert(
      detail::has_storage_cell_v<shared_cyclical, std::shared_ptr<A>>);
}

// The routes of an abstract interface over a value include no value wrapper
// of the interface, which cannot exist.
TEST(type_registration_test, abstract_interface_binds_over_value_storage) {
  using abstract_interface_test::A;
  using abstract_interface_test::I;

  using abstract_interface_test::reader;

  container<> unique_container;
  unique_container.register_type<scope<unique>, storage<A>, interfaces<I>>();
  unique_container.register_type<scope<shared>, storage<reader>>();
  EXPECT_EQ(unique_container.resolve<reader &>().value, 7);

  container<> optional_container;
  optional_container
      .register_type<scope<shared>, storage<std::optional<A>>, interfaces<I>>();
  EXPECT_EQ(optional_container.resolve<I &>().value(), 7);
  EXPECT_EQ(optional_container.resolve<I *>()->value(), 7);

  static_container<
      bindings<bind<scope<shared>, storage<std::optional<A>>, interfaces<I>>>>
      static_optional;
  EXPECT_EQ(static_optional.resolve<I &>().value(), 7);
}

// A unique instance lives in the resolution context: a dependency can refer
// to it, the caller of the container cannot.
TEST(type_registration_test,
     unique_instance_is_referred_to_only_by_what_a_scope_keeps) {
  using abstract_interface_test::A;
  using abstract_interface_test::I;
  using abstract_interface_test::reader;

  // A class a shared scope keeps keeps the unique instance it refers to.
  container<> runtime;
  runtime.register_type<scope<unique>, storage<A>, interfaces<I>>();
  runtime.register_type<scope<shared>, storage<reader>>();
  EXPECT_EQ(runtime.resolve<reader &>().value, 7);
  EXPECT_THROW(runtime.resolve<I &>(), type_not_convertible_exception);
  EXPECT_THROW(runtime.resolve<I *>(), type_not_convertible_exception);

  static_container<bindings<bind<scope<unique>, storage<A>, interfaces<I>>,
                            bind<scope<shared>, storage<reader>>>>
      compile_time;
  EXPECT_EQ(compile_time.resolve<reader &>().value, 7);

  // A class returned to the caller would refer to an instance that is gone
  // when the resolution ends.
  container<> returned;
  returned.register_type<scope<unique>, storage<A>, interfaces<I>>();
  returned.register_type<scope<unique>, storage<reader>>();
  EXPECT_THROW(returned.resolve<reader>(), type_not_convertible_exception);
  EXPECT_THROW(returned.construct<reader>(), type_not_convertible_exception);

  static_container<bindings<bind<scope<unique>, storage<A>, interfaces<I>>,
                            bind<scope<unique>, storage<reader>>>>
      returned_static;
  EXPECT_THROW(returned_static.resolve<reader>(),
               type_not_convertible_exception);
}

// A unique pointer is handed to the caller, not referred to in the context.
TEST(type_registration_test, unique_pointer_is_handed_to_caller) {
  using abstract_interface_test::A;

  container<> runtime;
  runtime.register_type<scope<unique>, storage<A *>>();
  std::unique_ptr<A> instance(runtime.resolve<A *>());
  EXPECT_EQ(instance->value(), 7);
}

// A resolved rvalue reference is returned by value, also when it is annotated.
TEST(type_registration_test, annotated_rvalue_request_returns_value) {
  struct tag {};
  struct A {};

  container<> runtime;
  runtime.register_type<scope<unique>, storage<A>,
                        interfaces<annotated<A, tag>>>();
  static_assert(
      std::is_same_v<decltype(runtime.resolve<annotated<A &&, tag>>()), A>);
  static_assert(
      std::is_same_v<decltype(runtime.resolve<annotated<A, tag> &&>()), A>);
  (void)runtime.resolve<annotated<A &&, tag>>();
}

TEST(type_registration_test, cyclical_scope_does_not_store_raw_pointers) {
  struct A {};

  static_assert(!detail::has_storage_cell_v<shared_cyclical, A *>);
  static_assert(detail::has_storage_cell_v<shared, A *>);
}

// A kept optional is copied as its element or as itself.
TEST(type_registration_test, kept_optional_is_copied_as_itself) {
  using abstract_interface_test::A;

  container<> runtime;
  runtime.register_type<scope<shared>, storage<std::optional<A>>>();
  std::optional<A> copy = runtime.resolve<std::optional<A>>();
  ASSERT_TRUE(copy.has_value());
  EXPECT_NE(&*copy, &*runtime.resolve<std::optional<A> &>());
}

// A cyclical bounded array is published by its first element, as a shared one.
TEST(type_registration_test, cyclical_bounded_array_publishes_its_rows) {
  struct A {};

  container<> runtime;
  runtime.register_type<scope<shared_cyclical>, storage<A[2]>>();
  A *rows = runtime.resolve<A *>();
  A(&whole)[2] = runtime.resolve<A(&)[2]>();
  EXPECT_EQ(rows, &whole[0]);
  EXPECT_EQ(rows, runtime.resolve<A *>());
}

namespace fresh_value_test {
struct I {
  virtual ~I() = default;
  virtual int value() const = 0;
};
struct A : I {
  int value() const override { return 7; }
};
struct move_only {
  move_only() = default;
  move_only(move_only &&) = default;
  move_only(const move_only &) = delete;
};
// Owns a new instance of its interface.
struct owner {
  explicit owner(std::unique_ptr<I> init) : instance(std::move(init)) {}
  std::unique_ptr<I> instance;
};
} // namespace fresh_value_test

// A new value of a unique scope is moved into an owning handle of itself or of
// its interface.
TEST(type_registration_test, unique_value_is_moved_into_owning_handle) {
  using fresh_value_test::A;
  using fresh_value_test::I;
  using fresh_value_test::owner;

  container<> runtime;
  runtime.register_type<scope<unique>, storage<A>, interfaces<A, I>>();
  runtime.register_type<scope<unique>, storage<owner>>();
  EXPECT_EQ(runtime.resolve<std::unique_ptr<A>>()->value(), 7);
  EXPECT_EQ(runtime.resolve<std::shared_ptr<I>>()->value(), 7);
  EXPECT_EQ(runtime.resolve<owner>().instance->value(), 7);

  static_container<bindings<bind<scope<unique>, storage<A>, interfaces<I>>,
                            bind<scope<unique>, storage<owner>>>>
      compile_time;
  EXPECT_EQ(compile_time.resolve<std::unique_ptr<I>>()->value(), 7);
  EXPECT_EQ(compile_time.resolve<owner>().instance->value(), 7);
}

// A new optional of a unique scope gives its element away.
TEST(type_registration_test, unique_optional_is_moved_out_of) {
  using fresh_value_test::move_only;

  container<> runtime;
  runtime.register_type<scope<unique>, storage<std::optional<move_only>>>();
  move_only value = runtime.resolve<move_only>();
  (void)value;
  static_assert(
      std::is_same_v<decltype(runtime.resolve<move_only &&>()), move_only>);
}

namespace operation_test {
struct A {
  int value() const { return 7; }
};
struct tag {};
// Takes its dependency as an annotated rvalue reference.
struct rvalue_consumer {
  explicit rvalue_consumer(annotated<A &&, tag> init) {
    A taken = static_cast<A &&>(init);
    value = taken.value();
  }
  int value = 0;
};
} // namespace operation_test

// construct<T &>() refers to an instance as resolve<T &>() does; nothing is
// constructed for it.
TEST(type_registration_test, construct_of_reference_resolves_it) {
  using operation_test::A;

  container<> kept;
  kept.register_type<scope<shared>, storage<std::optional<A>>>();
  EXPECT_EQ(&kept.construct<A &>(), &kept.resolve<A &>());
  EXPECT_EQ(&kept.construct<std::optional<A> &>(),
            &kept.resolve<std::optional<A> &>());

  container<> fresh;
  fresh.register_type<scope<unique>, storage<A>>();
  EXPECT_THROW(fresh.construct<A &>(), type_not_convertible_exception);
}

// A dependency of an annotated rvalue reference is made of the rvalue.
TEST(type_registration_test, annotated_rvalue_reference_dependency) {
  using operation_test::A;
  using operation_test::rvalue_consumer;
  using operation_test::tag;

  container<> runtime;
  runtime.register_type<scope<unique>, storage<A>,
                        interfaces<annotated<A, tag>>>();
  runtime.register_type<
      scope<unique>, storage<rvalue_consumer>,
      factory<constructor<rvalue_consumer(annotated<A &&, tag>)>>>();
  EXPECT_EQ(runtime.resolve<rvalue_consumer>().value, 7);
}

// A request with a key in its type is looked up under that key by the caller
// of a runtime container, as it is for a dependency.
TEST(type_registration_test, runtime_request_keyed_by_type) {
  using operation_test::A;
  using key = key_type<std::size_t, 1>;
  struct traits : runtime_container_traits {
    using lookup_definition_type = lookups<associative<std::size_t, A>>;
  };

  container<traits> runtime;
  runtime.register_type<scope<shared>, storage<A>, interfaces<A>>(
      key_value{std::size_t(1)});
  A &kept = runtime.resolve<dependency<A &, key>>();
  EXPECT_EQ(&kept, &runtime.resolve<A &>(std::size_t(1)));
  EXPECT_EQ((runtime.construct<dependency<A, key>>().value()), 7);
}

namespace kept_dependency_test {
// Counts the parts alive, so that a test sees whether a service refers to one
// that is gone.
struct part {
  part() { ++alive; }
  part(const part &) { ++alive; }
  ~part() { --alive; }

  int value = 7;
  inline static int alive = 0;
};

struct service {
  explicit service(part &p) : used(p) {}

  part &used;
};

template <typename Scope> void check_runtime() {
  {
    container<> runtime;
    runtime.register_type<scope<unique>, storage<part>>();
    runtime.register_type<scope<Scope>, storage<service>,
                          factory<constructor<service(part &)>>>();
    service &kept = runtime.resolve<service &>();
    EXPECT_EQ(part::alive, 1);
    EXPECT_EQ(kept.used.value, 7);
  }
  EXPECT_EQ(part::alive, 0);
}

template <typename Scope> void check_static() {
  {
    static_container<bindings<bind<scope<unique>, storage<part>>,
                              bind<scope<Scope>, storage<service>,
                                   factory<constructor<service(part &)>>>>>
        fixed;
    service &kept = fixed.template resolve<service &>();
    EXPECT_EQ(part::alive, 1);
    EXPECT_EQ(kept.used.value, 7);
  }
  EXPECT_EQ(part::alive, 0);
}
} // namespace kept_dependency_test

// A unique instance a kept one refers to is constructed for it, and lives as
// long as the container.
TEST(type_registration_test, kept_instance_keeps_unique_dependency) {
  using namespace kept_dependency_test;
  check_runtime<shared>();
  check_runtime<shared_cyclical>();
  check_static<shared>();
  check_static<shared_cyclical>();
}

namespace published_test {
template <typename Target, typename... Resolutions>
constexpr bool publishes(type_list<Resolutions...>) {
  return (std::is_same_v<typename Resolutions::target_type, Target> || ...);
}
} // namespace published_test

// A binding publishes only the requests that select it: a bounded array binds
// its rows and itself, and each of them serves its own requests.
TEST(type_registration_test, binding_publishes_only_requests_it_selects) {
  using published_test::publishes;
  struct A {};
  using model =
      detail::binding_model<type_registration<scope<shared>, storage<A[2]>>>;
  using storage_type = typename model::storage_type;
  using rows = typename detail::binding_resolutions<A, storage_type>::type;
  using whole = typename detail::binding_resolutions<A[2], storage_type>::type;

  static_assert(publishes<A *>(rows{}));
  static_assert(!publishes<A(*)[2]>(rows{}));
  static_assert(!publishes<A(&)[2]>(rows{}));
  static_assert(publishes<A(*)[2]>(whole{}));
  static_assert(!publishes<A *>(whole{}));
}

// A pointer to a bounded array is not constructed: new cannot make one, so
// construct resolves it.
TEST(type_registration_test, construct_of_pointer_to_bounded_array_resolves) {
  struct A {};

  container<> runtime;
  runtime.register_type<scope<shared>, storage<A[2]>>();
  A(*constructed)[2] = runtime.construct<A(*)[2]>();
  EXPECT_EQ(constructed, &runtime.resolve<A(&)[2]>());
}

// A handle of a dynamic array is made of the bound value by both containers;
// a default factory has no size to construct one with.
TEST(type_registration_test, construct_of_dynamic_array_handle_holds_value) {
  using operation_test::A;

  container<> runtime;
  runtime.register_type<scope<unique>, storage<A>>();
  std::shared_ptr<A[]> made = runtime.construct<std::shared_ptr<A[]>>();
  EXPECT_EQ(made[0].value(), 7);

  static_container<bindings<bind<scope<unique>, storage<A>>>> fixed;
  std::shared_ptr<A[]> fixed_made = fixed.construct<std::shared_ptr<A[]>>();
  EXPECT_EQ(fixed_made[0].value(), 7);
}

// invoke returns what the callable returns, a reference included.
TEST(type_registration_test, invoke_returns_what_the_callable_returns) {
  using operation_test::A;

  container<> runtime;
  runtime.register_type<scope<shared>, storage<A>>();
  A &kept = runtime.invoke([](A &a) -> A & { return a; });
  EXPECT_EQ(&kept, &runtime.resolve<A &>());

  static_container<bindings<bind<scope<shared>, storage<A>>>> fixed;
  A &fixed_kept = fixed.invoke([](A &a) -> A & { return a; });
  EXPECT_EQ(&fixed_kept, &fixed.resolve<A &>());
}

namespace detected_wrapper_test {
struct A {
  A() {}
  int value = 7;
};
// An aggregate that can be neither copied nor moved.
struct immovable {
  const std::unique_ptr<A> held;
};
} // namespace detected_wrapper_test

// invoke calls a callable directly, so a parameter of a type that can be
// neither copied nor moved compiles; such a value is not auto-constructed.
TEST(type_registration_test, invoke_takes_immovable_parameter) {
  using namespace detected_wrapper_test;

  container<> runtime;
  runtime.register_type<scope<unique>, storage<A>>();
  EXPECT_THROW(runtime.invoke([](immovable) { return 0; }),
               type_not_found_exception);
}

namespace annotated_construct_test {
struct I {
  virtual ~I() = default;
  virtual int value() const = 0;
};
struct A : I {
  int value() const override { return 7; }
};
struct tag {};
} // namespace annotated_construct_test

// construct of an annotated pointer to an abstract interface refers to the
// instance, as for a plain one; nothing abstract is constructed.
TEST(type_registration_test, construct_of_annotated_abstract_pointer) {
  using namespace annotated_construct_test;
  using request = annotated<I *, tag>;

  container<> runtime;
  runtime.register_type<scope<shared>, storage<A>,
                        interfaces<annotated<I, tag>>>();
  I *constructed = runtime.construct<request>();
  EXPECT_EQ(constructed, static_cast<I *>(runtime.resolve<request>()));
  EXPECT_EQ(constructed->value(), 7);

  static_container<
      bindings<bind<scope<shared>, storage<A>, interfaces<annotated<I, tag>>>>>
      fixed;
  I *fixed_constructed = fixed.construct<request>();
  EXPECT_EQ(fixed_constructed, static_cast<I *>(fixed.resolve<request>()));
}

// A shared_ptr of a bounded array owns the array, publishes its rows, and is
// made without a size from its factory.
TEST(type_registration_test, shared_pointer_of_bounded_array) {
  using operation_test::A;
  using handle = std::shared_ptr<A[2]>;

  container<> runtime;
  runtime.register_type<scope<shared>, storage<handle>>();
  handle &kept = runtime.resolve<handle &>();
  EXPECT_EQ(runtime.resolve<A *>(), &kept[0]);
  EXPECT_EQ(kept[1].value(), 7);

  static_container<bindings<bind<scope<unique>, storage<handle>>>> fixed;
  handle made = fixed.resolve<handle>();
  EXPECT_EQ(made[1].value(), 7);
}

namespace immovable_unique_test {
struct immovable {
  immovable() {}
  immovable(const immovable &) = delete;
  immovable &operator=(const immovable &) = delete;
  int value = 7;
};
} // namespace immovable_unique_test

// A unique value a reference refers to is placed in the context of the
// resolution in place, so a value that can be neither copied nor moved is
// placed too.
TEST(type_registration_test, reference_to_immovable_unique_value) {
  using namespace immovable_unique_test;
  auto read = [](immovable &value) { return value.value; };

  container<> runtime;
  runtime.register_type<scope<unique>, storage<immovable>>();
  EXPECT_EQ(runtime.invoke(read), 7);

  static_container<bindings<bind<scope<unique>, storage<immovable>>>> fixed;
  EXPECT_EQ(fixed.invoke(read), 7);
}

namespace undetected_construct_test {
struct ambiguous {
  ambiguous(int) {}
  ambiguous(float) {}
};
using alternative = std::variant<ambiguous, int>;
alternative make() { return alternative(std::in_place_type<int>, 7); }
} // namespace undetected_construct_test

// The runtime container learns only at run time whether a type is bound: a
// type its default factory cannot construct is not found when it is not, and
// resolves when it is, as in the static container.
TEST(type_registration_test, runtime_construct_of_undetected_type) {
  using namespace undetected_construct_test;

  container<> runtime;
  EXPECT_THROW(runtime.construct<ambiguous>(), type_not_found_exception);
  runtime.register_type<scope<shared>, storage<alternative>,
                        factory<function<&make>>>();
  EXPECT_EQ(std::get<int>(runtime.construct<alternative>()), 7);

  static_container<bindings<
      bind<scope<shared>, storage<alternative>, factory<function<&make>>>>>
      fixed;
  EXPECT_EQ(std::get<int>(fixed.construct<alternative>()), 7);
}

namespace keyed_array_test {
struct A {
  A() {}
  int value = 7;
};
struct element_lookup : runtime_container_traits {
  using lookup_definition_type = lookups<associative<std::size_t, A>>;
};
struct array_lookups : runtime_container_traits {
  using lookup_definition_type =
      lookups<associative<std::size_t, A[2]>, associative<std::size_t, A>>;
};
} // namespace keyed_array_test

// A lookup is defined for the interface it names: an array and its element
// are two interfaces of a registration of an array, each with its own lookup.
// A keyed request selects the lookup of its interface, or else that of its
// normalized one.
TEST(type_registration_test, keyed_registration_of_array) {
  using namespace keyed_array_test;

  container<array_lookups> both;
  both.register_type<scope<shared>, storage<A[2]>>(key_value{std::size_t(1)});
  EXPECT_EQ(both.resolve<A(&)[2]>(key_value{std::size_t(1)})[1].value, 7);
  EXPECT_EQ(both.resolve<A *>(key_value{std::size_t(1)})->value, 7);
  EXPECT_THROW(both.resolve<A(&)[2]>(), type_not_found_exception);

  container<element_lookup> element;
  element.register_type<scope<shared>, storage<A[2]>>(
      key_value{std::size_t(1)});
  EXPECT_EQ(element.resolve<A *>(key_value{std::size_t(1)})->value, 7);
  EXPECT_THROW(element.resolve<A(&)[2]>(key_value{std::size_t(1)}),
               type_not_convertible_exception);
}

namespace construct_rvalue_test {
struct A {
  A() {}
  int value = 7;
};
} // namespace construct_rvalue_test

// construct<T &&>() resolves the request, as resolve<T &&>() does: it moves
// out of an instance made for it, and is refused one the container keeps.
TEST(type_registration_test, construct_of_rvalue_reference_resolves_it) {
  using construct_rvalue_test::A;

  container<> made;
  made.register_type<scope<unique>, storage<A>>();
  EXPECT_EQ(made.construct<A &&>().value, 7);
  EXPECT_EQ(made.construct<std::unique_ptr<A> &&>()->value, 7);

  container<> kept;
  kept.register_type<scope<shared>, storage<A>>();
  EXPECT_THROW(kept.construct<A &&>(), type_not_convertible_exception);

  static_container<bindings<bind<scope<unique>, storage<A>>>> fixed;
  EXPECT_EQ(fixed.construct<A &&>().value, 7);
}
