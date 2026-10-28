//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/binding/binding_model.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/factory/factory_traits.h>
#include <dingo/lookup/lookup.h>
#include <dingo/static/selection.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/dependency.h>
#include <dingo/type/normalized_type.h>

#include <type_traits>
#include <utility>

namespace dingo {

template <typename... Args> using bind = type_registration<Args...>;

namespace detail {

template <typename StaticBindings, typename State> class static_registry;

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

template <typename DependencyList, typename InterfaceBindings>
struct dependency_bindings;

template <typename BindingModel, typename InterfaceBindings,
          bool HasKnownDependencies =
              !std::is_same_v<binding_dependencies_t<BindingModel>, void>>
struct binding_dependency_resolution;

template <typename BindingModel, typename InterfaceBindings>
struct binding_dependency_resolution<BindingModel, InterfaceBindings, true> {
  using type =
      typename dependency_bindings<binding_dependencies_t<BindingModel>,
                                   InterfaceBindings>::type;
};

template <typename BindingModel, typename InterfaceBindings>
struct binding_dependency_resolution<BindingModel, InterfaceBindings, false> {
  using type = typename dependency_bindings<
      typename factory_traits<
          typename BindingModel::factory_type>::dependencies,
      InterfaceBindings>::type;
};

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

template <typename... InterfaceBindings, typename Entries>
struct key_value_bindings_are_declared<type_list<InterfaceBindings...>, Entries>
    : std::bool_constant<(
          key_value_binding_is_declared<InterfaceBindings, Entries>::value &&
          ...)> {};

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

template <typename InterfaceBinding, typename... InterfaceBindings>
struct key_value_duplicate_storage_count<InterfaceBinding,
                                         type_list<InterfaceBindings...>>
    : std::integral_constant<
          size_t, (size_t{0} + ... +
                   (key_value_duplicate_storage<InterfaceBinding,
                                                InterfaceBindings>::value
                        ? size_t{1}
                        : size_t{0}))> {};

template <typename InterfaceBindings, typename Entries>
struct key_value_bindings_are_unique;

template <typename InterfaceBinding, typename InterfaceBindings,
          typename Entries,
          typename Cardinality =
              key_value_lookup_cardinality_t<InterfaceBinding, Entries>>
struct key_value_binding_is_unique : std::false_type {};

template <typename InterfaceBinding, typename InterfaceBindings,
          typename Entries>
struct key_value_binding_is_unique<InterfaceBinding, InterfaceBindings, Entries,
                                   key_value_lookup_not_required>
    : std::true_type {};

template <typename InterfaceBinding, typename... InterfaceBindings,
          typename Entries>
struct key_value_binding_is_unique<
    InterfaceBinding, type_list<InterfaceBindings...>, Entries, one>
    : std::bool_constant<
          binding_count_v<typename InterfaceBinding::interface_type,
                          typename InterfaceBinding::key_type,
                          type_list<InterfaceBindings...>> == 1> {};

template <typename InterfaceBinding, typename... InterfaceBindings,
          typename Entries>
struct key_value_binding_is_unique<
    InterfaceBinding, type_list<InterfaceBindings...>, Entries, many>
    : std::bool_constant<
          key_value_duplicate_storage_count<
              InterfaceBinding, type_list<InterfaceBindings...>>::value == 1> {
};

template <typename... InterfaceBindings, typename Entries>
struct key_value_bindings_are_unique<type_list<InterfaceBindings...>, Entries>
    : std::bool_constant<(
          key_value_binding_is_unique<InterfaceBindings,
                                      type_list<InterfaceBindings...>,
                                      Entries>::value &&
          ...)> {};

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

template <typename BindingModel, typename InterfaceBindings>
using resolved_dependency_bindings_t =
    typename binding_dependency_resolution<BindingModel,
                                           InterfaceBindings>::type;

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
          binding_dependencies_t<BindingModel>, InterfaceBindings>>> {};

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
          BindingModel,
          first_missing_declared_dependency_t<
              binding_dependencies_t<BindingModel>, InterfaceBindings>> {};

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
                                             InterfaceBindings>... {};

} // namespace detail

template <typename... Registrations> struct static_bindings {
  using registration_types = type_list<Registrations...>;
  using interface_bindings =
      detail::static_registry_bindings_t<Registrations...>;

  static constexpr bool registrations_valid =
      (detail::binding_model<Registrations>::valid && ...);
  static constexpr bool factories_are_compile_time_bindable =
      (detail::binding_factory_is_compile_time_bindable<
           detail::binding_model<Registrations>>::value &&
       ...);
  static constexpr bool valid =
      registrations_valid && factories_are_compile_time_bindable;

  static_assert(
      registrations_valid,
      "bindings<...> source requires valid compile-time registrations");
  static_assert(
      factories_are_compile_time_bindable,
      "bindings<...> source requires compile-time-bindable factories");

private:
  // The exact and normalized interfaces of one request, its lookup key, and
  // the two lookups the public aliases select from. `bindings` and `selection`
  // differ only in whether they keep the whole list or one choice.
  template <typename Interface, typename LookupKey> struct lookup_keys {
    using exact_interface =
        detail::binding_exact_dependency_interface_t<Interface>;
    using normalized_interface =
        detail::binding_dependency_interface_t<Interface>;
    using key = std::conditional_t<detail::is_no_lookup_key_v<LookupKey>,
                                   detail::binding_dependency_key_t<Interface>,
                                   LookupKey>;

    static constexpr bool same =
        std::is_same_v<exact_interface, normalized_interface>;

    template <typename I>
    using list = detail::bindings_t<I, key, interface_bindings>;
    template <typename I>
    using choice = detail::static_selection_t<I, key, interface_bindings>;
  };

  template <typename Lookup, bool Same> struct selected_bindings;
  template <typename Lookup> struct selected_bindings<Lookup, true> {
    using type =
        typename Lookup::template list<typename Lookup::exact_interface>;
  };
  template <typename Lookup> struct selected_bindings<Lookup, false> {
    using exact =
        typename Lookup::template list<typename Lookup::exact_interface>;
    using type = std::conditional_t<
        type_list_size_v<exact> != 0, exact,
        typename Lookup::template list<typename Lookup::normalized_interface>>;
  };

  template <typename Lookup, bool Same> struct selected_binding;
  template <typename Lookup> struct selected_binding<Lookup, true> {
    using type =
        typename Lookup::template choice<typename Lookup::exact_interface>;
  };
  template <typename Lookup> struct selected_binding<Lookup, false> {
    using exact =
        typename Lookup::template choice<typename Lookup::exact_interface>;
    using type =
        std::conditional_t<exact::status != detail::binding_status::not_found,
                           exact,
                           typename Lookup::template choice<
                               typename Lookup::normalized_interface>>;
  };

public:
  template <typename Interface, typename LookupKey>
  using bindings =
      typename selected_bindings<lookup_keys<Interface, LookupKey>,
                                 lookup_keys<Interface, LookupKey>::same>::type;

  template <typename Interface, typename LookupKey>
  using selection =
      typename selected_binding<lookup_keys<Interface, LookupKey>,
                                lookup_keys<Interface, LookupKey>::same>::type;

  template <typename Interface, typename LookupKey>
  using binding = typename selection<Interface, LookupKey>::binding_type;

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
