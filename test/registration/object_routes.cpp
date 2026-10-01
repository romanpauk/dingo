//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "type_registration_common.h"

#include "support/custom_wrappers.h"

#include <array>
#include <memory>
#include <optional>
#include <variant>

// A binding publishes one route to an object for the reference and the pointer
// request of that object, and for the value request that copies it. These
// checks compare the object routes of a binding with the reference, pointer and
// value routes the storage lists describe, which are planned separately here,
// and show that merging them leaves the set of accepted requests unchanged.

namespace {
struct base_interface {
  virtual ~base_interface() = default;
};
struct other_interface {
  virtual ~other_interface() = default;
};
struct leaf : base_interface, other_interface {};
struct other_leaf {};

template <typename Resolutions> struct reference_requests;
template <typename Resolutions> struct pointer_requests;
template <typename Resolutions> struct value_requests;

template <typename... Resolutions>
struct reference_requests<type_list<Resolutions...>> {
  template <typename Resolution>
  using request = std::conditional_t<
      std::is_lvalue_reference_v<typename Resolution::target_type>,
      type_list<typename Resolution::target_type>,
      std::conditional_t<
          Resolution::serves_reference,
          type_list<detail::resolution_reference_target_t<Resolution>>,
          type_list<>>>;

  using type = type_list_cat_t<request<Resolutions>...>;
};

template <typename... Resolutions>
struct pointer_requests<type_list<Resolutions...>> {
  template <typename Resolution>
  using request =
      std::conditional_t<std::is_pointer_v<typename Resolution::target_type>,
                         type_list<typename Resolution::target_type>,
                         type_list<>>;

  using type = type_list_cat_t<request<Resolutions>...>;
};

// The value targets the object routes accept through the copy of their object.
template <typename... Resolutions>
struct value_requests<type_list<Resolutions...>> {
  template <typename Resolution>
  using request = std::conditional_t<
      Resolution::serves_value,
      type_list<detail::resolution_value_target_t<Resolution>>, type_list<>>;

  using type = type_list_cat_t<request<Resolutions>...>;
};

template <typename Resolutions> struct targets;

template <typename... Resolutions> struct targets<type_list<Resolutions...>> {
  using type = type_list<typename Resolutions::target_type...>;
};

template <typename Left, typename Right> struct same_set : std::false_type {};

template <typename... Left, typename... Right>
struct same_set<type_list<Left...>, type_list<Right...>>
    : std::bool_constant<
          sizeof...(Left) == sizeof...(Right) &&
          (type_list_contains_v<Left, type_list<Right...>> && ...) &&
          (type_list_contains_v<Right, type_list<Left...>> && ...)> {};

// The conversion of a resolution.
template <typename Resolution> struct resolution_conversion;

template <typename Target, typename Source, typename Conversion, bool Reference,
          bool Value>
struct resolution_conversion<
    resolution<Target, detail::type_resolution<Target, Source, Conversion>,
               Reference, Value>> {
  using source = Source;
  using conversion = Conversion;
};

// The conversions that only reach an object the source already holds.
template <typename Conversion> struct is_default_borrow : std::false_type {};

template <typename Target, typename Source, typename Access, typename Argument>
struct is_default_borrow<
    detail::traits_type_conversion<Target, Source, Access, Argument>>
    : std::bool_constant<
          detail::is_default_type_conversion<type_conversion_traits<
              Target,
              std::remove_cv_t<std::remove_reference_t<Source>>>>::value> {};

template <typename Target, typename Source>
struct is_default_borrow<detail::address_type_conversion<Target, Source>>
    : std::true_type {};

template <typename Target, typename Source, typename Conversion>
struct is_default_borrow<
    detail::dereference_type_conversion<Target, Source, Conversion>>
    : is_default_borrow<Conversion> {};

// A route that serves a reference request in place of a reference route must
// replace a route that borrows the same object from the same source.
template <typename Shared, typename ReferenceRoutes, typename PointerRoutes>
struct shared_route_replaces_reference_route : std::false_type {};

template <typename Shared, typename... References, typename... Pointers>
struct shared_route_replaces_reference_route<Shared, type_list<References...>,
                                             type_list<Pointers...>> {
private:
  using object = std::remove_pointer_t<typename Shared::target_type>;
  using pointer_route =
      resolution<typename Shared::target_type, typename Shared::operation>;
  using shared = resolution_conversion<Shared>;

  template <typename Reference>
  static constexpr bool replaces =
      std::is_same_v<typename Reference::target_type, object &> &&
      std::is_same_v<typename resolution_conversion<Reference>::source,
                     typename shared::source> &&
      is_default_borrow<
          typename resolution_conversion<Reference>::conversion>::value;

public:
  static constexpr bool value =
      type_list_contains_v<pointer_route, type_list<Pointers...>> &&
      is_default_borrow<typename shared::conversion>::value &&
      (replaces<References> || ...);
};

// The identity copy of a stored object: copied from an lvalue source, or from
// the pointee of a pointer source.
template <typename Value, typename Source, typename Conversion>
inline constexpr bool is_identity_copy_v = false;

template <typename Value, typename Source>
inline constexpr bool is_identity_copy_v<
    Value, Source, detail::identity_type_conversion<Value, Source>> = true;

template <typename Value, typename Source>
inline constexpr bool is_identity_copy_v<
    Value, Source *,
    detail::dereference_type_conversion<
        Value, Source *, detail::identity_type_conversion<Value, Source &>>> =
    true;

// A route that serves the copy of its object in place of a value route must
// replace the identity copy of the same source.
template <typename Shared, typename ValueRoutes>
struct shared_route_replaces_value_route : std::false_type {};

template <typename Shared, typename... Values>
struct shared_route_replaces_value_route<Shared, type_list<Values...>> {
private:
  using value = detail::resolution_value_target_t<Shared>;
  using shared = resolution_conversion<Shared>;

