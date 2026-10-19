//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/binding/model.h>
#include <dingo/dependency/collection.h>
#include <dingo/dependency/normalized_type.h>
#include <dingo/factory/traits.h>
#include <dingo/key/key.h>
#include <dingo/key/selected.h>
#include <dingo/registration/type_registration.h>
#include <dingo/static/index.h>

#include <type_traits>

namespace dingo {
namespace detail {

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

template <typename InterfaceBindings, typename... BindingModels>
struct static_registry_dependency_diagnostics
    // Keep dependency booleans available on the source itself so static-only
    // and static/runtime paths can branch on them. Emit the detailed
    // diagnostics only when a static graph path instantiates this helper.
    : binding_declared_dependency_diagnostic<BindingModels,
                                             InterfaceBindings>... {};

} // namespace detail
} // namespace dingo
