//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/binding_model.h>
#include <dingo/core/dependency.h>
#include <dingo/core/factory_traits.h>
#include <dingo/lookup/lookup.h>
#include <dingo/registration/annotated.h>
#include <dingo/registration/collection_traits.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_list.h>

#include <type_traits>

namespace dingo {

template <typename... Args> using bind = type_registration<Args...>;

namespace detail {

template <typename StaticBindings, typename State> class static_registry;

enum class dependency_resolution_status {
  resolved,
  missing,
  ambiguous,
};

template <typename BindingModel>
using binding_dependencies_t = typename BindingModel::dependencies_type::type;

template <typename...> inline constexpr bool dependent_false_v = false;

template <typename Request>
using binding_dependency_unwrapped_t =
    std::conditional_t<is_selected_v<Request>, selected_type_t<Request>,
                       Request>;

template <typename Request>
using binding_dependency_interface_t =
    normalized_type_t<binding_dependency_unwrapped_t<Request>>;

template <typename Request>
using binding_exact_dependency_interface_t = std::remove_cv_t<
    std::remove_reference_t<binding_dependency_unwrapped_t<Request>>>;

template <typename Request>
using binding_dependency_key_t =
    make_lookup_key_t<selected_selector_t<Request>>;

template <typename Interface, typename RequestTypes>
struct annotated_dependency_types {
  using type = RequestTypes;
};

template <typename T, typename Tag, typename... RequestTypes>
struct annotated_dependency_types<annotated<T, Tag>,
                                  type_list<RequestTypes...>> {
  using type = type_list<annotated<RequestTypes, Tag>...>;
};

template <typename LookupKey, typename RequestTypes>
struct keyed_dependency_types {
  using type = RequestTypes;
};

template <typename LookupKey, typename... RequestTypes>
struct keyed_dependency_types<LookupKey, type_list<RequestTypes...>> {
private:
  using selector = typename std::decay_t<LookupKey>::selector_type;

public:
  using type = type_list<dependency<RequestTypes, selector>...>;
};

template <typename Binding> struct binding_dependency_types {
private:
  using interface_type = typename Binding::interface_type;
  using binding_model_type = typename Binding::binding_model_type;
  using raw_interface_type = typename annotated_traits<interface_type>::type;
  using storage_type = typename binding_model_type::storage_type;
  using capabilities =
      binding_capability_types<raw_interface_type, storage_type>;
  using base_types = type_list_cat_t<
      rebind_leaf_t<typename capabilities::value_types, raw_interface_type>,
      rebind_leaf_t<typename capabilities::lvalue_reference_types,
                    raw_interface_type>,
      rebind_leaf_t<typename capabilities::rvalue_reference_types,
                    raw_interface_type>,
      rebind_leaf_t<typename capabilities::pointer_types, raw_interface_type>>;

public:
  using type = type_list_unique_t<typename keyed_dependency_types<
      typename Binding::key_type, typename annotated_dependency_types<
                                      interface_type, base_types>::type>::type>;
};

template <typename DependencyList, typename InterfaceBindings>
struct dependency_bindings;

template <typename ResolvedBindings> struct dependency_bindings_are_resolved;

template <typename BindingModel, typename InterfaceBindings,
          bool HasKnownDependencies =
              !std::is_same_v<binding_dependencies_t<BindingModel>, void>>
struct binding_dependency_resolution;

template <typename BindingModel, typename InterfaceBindings>
struct binding_dependency_resolution<BindingModel, InterfaceBindings, true> {
  using type =
      typename dependency_bindings<binding_dependencies_t<BindingModel>,
                                   InterfaceBindings>::type;
  static constexpr dependency_resolution_status status =
      dependency_bindings_are_resolved<type>::value
          ? dependency_resolution_status::resolved
          : dependency_resolution_status::missing;
};

template <typename BindingModel, typename InterfaceBindings>
struct binding_dependency_resolution<BindingModel, InterfaceBindings, false> {
  using type = typename dependency_bindings<
      typename factory_traits<
          typename BindingModel::factory_type>::dependencies,
      InterfaceBindings>::type;
  static constexpr dependency_resolution_status status =
      std::is_same_v<type, void>
          ? dependency_resolution_status::resolved
          : (dependency_bindings_are_resolved<type>::value
                 ? dependency_resolution_status::resolved
                 : dependency_resolution_status::missing);
};

template <typename Interface, typename LookupKey, typename InterfaceBinding>
struct binding_matches
    : std::bool_constant<
          std::is_same_v<Interface,
                         typename InterfaceBinding::interface_type> &&
          lookup_keys_match<LookupKey,
                            typename InterfaceBinding::key_type>::value> {};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
struct bindings;

template <typename Interface, typename LookupKey>
struct bindings<Interface, LookupKey, type_list<>> {
  using type = type_list<>;
};

template <typename Interface, typename LookupKey, typename Head,
          typename... Tail>
struct bindings<Interface, LookupKey, type_list<Head, Tail...>> {
private:
  using tail_type =
      typename bindings<Interface, LookupKey, type_list<Tail...>>::type;

public:
  using type =
      std::conditional_t<binding_matches<Interface, LookupKey, Head>::value,
                         type_list_cat_t<type_list<Head>, tail_type>,
                         tail_type>;
};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
using bindings_t =
    typename bindings<Interface, LookupKey, InterfaceBindings>::type;

template <typename Interface, typename LookupKey, typename InterfaceBindings>
struct binding_count;

template <typename Interface, typename LookupKey>
struct binding_count<Interface, LookupKey, type_list<>>
    : std::integral_constant<size_t, 0> {};

template <typename Interface, typename LookupKey, typename Head,
          typename... Tail>
struct binding_count<Interface, LookupKey, type_list<Head, Tail...>>
    : std::integral_constant<
          size_t,
          (binding_matches<Interface, LookupKey, Head>::value ? 1 : 0) +
              binding_count<Interface, LookupKey, type_list<Tail...>>::value> {
};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
inline constexpr size_t binding_count_v =
    binding_count<Interface, LookupKey, InterfaceBindings>::value;

template <typename Bindings,
          bool HasSingleBinding = (type_list_size_v<Bindings> == 1)>
struct single_binding;

template <typename Bindings> struct single_binding<Bindings, false> {
  using type = void;
};

template <typename Head, typename... Tail>
struct single_binding<type_list<Head, Tail...>, true> {
  using type = Head;
};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
struct binding_lookup {
  using type = typename single_binding<
      bindings_t<Interface, LookupKey, InterfaceBindings>>::type;
};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
using binding_t =
    typename binding_lookup<Interface, LookupKey, InterfaceBindings>::type;

struct key_value_lookup_not_required {};

template <typename InterfaceBinding, typename Entries,
          typename LookupKey = typename InterfaceBinding::key_type>
struct key_value_lookup_cardinality {
  using type = key_value_lookup_not_required;
};

template <typename InterfaceBinding, typename Entries, typename Key, auto Value>
struct key_value_lookup_cardinality<InterfaceBinding, Entries,
                                    lookup_key<::dingo::key_type<Key, Value>>> {
private:
  using key_domain = typename InterfaceBinding::key_type::definition_type;
  using entry =
      selected_lookup_entry_t<typename InterfaceBinding::interface_type,
                              key_domain, Entries>;

  template <typename Entry, bool Found = !std::is_void_v<Entry>>
  struct entry_cardinality {
    using type = void;
  };

  template <typename Entry> struct entry_cardinality<Entry, true> {
    using type = typename Entry::cardinality;
  };

public:
  using type = typename entry_cardinality<entry>::type;
};

template <typename InterfaceBinding, typename Entries>
using key_value_lookup_cardinality_t =
    typename key_value_lookup_cardinality<InterfaceBinding, Entries>::type;

template <typename InterfaceBinding, typename Entries>
struct key_value_binding_is_declared
    : std::bool_constant<!std::is_void_v<
          key_value_lookup_cardinality_t<InterfaceBinding, Entries>>> {};

template <typename InterfaceBindings, typename Entries>
struct key_value_bindings_are_declared;

template <typename Entries>
struct key_value_bindings_are_declared<type_list<>, Entries> : std::true_type {
};

template <typename Head, typename... Tail, typename Entries>
struct key_value_bindings_are_declared<type_list<Head, Tail...>, Entries>
    : std::bool_constant<
          key_value_binding_is_declared<Head, Entries>::value &&
          key_value_bindings_are_declared<type_list<Tail...>, Entries>::value> {
};

template <typename InterfaceBinding, typename Other>
struct key_value_duplicate_storage
    : std::bool_constant<
          std::is_same_v<typename InterfaceBinding::interface_type,
                         typename Other::interface_type> &&
          std::is_same_v<
              typename InterfaceBinding::binding_model_type::storage_type::type,
              typename Other::binding_model_type::storage_type::type> &&
          lookup_keys_match<typename InterfaceBinding::key_type,
                            typename Other::key_type>::value> {};

template <typename InterfaceBinding, typename InterfaceBindings>
struct key_value_duplicate_storage_count;

template <typename InterfaceBinding>
struct key_value_duplicate_storage_count<InterfaceBinding, type_list<>>
    : std::integral_constant<size_t, 0> {};

template <typename InterfaceBinding, typename Head, typename... Tail>
struct key_value_duplicate_storage_count<InterfaceBinding,
                                         type_list<Head, Tail...>>
    : std::integral_constant<
          size_t,
          (key_value_duplicate_storage<InterfaceBinding, Head>::value ? 1 : 0) +
              key_value_duplicate_storage_count<InterfaceBinding,
                                                type_list<Tail...>>::value> {};

template <typename InterfaceBindings, typename Entries>
struct key_value_bindings_are_unique;

template <typename Entries>
struct key_value_bindings_are_unique<type_list<>, Entries> : std::true_type {};

template <typename Head, typename... Tail, typename Entries>
struct key_value_bindings_are_unique<type_list<Head, Tail...>, Entries> {
private:
  using cardinality = key_value_lookup_cardinality_t<Head, Entries>;
  static constexpr bool head_unique =
      std::is_same_v<cardinality, key_value_lookup_not_required> ||
      (std::is_same_v<cardinality, one> &&
       binding_count_v<typename Head::interface_type, typename Head::key_type,
                       type_list<Head, Tail...>> == 1) ||
      (std::is_same_v<cardinality, many> &&
       key_value_duplicate_storage_count<Head,
                                         type_list<Head, Tail...>>::value == 1);

public:
  static constexpr bool value =
      head_unique &&
      key_value_bindings_are_unique<type_list<Tail...>, Entries>::value;
};

template <typename DependencyList, typename InterfaceBindings>
struct dependencies_registered;

template <typename DependencyList, typename InterfaceBindings>
struct first_missing_declared_dependency;

template <typename InterfaceBindings>
struct first_missing_declared_dependency<void, InterfaceBindings> {
  using type = void;
};

template <typename Dependency, typename InterfaceBindings,
          bool IsCollection = collection_traits<
              binding_dependency_interface_t<Dependency>>::is_collection>
struct declared_dependency_is_registered;

template <typename Dependency, typename InterfaceBindings>
struct declared_dependency_is_registered<Dependency, InterfaceBindings, false> {
private:
  using dependency_type = binding_dependency_interface_t<Dependency>;
  using dependency_key = binding_dependency_key_t<Dependency>;

public:
  static constexpr bool value = !std::is_void_v<
      binding_t<dependency_type, dependency_key, InterfaceBindings>>;
};

template <typename Dependency, typename InterfaceBindings>
struct declared_dependency_is_registered<Dependency, InterfaceBindings, true> {
private:
  using dependency_type = binding_dependency_interface_t<Dependency>;
  using dependency_key = binding_dependency_key_t<Dependency>;
  using collection_type = collection_traits<dependency_type>;

public:
  static constexpr bool value =
      binding_count_v<normalized_type_t<typename collection_type::resolve_type>,
                      dependency_key, InterfaceBindings> != 0;
};

template <typename InterfaceBindings>
struct first_missing_declared_dependency<type_list<>, InterfaceBindings> {
  using type = void;
};

template <typename Head, typename... Tail, typename InterfaceBindings>
struct first_missing_declared_dependency<type_list<Head, Tail...>,
                                         InterfaceBindings> {
private:
  using dependency_type = binding_dependency_interface_t<Head>;
  using dependency_key = binding_dependency_key_t<Head>;

public:
  using type = std::conditional_t<
      !declared_dependency_is_registered<Head, InterfaceBindings>::value, Head,
      typename first_missing_declared_dependency<type_list<Tail...>,
                                                 InterfaceBindings>::type>;
};

template <typename DependencyList, typename InterfaceBindings>
using first_missing_declared_dependency_t =
    typename first_missing_declared_dependency<DependencyList,
                                               InterfaceBindings>::type;

template <typename InterfaceBindings>
struct dependencies_registered<void, InterfaceBindings> : std::false_type {};

template <typename InterfaceBindings>
struct dependencies_registered<type_list<>, InterfaceBindings>
    : std::true_type {};

template <typename Head, typename... Tail, typename InterfaceBindings>
struct dependencies_registered<type_list<Head, Tail...>, InterfaceBindings>
    : std::bool_constant<
          declared_dependency_is_registered<Head, InterfaceBindings>::value &&
          dependencies_registered<type_list<Tail...>,
                                  InterfaceBindings>::value> {};

template <typename InterfaceBindings>
struct dependency_bindings<void, InterfaceBindings> {
  using type = void;
};

template <typename InterfaceBindings>
struct dependency_bindings<type_list<>, InterfaceBindings> {
  using type = type_list<>;
};

template <typename Dependency, typename InterfaceBindings,
          bool IsCollection = collection_traits<
              binding_dependency_interface_t<Dependency>>::is_collection>
struct dependency_binding_list;

template <typename Dependency, typename InterfaceBindings>
struct dependency_binding_list<Dependency, InterfaceBindings, false> {
private:
  using dependency_type = binding_dependency_interface_t<Dependency>;
  using dependency_key = binding_dependency_key_t<Dependency>;

public:
  using type =
      type_list<binding_t<dependency_type, dependency_key, InterfaceBindings>>;
};

template <typename Dependency, typename InterfaceBindings>
struct dependency_binding_list<Dependency, InterfaceBindings, true> {
private:
  using dependency_type = binding_dependency_interface_t<Dependency>;
  using dependency_key = binding_dependency_key_t<Dependency>;
  using collection_type = collection_traits<dependency_type>;

public:
  using type =
      bindings_t<normalized_type_t<typename collection_type::resolve_type>,
                 dependency_key, InterfaceBindings>;
};

template <typename... Dependencies, typename InterfaceBindings>
struct dependency_bindings<type_list<Dependencies...>, InterfaceBindings> {
  using type = type_list_cat_t<typename dependency_binding_list<
      Dependencies, InterfaceBindings>::type...>;
};

template <typename ResolvedBindings>
struct dependency_bindings_are_resolved : std::false_type {};

template <> struct dependency_bindings_are_resolved<void> : std::false_type {};

template <>
struct dependency_bindings_are_resolved<type_list<>> : std::true_type {};

template <typename Head, typename... Tail>
struct dependency_bindings_are_resolved<type_list<Head, Tail...>>
    : std::bool_constant<
          !std::is_void_v<Head> &&
          dependency_bindings_are_resolved<type_list<Tail...>>::value> {};

template <typename BindingModel, typename InterfaceBindings,
          typename LocalBindings = typename BindingModel::bindings_type>
struct effective_interface_bindings {
  using type = InterfaceBindings;
};

template <typename InterfaceBinding, typename InterfaceBindings>
struct binding_shadowed_by {
  static constexpr bool value = false;
};

template <typename InterfaceBinding, typename Head, typename... Tail>
struct binding_shadowed_by<InterfaceBinding, type_list<Head, Tail...>>
    : std::bool_constant<
          (std::is_same_v<typename InterfaceBinding::interface_type,
                          typename Head::interface_type> &&
           lookup_keys_match<typename InterfaceBinding::key_type,
                             typename Head::key_type>::value) ||
          binding_shadowed_by<InterfaceBinding, type_list<Tail...>>::value> {};

template <typename InterfaceBindings, typename LocalInterfaceBindings>
struct remove_shadowed_bindings;

template <typename LocalInterfaceBindings>
struct remove_shadowed_bindings<type_list<>, LocalInterfaceBindings> {
  using type = type_list<>;
};

template <typename Head, typename... Tail, typename LocalInterfaceBindings>
struct remove_shadowed_bindings<type_list<Head, Tail...>,
                                LocalInterfaceBindings> {
private:
  using tail_type =
      typename remove_shadowed_bindings<type_list<Tail...>,
                                        LocalInterfaceBindings>::type;

public:
  using type = std::conditional_t<
      binding_shadowed_by<Head, LocalInterfaceBindings>::value, tail_type,
      type_list_cat_t<type_list<Head>, tail_type>>;
};

template <typename BindingModel, typename InterfaceBindings,
          typename... LocalRegistrations>
struct effective_interface_bindings<BindingModel, InterfaceBindings,
                                    static_bindings<LocalRegistrations...>> {
private:
  using local_interface_bindings = type_list_cat_t<typename binding_expansion<
      binding_model<LocalRegistrations>>::interface_bindings...>;
  using host_interface_bindings =
      typename remove_shadowed_bindings<InterfaceBindings,
                                        local_interface_bindings>::type;

public:
  using type =
      type_list_cat_t<local_interface_bindings, host_interface_bindings>;
};

template <typename BindingModel, typename InterfaceBindings>
using effective_interface_bindings_t =
    typename effective_interface_bindings<BindingModel,
                                          InterfaceBindings>::type;

template <typename BindingModel, typename InterfaceBindings>
using resolved_dependency_bindings_t = typename binding_dependency_resolution<
    BindingModel,
    effective_interface_bindings_t<BindingModel, InterfaceBindings>>::type;

template <typename BindingModel, typename InterfaceBindings>
inline constexpr dependency_resolution_status
    binding_dependency_resolution_status_v = binding_dependency_resolution<
        BindingModel, effective_interface_bindings_t<
                          BindingModel, InterfaceBindings>>::status;

template <typename BindingModel, typename MissingDependency,
          bool Valid = std::is_void_v<MissingDependency>>
struct declared_dependency_diagnostic;

template <typename BindingModel, typename MissingDependency>
struct declared_dependency_diagnostic<BindingModel, MissingDependency, true>
    : std::true_type {};

template <typename BindingModel, typename MissingDependency>
struct declared_dependency_diagnostic<BindingModel, MissingDependency, false> {
  static_assert(
      dependent_false_v<BindingModel, MissingDependency>,
      "bindings<...> source requires every declared dependency to map "
      "to an interface binding");
  static constexpr bool value = false;
};

template <typename BindingModel, typename InterfaceBindings,
          bool HasKnownDependencies =
              !std::is_same_v<binding_dependencies_t<BindingModel>, void>>
struct binding_declared_dependencies_resolved;

template <typename BindingModel, typename InterfaceBindings>
struct binding_declared_dependencies_resolved<BindingModel, InterfaceBindings,
                                              true>
    : std::bool_constant<std::is_void_v<first_missing_declared_dependency_t<
          binding_dependencies_t<BindingModel>,
          effective_interface_bindings_t<BindingModel, InterfaceBindings>>>> {};

template <typename BindingModel, typename InterfaceBindings>
struct binding_declared_dependencies_resolved<BindingModel, InterfaceBindings,
                                              false> : std::true_type {};

template <typename BindingModel, typename InterfaceBindings,
          bool DependenciesResolved = binding_declared_dependencies_resolved<
              BindingModel, InterfaceBindings>::value>
struct binding_declared_dependency_diagnostic;

template <typename BindingModel, typename InterfaceBindings>
struct binding_declared_dependency_diagnostic<BindingModel, InterfaceBindings,
                                              true> : std::true_type {};

template <typename BindingModel, typename InterfaceBindings>
struct binding_declared_dependency_diagnostic<BindingModel, InterfaceBindings,
                                              false>
    : declared_dependency_diagnostic<
          BindingModel, first_missing_declared_dependency_t<
                            binding_dependencies_t<BindingModel>,
                            effective_interface_bindings_t<
                                BindingModel, InterfaceBindings>>> {};

template <typename BindingModel, typename InterfaceBindings,
          bool HasKnownDependencies =
              !std::is_same_v<binding_dependencies_t<BindingModel>, void>>
struct binding_inferred_dependencies_resolved;

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_dependencies_resolved<BindingModel, InterfaceBindings,
                                              true> : std::true_type {};

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_dependencies_resolved<BindingModel, InterfaceBindings,
                                              false> : std::true_type {};

template <typename BindingModel, typename InterfaceBindings,
          bool DependenciesResolved = binding_inferred_dependencies_resolved<
              BindingModel, InterfaceBindings>::value>
struct binding_inferred_dependency_diagnostic;

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_dependency_diagnostic<BindingModel, InterfaceBindings,
                                              true> : std::true_type {};

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_dependency_diagnostic<BindingModel, InterfaceBindings,
                                              false> : std::true_type {};

template <typename BindingModel, typename InterfaceBindings,
          bool HasKnownDependencies =
              !std::is_same_v<binding_dependencies_t<BindingModel>, void>>
struct binding_inferred_dependencies_unambiguous;

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_dependencies_unambiguous<BindingModel,
                                                 InterfaceBindings, true>
    : std::true_type {};

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_dependencies_unambiguous<BindingModel,
                                                 InterfaceBindings, false>
    : std::true_type {};

template <typename BindingModel, typename InterfaceBindings,
          bool DependenciesUnambiguous =
              binding_inferred_dependencies_unambiguous<
                  BindingModel, InterfaceBindings>::value>
struct binding_inferred_ambiguity_diagnostic;

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_ambiguity_diagnostic<BindingModel, InterfaceBindings,
                                             true> : std::true_type {};

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_ambiguity_diagnostic<BindingModel, InterfaceBindings,
                                             false> : std::true_type {};

template <typename BindingModel>
struct binding_factory_is_compile_time_bindable
    : std::bool_constant<factory_traits<
          typename BindingModel::factory_type>::is_compile_time_bindable> {};

template <typename... Registrations>
using static_registry_bindings_t = type_list_cat_t<typename binding_expansion<
    binding_model<Registrations>>::interface_bindings...>;

template <typename InterfaceBindings, typename... BindingModels>
struct static_registry_dependency_diagnostics
    // Keep dependency booleans available on the source itself so static-only
    // and static/runtime paths can branch on them. Emit the detailed
    // diagnostics only when a static graph path instantiates this helper.
    : binding_declared_dependency_diagnostic<BindingModels,
                                             InterfaceBindings>...,
      binding_inferred_dependency_diagnostic<BindingModels,
                                             InterfaceBindings>...,
      binding_inferred_ambiguity_diagnostic<BindingModels,
                                            InterfaceBindings>... {};

} // namespace detail

template <typename... Registrations> struct static_bindings {
  using registration_types = type_list<Registrations...>;
  using binding_models = type_list<detail::binding_model<Registrations>...>;
  using interface_bindings =
      detail::static_registry_bindings_t<Registrations...>;

  static constexpr bool registrations_valid =
      (detail::binding_model<Registrations>::valid && ...);
  static constexpr bool factories_are_compile_time_bindable =
      (detail::binding_factory_is_compile_time_bindable<
           detail::binding_model<Registrations>>::value &&
       ...);
  static constexpr bool declared_dependencies_are_resolved =
      (detail::binding_declared_dependencies_resolved<
           detail::binding_model<Registrations>, interface_bindings>::value &&
       ...);
  static constexpr bool inferred_dependencies_are_resolved =
      (detail::binding_inferred_dependencies_resolved<
           detail::binding_model<Registrations>, interface_bindings>::value &&
       ...);
  static constexpr bool inferred_dependencies_are_unambiguous =
      (detail::binding_inferred_dependencies_unambiguous<
           detail::binding_model<Registrations>, interface_bindings>::value &&
       ...);
  static constexpr bool dependencies_are_resolved =
      declared_dependencies_are_resolved &&
      inferred_dependencies_are_resolved &&
      inferred_dependencies_are_unambiguous;
  static constexpr bool valid =
      registrations_valid && factories_are_compile_time_bindable;

  static_assert(
      registrations_valid,
      "bindings<...> source requires valid compile-time registrations");
  static_assert(
      factories_are_compile_time_bindable,
      "bindings<...> source requires compile-time-bindable factories");

private:
  template <typename Interface, typename LookupKey>
  using binding_lookup_key_t =
      std::conditional_t<detail::is_no_lookup_key_v<LookupKey>,
                         detail::binding_dependency_key_t<Interface>,
                         LookupKey>;

  template <typename Interface, typename LookupKey>
  using exact_bindings_t = detail::bindings_t<
      detail::binding_exact_dependency_interface_t<Interface>,
      binding_lookup_key_t<Interface, LookupKey>, interface_bindings>;

  template <typename Interface, typename LookupKey>
  using normalized_bindings_t =
      detail::bindings_t<detail::binding_dependency_interface_t<Interface>,
                         binding_lookup_key_t<Interface, LookupKey>,
                         interface_bindings>;

  template <typename Interface, typename LookupKey,
            bool SameLookup = std::is_same_v<
                detail::binding_exact_dependency_interface_t<Interface>,
                detail::binding_dependency_interface_t<Interface>>>
  struct selected_bindings {
    using exact = exact_bindings_t<Interface, LookupKey>;
    using type =
        std::conditional_t<type_list_size_v<exact> != 0, exact,
                           normalized_bindings_t<Interface, LookupKey>>;
  };

  template <typename Interface, typename LookupKey>
  struct selected_bindings<Interface, LookupKey, true> {
    using type = exact_bindings_t<Interface, LookupKey>;
  };

public:
  template <typename Interface, typename LookupKey>
  using bindings = typename selected_bindings<Interface, LookupKey>::type;

  template <typename Interface, typename LookupKey>
  using binding =
      typename detail::single_binding<bindings<Interface, LookupKey>>::type;

  template <typename Interface>
  using model =
      typename binding<Interface, detail::no_lookup_key_t>::binding_model_type;

  template <typename Interface>
  using dependencies = typename model<Interface>::dependencies_type::type;

  template <typename Interface>
  using dependency_bindings =
      detail::resolved_dependency_bindings_t<model<Interface>,
                                             interface_bindings>;
};

} // namespace dingo