  template <typename Value>
  static constexpr bool replaces =
      std::is_same_v<typename Value::target_type, value> &&
      std::is_same_v<typename resolution_conversion<Value>::source,
                     typename shared::source> &&
      is_identity_copy_v<value, typename shared::source,
                         typename resolution_conversion<Value>::conversion>;

public:
  static constexpr bool value_replaced = (replaces<Values> || ...);
};

// The type-erased requests that bindings are asked: every qualification of a
// value, reference, rvalue reference and pointer of a type.
template <typename T>
using request_variants =
    type_list<T, const T, volatile T, const volatile T, T &, const T &,
              volatile T &, const volatile T &, T &&, const T &&, T *,
              const T *, volatile T *, const volatile T *, T *const,
              const T *const, T **, const T **, const T *const *, T *const *>;

using requested_types =
    type_list_cat_t<request_variants<leaf>, request_variants<base_interface>,
                    request_variants<other_interface>,
                    request_variants<other_leaf>, request_variants<int>,
                    request_variants<std::shared_ptr<leaf>>,
                    request_variants<std::shared_ptr<base_interface>>,
                    request_variants<std::unique_ptr<leaf>>,
                    request_variants<std::optional<leaf>>,
                    request_variants<std::variant<leaf, other_leaf>>,
                    request_variants<leaf[2]>>;

template <typename... Requests>
std::array<type_descriptor, sizeof...(Requests)>
describe_requests(type_list<Requests...>) {
  return {describe_type<Requests>()...};
}

template <typename Routes> bool accepts(const type_descriptor &requested_type) {
  return detail::resolution_request_index(Routes{}, requested_type) !=
         type_list_size_v<Routes>;
}

template <typename Interface, typename Storage> struct object_route_check {
private:
  using shape = detail::storage_shape_t<Storage>;
  using conversions = typename Storage::conversions;
  using interface_routes = detail::interface_resolutions<Interface, shape>;

  using reference_routes =
      type_list_merge_t<typename interface_routes::lvalue_reference_resolutions,
                        detail::storage_resolutions_t<
                            typename conversions::lvalue_reference_types,
                            Interface, shape, borrow>>;
  using pointer_routes = type_list_merge_t<
      typename interface_routes::pointer_resolutions,
      detail::storage_resolutions_t<typename conversions::pointer_types,
                                    Interface, shape, borrow>>;
  using object_routes =
      typename detail::binding_object_resolutions<Interface, shape>::type;
  // Every value route the storage lists describe, and the value routes that
  // are published beside the object routes.
  using value_routes =
      typename detail::binding_value_resolutions<Interface, shape>::type;
  using published_value_routes =
      typename detail::binding_published_value_resolutions<Interface,
                                                           shape>::type;

  using rvalue_routes =
      typename detail::binding_rvalue_reference_resolutions<Interface,
                                                            shape>::type;
  // The routes as the separate reference, pointer and value routes would list
  // them, and as the binding publishes them.
  using separate_routes = type_list_cat_t<value_routes, reference_routes,
                                          rvalue_routes, pointer_routes>;
  using published_routes =
      typename detail::binding_resolutions<Interface, Storage>::type;

  template <typename Resolution> static constexpr bool shared_route_is_sound() {
    bool sound = true;
    if constexpr (Resolution::serves_reference) {
      sound =
          sound &&
          shared_route_replaces_reference_route<Resolution, reference_routes,
                                                pointer_routes>::value;
    }
    if constexpr (Resolution::serves_value) {
      sound = sound &&
              shared_route_replaces_value_route<Resolution,
                                                value_routes>::value_replaced;
    }
    return sound;
  }

  template <typename... Routes>
  static constexpr bool all_shared_routes_are_sound(type_list<Routes...>) {
    return (shared_route_is_sound<Routes>() && ...);
  }

public:
  // Reference and pointer requests are accepted by the same targets.
  static constexpr bool same_reference_targets =
      same_set<typename reference_requests<object_routes>::type,
               typename targets<reference_routes>::type>::value;
  static constexpr bool same_pointer_targets =
      same_set<typename pointer_requests<object_routes>::type,
               typename targets<pointer_routes>::type>::value;
  // The values are accepted by the same targets.
  using accepted_values =
      type_list_cat_t<typename targets<published_value_routes>::type,
                      typename value_requests<object_routes>::type>;
  static constexpr bool same_value_targets =
      same_set<accepted_values, typename targets<value_routes>::type>::value;
  // A route is published once, not once per category.
  static constexpr bool routes_are_unique =
      std::is_same_v<object_routes, type_list_unique_t<object_routes>> &&
      std::is_same_v<published_value_routes,
                     type_list_unique_t<published_value_routes>>;
  // A shared route is the pointer route of an object whose reference route
  // borrows the same object from the same source.
  static constexpr bool shared_routes_are_sound =
      all_shared_routes_are_sound(object_routes{});

  static constexpr bool value = same_reference_targets &&
                                same_pointer_targets && same_value_targets &&
                                routes_are_unique && shared_routes_are_sound;

  // A type-erased request is accepted by the same routes: the descriptor of the
  // request is matched against the object route in place of the routes it
  // replaces.
  static bool accepts_the_same_erased_requests() {
    for (const auto &requested_type : describe_requests(requested_types{})) {
      if (accepts<separate_routes>(requested_type) !=
          accepts<published_routes>(requested_type)) {
        return false;
      }
    }
    return true;
  }

  static constexpr std::size_t published = type_list_size_v<object_routes>;
  static constexpr std::size_t separate =
      type_list_size_v<reference_routes> + type_list_size_v<pointer_routes>;
};

template <typename Scope, typename Type, typename... Interfaces>
struct binding_object_routes {
  using model =
      detail::binding_model<type_registration<scope<Scope>, storage<Type>,
                                              interfaces<Interfaces...>>>;
  using storage_type = typename model::storage_type;

  static constexpr bool value =
      (object_route_check<Interfaces, storage_type>::value && ...);
};

template <typename Scope, typename Type, typename... Interfaces>
inline constexpr bool object_routes_match_v =
    binding_object_routes<Scope, Type, Interfaces...>::value;

template <typename Scope, typename Type, typename... Interfaces>
bool erased_requests_match() {
  using storage_type =
      typename binding_object_routes<Scope, Type, Interfaces...>::storage_type;
  return (
      object_route_check<Interfaces,
                         storage_type>::accepts_the_same_erased_requests() &&
      ...);
}

// plain values and references
static_assert(object_routes_match_v<shared, leaf, leaf>);
static_assert(object_routes_match_v<shared, leaf, base_interface>);
static_assert(
    object_routes_match_v<shared, leaf, leaf, base_interface, other_interface>);
static_assert(object_routes_match_v<shared, other_leaf, other_leaf>);
static_assert(object_routes_match_v<shared_cyclical, leaf, leaf, base_interface,
                                    other_interface>);
static_assert(object_routes_match_v<unique, leaf, leaf, base_interface>);
static_assert(object_routes_match_v<external, leaf, leaf>);
static_assert(object_routes_match_v<external, leaf &, leaf, base_interface>);
static_assert(
    object_routes_match_v<external, const leaf &, leaf, base_interface>);

// raw pointers
static_assert(object_routes_match_v<external, leaf *, leaf>);
static_assert(object_routes_match_v<external, leaf *, leaf, base_interface,
                                    other_interface>);
static_assert(
    object_routes_match_v<external, const leaf *, leaf, base_interface>);
static_assert(object_routes_match_v<shared, leaf *, leaf, base_interface>);
static_assert(object_routes_match_v<unique, leaf *, leaf, base_interface>);

// wrappers
static_assert(object_routes_match_v<shared, std::shared_ptr<leaf>, leaf>);
static_assert(object_routes_match_v<shared, std::shared_ptr<leaf>, leaf,
                                    base_interface, other_interface>);
static_assert(object_routes_match_v<shared, std::shared_ptr<leaf>,
                                    std::shared_ptr<base_interface>>);
static_assert(object_routes_match_v<shared, std::shared_ptr<const leaf>, leaf,
                                    base_interface>);
static_assert(
    object_routes_match_v<shared, std::unique_ptr<leaf>, leaf, base_interface>);
static_assert(object_routes_match_v<shared, std::optional<leaf>, leaf>);
static_assert(object_routes_match_v<external, std::shared_ptr<leaf>, leaf,
                                    base_interface>);
static_assert(object_routes_match_v<external, std::unique_ptr<leaf>, leaf,
                                    base_interface>);
static_assert(object_routes_match_v<external, std::optional<leaf>, leaf>);
static_assert(object_routes_match_v<external, std::optional<leaf> *, leaf>);
static_assert(object_routes_match_v<external, std::shared_ptr<leaf> *, leaf,
                                    base_interface>);
static_assert(
    object_routes_match_v<unique, std::shared_ptr<leaf>, leaf, base_interface>);
static_assert(
    object_routes_match_v<unique, std::unique_ptr<leaf>, leaf, base_interface>);
static_assert(object_routes_match_v<unique, std::optional<leaf>, leaf>);
static_assert(object_routes_match_v<shared_cyclical, std::shared_ptr<leaf>,
                                    leaf, base_interface>);

// alternatives
static_assert(object_routes_match_v<shared, std::variant<leaf, other_leaf>,
                                    leaf, other_leaf>);
static_assert(object_routes_match_v<external, std::variant<leaf, other_leaf>,
                                    leaf, other_leaf>);
static_assert(
    object_routes_match_v<external, const std::variant<leaf, other_leaf> &,
                          leaf, other_leaf>);
static_assert(object_routes_match_v<unique, std::variant<leaf, other_leaf>,
                                    leaf, other_leaf>);

// arrays
static_assert(object_routes_match_v<shared, leaf[2], leaf>);
static_assert(object_routes_match_v<shared, int[2][3], int>);
static_assert(object_routes_match_v<external, leaf[2], leaf>);
static_assert(object_routes_match_v<shared, std::shared_ptr<leaf[]>, leaf>);
static_assert(object_routes_match_v<shared, std::unique_ptr<leaf[]>, leaf>);
static_assert(object_routes_match_v<external, std::shared_ptr<leaf[]>, leaf>);
static_assert(object_routes_match_v<unique, std::unique_ptr<leaf[]>, leaf>);

// custom wrappers
static_assert(object_routes_match_v<shared, test_shared<leaf>, leaf>);
static_assert(
    object_routes_match_v<shared, test_shared<leaf>, leaf, base_interface>);
static_assert(
    object_routes_match_v<external, test_unique<leaf>, leaf, base_interface>);
static_assert(object_routes_match_v<shared, test_optional<leaf>, leaf>);
static_assert(object_routes_match_v<external, test_optional<leaf>, leaf>);
static_assert(
    object_routes_match_v<unique, test_unique<leaf>, leaf, base_interface>);

TEST(object_routes_test, bindings_accept_the_same_erased_requests) {
  // plain values and references
  EXPECT_TRUE((erased_requests_match<shared, leaf, leaf>()));
  EXPECT_TRUE(
      (erased_requests_match<shared, leaf, base_interface, other_interface>()));
  EXPECT_TRUE((erased_requests_match<shared, other_leaf, other_leaf>()));
  EXPECT_TRUE(
      (erased_requests_match<shared_cyclical, leaf, leaf, base_interface>()));
  EXPECT_TRUE((erased_requests_match<unique, leaf, leaf, base_interface>()));
  EXPECT_TRUE((erased_requests_match<external, leaf, leaf>()));
  EXPECT_TRUE(
      (erased_requests_match<external, leaf &, leaf, base_interface>()));
  EXPECT_TRUE(
      (erased_requests_match<external, const leaf &, leaf, base_interface>()));

  // raw pointers
  EXPECT_TRUE((erased_requests_match<external, leaf *, leaf, base_interface,
                                     other_interface>()));
  EXPECT_TRUE(
      (erased_requests_match<external, const leaf *, leaf, base_interface>()));
  EXPECT_TRUE((erased_requests_match<shared, leaf *, leaf, base_interface>()));

  // wrappers
  EXPECT_TRUE((erased_requests_match<shared, std::shared_ptr<leaf>, leaf,
                                     base_interface>()));
  EXPECT_TRUE((erased_requests_match<shared, std::shared_ptr<leaf>,
                                     std::shared_ptr<base_interface>>()));
  EXPECT_TRUE((erased_requests_match<shared, std::shared_ptr<const leaf>, leaf,
                                     base_interface>()));
  EXPECT_TRUE((erased_requests_match<shared, std::unique_ptr<leaf>, leaf,
                                     base_interface>()));
  EXPECT_TRUE((erased_requests_match<shared, std::optional<leaf>, leaf>()));
  EXPECT_TRUE((erased_requests_match<external, std::shared_ptr<leaf>, leaf,
                                     base_interface>()));
  EXPECT_TRUE((erased_requests_match<external, std::optional<leaf> *, leaf>()));
  EXPECT_TRUE((erased_requests_match<unique, std::unique_ptr<leaf>, leaf,
                                     base_interface>()));
  EXPECT_TRUE((erased_requests_match<unique, std::optional<leaf>, leaf>()));
  EXPECT_TRUE((erased_requests_match<shared_cyclical, std::shared_ptr<leaf>,
                                     leaf, base_interface>()));

  // alternatives and arrays
  EXPECT_TRUE((erased_requests_match<shared, std::variant<leaf, other_leaf>,
                                     leaf, other_leaf>()));
  EXPECT_TRUE((erased_requests_match<external, std::variant<leaf, other_leaf>,
                                     leaf, other_leaf>()));
  EXPECT_TRUE((erased_requests_match<shared, leaf[2], leaf>()));
  EXPECT_TRUE((erased_requests_match<shared, int[2][3], int>()));
  EXPECT_TRUE((erased_requests_match<shared, std::shared_ptr<leaf[]>, leaf>()));
  EXPECT_TRUE((erased_requests_match<unique, std::unique_ptr<leaf[]>, leaf>()));

  // custom wrappers
  EXPECT_TRUE((erased_requests_match<shared, test_shared<leaf>, leaf,
                                     base_interface>()));
  EXPECT_TRUE((erased_requests_match<external, test_unique<leaf>, leaf,
                                     base_interface>()));
  EXPECT_TRUE((erased_requests_match<shared, test_optional<leaf>, leaf>()));
}

TEST(object_routes_test, a_pointer_route_serves_the_reference_of_its_object) {
  using model =
      detail::binding_model<type_registration<scope<shared>, storage<leaf>>>;
  using routes = typename detail::binding_object_resolutions<
      leaf, detail::storage_shape_t<typename model::storage_type>>::type;

  static_assert(type_list_size_v<routes> == 1);
  using route = type_list_head_t<routes>;
  static_assert(std::is_same_v<typename route::target_type, leaf *>);
  static_assert(route::serves_reference);
  static_assert(detail::is_resolution_request_for_v<route, leaf &>);
  static_assert(detail::is_resolution_request_for_v<route, const leaf &>);
  static_assert(detail::is_resolution_request_for_v<route, leaf *>);
  static_assert(detail::is_resolution_request_for_v<route, const leaf *>);
  static_assert(!detail::is_resolution_request_for_v<route, leaf>);
  static_assert(!detail::is_resolution_request_for_v<route, leaf &&>);
  static_assert(!detail::is_resolution_request_for_v<route, other_leaf &>);

  EXPECT_TRUE(
      (detail::matches_resolution_request<route>(describe_type<leaf &>())));
  EXPECT_TRUE((detail::matches_resolution_request<route>(
      describe_type<const leaf &>())));
  EXPECT_TRUE(
      (detail::matches_resolution_request<route>(describe_type<leaf *>())));
  EXPECT_TRUE((detail::matches_resolution_request<route>(
      describe_type<const leaf *>())));
  // The copy of the object is served too, the move of it is not.
  static_assert(route::serves_value);
  EXPECT_TRUE(
      (detail::matches_resolution_request<route>(describe_type<leaf>())));
  EXPECT_TRUE(
      (detail::matches_resolution_request<route>(describe_type<const leaf>())));
  EXPECT_FALSE(
      (detail::matches_resolution_request<route>(describe_type<other_leaf>())));
  EXPECT_FALSE(
      (detail::matches_resolution_request<route>(describe_type<leaf &&>())));
  EXPECT_FALSE((detail::matches_resolution_request<route>(
      describe_type<other_leaf &>())));
  EXPECT_FALSE((detail::matches_resolution_request<route>(
      describe_type<other_leaf *>())));
}

TEST(object_routes_test, a_const_object_does_not_serve_a_mutable_request) {
  using model = detail::binding_model<
      type_registration<scope<external>, storage<const leaf &>>>;
  using routes = typename detail::binding_object_resolutions<
      leaf, detail::storage_shape_t<typename model::storage_type>>::type;

  static_assert(type_list_size_v<routes> == 1);
  using route = type_list_head_t<routes>;
  static_assert(std::is_same_v<typename route::target_type, const leaf *>);

  EXPECT_FALSE(
      (detail::matches_resolution_request<route>(describe_type<leaf &>())));
  EXPECT_FALSE(
      (detail::matches_resolution_request<route>(describe_type<leaf *>())));
  EXPECT_TRUE((detail::matches_resolution_request<route>(
      describe_type<const leaf &>())));
  EXPECT_TRUE((detail::matches_resolution_request<route>(
      describe_type<const leaf *>())));
}

TEST(object_routes_test, wrapper_leaf_routes_keep_a_route_per_category) {
  using model = detail::binding_model<
      type_registration<scope<shared>, storage<std::shared_ptr<leaf>>>>;
  using routes = typename detail::binding_object_resolutions<
      leaf, detail::storage_shape_t<typename model::storage_type>>::type;

  // The handle is an object of the storage and shares its route, the leaf is
  // reached through the wrapper traits and keeps a route per category.
  static_assert(type_list_size_v<routes> == 3);
  static_assert(type_list_contains_v<std::shared_ptr<leaf> *,
                                     typename targets<routes>::type>);
  static_assert(type_list_contains_v<leaf &, typename targets<routes>::type>);
  static_assert(type_list_contains_v<leaf *, typename targets<routes>::type>);
}

// The behavior of requests served by an object route.
struct copy_counted {
  copy_counted() = default;
  copy_counted(const copy_counted &other) : copies(other.copies + 1) {}

  int copies = 0;
};

struct move_only_object {
  move_only_object() = default;
  move_only_object(move_only_object &&) = default;
  move_only_object &operator=(move_only_object &&) = default;
};

struct custom_copy {
  custom_copy() : value(1) {}

  int value;
};
} // namespace

namespace dingo {
template <> struct type_conversion_traits<custom_copy, custom_copy> {
  template <typename> using required_access = borrow;

  static custom_copy convert(const custom_copy &source) {
    custom_copy result;
    result.value = source.value + 10;
    return result;
  }
};
} // namespace dingo

namespace {
TEST(object_routes_test, one_route_serves_reference_pointer_and_copy) {
  container<> container;
  container.register_type<scope<shared>, storage<copy_counted>>();

  auto &reference = container.resolve<copy_counted &>();
  EXPECT_EQ(&reference, container.resolve<copy_counted *>());
  EXPECT_EQ(&reference, &container.resolve<const copy_counted &>());
  EXPECT_EQ(&reference, container.resolve<const copy_counted *>());

  // The copy is taken from the stored object once.
  const auto copy = container.resolve<copy_counted>();
  EXPECT_EQ(copy.copies, 1);
  EXPECT_NE(&copy, &reference);
  EXPECT_EQ(container.resolve<const copy_counted>().copies, 1);
  EXPECT_THROW(container.resolve<copy_counted &&>(),
               type_not_convertible_exception);
}

TEST(object_routes_test, a_move_only_object_serves_references_and_no_copy) {
  container<> container;
  container.register_type<scope<shared>, storage<move_only_object>>();

  auto &reference = container.resolve<move_only_object &>();
  EXPECT_EQ(&reference, container.resolve<move_only_object *>());
  EXPECT_THROW(container.resolve<move_only_object>(),
               type_not_convertible_exception);
}

TEST(object_routes_test, a_const_object_serves_const_references_and_copies) {
  const copy_counted object;
  container<> container;
  container.register_type<scope<external>, storage<const copy_counted &>>(
      object);

  EXPECT_EQ(&container.resolve<const copy_counted &>(), &object);
  EXPECT_EQ(container.resolve<const copy_counted *>(), &object);
  EXPECT_EQ(container.resolve<copy_counted>().copies, 1);
  EXPECT_THROW(container.resolve<copy_counted &>(),
               type_not_convertible_exception);
  EXPECT_THROW(container.resolve<copy_counted *>(),
               type_not_convertible_exception);
}

TEST(object_routes_test, external_unique_handle_serves_a_copy_of_its_object) {
  auto handle = std::make_unique<copy_counted>();
  auto *object = handle.get();
  container<> container;
  container
      .register_type<scope<external>, storage<std::unique_ptr<copy_counted>>>(
          std::move(handle));

  EXPECT_EQ(&container.resolve<copy_counted &>(), object);
  EXPECT_EQ(container.resolve<copy_counted *>(), object);
  const auto copy = container.resolve<copy_counted>();
  EXPECT_EQ(copy.copies, 1);
  EXPECT_NE(&copy, object);
}

TEST(object_routes_test, external_unique_handle_serves_no_copy_of_move_only) {
  auto handle = std::make_unique<move_only_object>();
  auto *object = handle.get();
  container<> container;
  container.register_type<scope<external>,
                          storage<std::unique_ptr<move_only_object>>>(
      std::move(handle));

  EXPECT_EQ(&container.resolve<move_only_object &>(), object);
  EXPECT_THROW(container.resolve<move_only_object>(),
               type_not_convertible_exception);
}

TEST(object_routes_test, shared_pointer_serves_a_copy_of_its_pointee) {
  // Shared storage owns the object its factory returns.
  container<> container;
  container.register_type<scope<shared>, storage<copy_counted *>>(
      callable([]() { return new copy_counted; }));

  auto &object = container.resolve<copy_counted &>();
  EXPECT_EQ(container.resolve<copy_counted *>(), &object);
  const auto copy = container.resolve<copy_counted>();
  EXPECT_EQ(copy.copies, 1);
  EXPECT_NE(&copy, &object);
}

TEST(object_routes_test, shared_pointer_serves_no_copy_of_move_only_pointee) {
  container<> container;
  container.register_type<scope<shared>, storage<move_only_object *>>(
      callable([]() { return new move_only_object; }));

  auto &object = container.resolve<move_only_object &>();
  EXPECT_EQ(container.resolve<move_only_object *>(), &object);
  EXPECT_THROW(container.resolve<move_only_object>(),
               type_not_convertible_exception);
}

TEST(object_routes_test, a_custom_copy_conversion_keeps_its_value_route) {
  container<> container;
  container.register_type<scope<shared>, storage<custom_copy>>();

  auto &reference = container.resolve<custom_copy &>();
  EXPECT_EQ(&reference, container.resolve<custom_copy *>());
  // The custom conversion decides how the value is copied.
  EXPECT_EQ(container.resolve<custom_copy>().value, reference.value + 10);
}

TEST(object_routes_test, interface_objects_serve_their_references) {
  container<> container;
  container.register_type<scope<shared>, storage<leaf>,
                          interfaces<base_interface, other_interface>>();

  auto &base = container.resolve<base_interface &>();
  EXPECT_EQ(&base, container.resolve<base_interface *>());
  auto &other = container.resolve<other_interface &>();
  EXPECT_EQ(&other, container.resolve<other_interface *>());
  EXPECT_THROW(container.resolve<leaf &>(), type_not_found_exception);
}

TEST(object_routes_test, wrapper_objects_serve_their_references_and_copies) {
  container<> container;
  container
      .register_type<scope<shared>, storage<std::shared_ptr<copy_counted>>>();

  auto &leaf_reference = container.resolve<copy_counted &>();
  EXPECT_EQ(&leaf_reference, container.resolve<copy_counted *>());
  auto &handle = container.resolve<std::shared_ptr<copy_counted> &>();
  EXPECT_EQ(&handle, container.resolve<std::shared_ptr<copy_counted> *>());
  EXPECT_EQ(handle.get(), &leaf_reference);
  EXPECT_EQ(container.resolve<std::shared_ptr<copy_counted>>().get(),
            &leaf_reference);
  EXPECT_EQ(container.resolve<copy_counted>().copies, 1);
}

TEST(object_routes_test, a_pointer_storage_serves_its_pointee) {
  copy_counted object;
  container<> container;
  container.register_type<scope<external>, storage<copy_counted *>>(&object);

  EXPECT_EQ(&container.resolve<copy_counted &>(), &object);
  EXPECT_EQ(container.resolve<copy_counted *>(), &object);
  EXPECT_EQ(container.resolve<copy_counted>().copies, 1);
}

TEST(object_routes_test, external_shared_handle_serves_a_copy_of_its_leaf) {
  auto handle = std::make_shared<copy_counted>();
  container<> container;
  container
      .register_type<scope<external>, storage<std::shared_ptr<copy_counted>>>(
          handle);

  EXPECT_EQ(&container.resolve<copy_counted &>(), handle.get());
  EXPECT_EQ(container.resolve<copy_counted *>(), handle.get());
  EXPECT_EQ(container.resolve<std::shared_ptr<copy_counted>>(), handle);
  const auto copy = container.resolve<copy_counted>();
  EXPECT_EQ(copy.copies, 1);
  EXPECT_NE(&copy, handle.get());
}

TEST(object_routes_test, external_nested_shared_handle_serves_a_copy_of_leaf) {
  auto handle = std::make_shared<std::optional<copy_counted>>(std::in_place);
  container<> container;
  container.register_type<
      scope<external>, storage<std::shared_ptr<std::optional<copy_counted>>>>(
      handle);

  const auto copy = container.resolve<copy_counted>();
  EXPECT_EQ(copy.copies, 1);
  EXPECT_NE(&copy, &**handle);
}

TEST(object_routes_test,
     external_nested_shared_handle_serves_no_copy_of_move_only_leaf) {
  auto handle =
      std::make_shared<std::optional<move_only_object>>(std::in_place);
  container<> container;
  container
      .register_type<scope<external>,
                     storage<std::shared_ptr<std::optional<move_only_object>>>>(
          handle);

  EXPECT_THROW(container.resolve<move_only_object>(),
               type_not_convertible_exception);
}
} // namespace
