//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/lookup/lookup.h>
#include <dingo/static/registry.h>
#include <dingo/type/key.h>

#include <cstddef>
#include <type_traits>

// The compile-time checks of a static registry that registration validity does
// not cover: declared dependencies must be bound, and bindings with a fixed
// runtime key must be declared by a lookup definition and bound once.
namespace dingo {
namespace detail {

// A binding with a fixed runtime key, key_type<K, V>, is looked up through the
// associative definition of its key domain: the definition must exist, and a
// one definition takes one binding of the interface and key, a many
// definition one per storage. Any other binding needs no definition.
template <typename InterfaceBinding, typename Other>
struct same_fixed_key_storage
    : std::bool_constant<
          std::is_same_v<typename InterfaceBinding::interface_type,
                         typename Other::interface_type> &&
          std::is_same_v<
              typename InterfaceBinding::binding_model_type::storage_type::type,
              typename Other::binding_model_type::storage_type::type> &&
          lookup_keys_match<typename InterfaceBinding::key_type,
                            typename Other::key_type>::value> {};

template <typename InterfaceBinding, typename Entries,
          typename Key = typename InterfaceBinding::key_type>
struct fixed_key_binding {
  static constexpr bool declared = true;

  template <typename... Bindings>
  static constexpr bool is_unique(type_list<Bindings...>) {
    return true;
  }
};

template <typename InterfaceBinding, typename Entries, typename Key, auto Value>
struct fixed_key_binding<InterfaceBinding, Entries,
                         lookup_key<::dingo::key_type<Key, Value>>> {
private:
  using interface_type = typename InterfaceBinding::interface_type;
  using key_type = typename InterfaceBinding::key_type;
  using entry =
      selected_lookup_entry_t<interface_type,
                              typename key_type::definition_type, Entries>;

public:
  static constexpr bool declared = !std::is_void_v<entry>;

  template <typename... Bindings>
  static constexpr bool is_unique(type_list<Bindings...>) {
    if constexpr (!declared) {
      return false;
    } else if constexpr (std::is_same_v<typename entry::cardinality,
                                        ::dingo::one>) {
      return binding_count_v<interface_type, key_type,
                             type_list<Bindings...>> == 1;
    } else {
      return (std::size_t{0} + ... +
              (same_fixed_key_storage<InterfaceBinding, Bindings>::value
                   ? std::size_t{1}
                   : std::size_t{0})) == 1;
    }
  }
};

template <typename InterfaceBindings, typename Entries>
struct key_value_bindings_are_declared;

template <typename... InterfaceBindings, typename Entries>
struct key_value_bindings_are_declared<type_list<InterfaceBindings...>, Entries>
    : std::bool_constant<(
          fixed_key_binding<InterfaceBindings, Entries>::declared && ...)> {};

template <typename InterfaceBindings, typename Entries>
struct key_value_bindings_are_unique;

template <typename... InterfaceBindings, typename Entries>
struct key_value_bindings_are_unique<type_list<InterfaceBindings...>, Entries>
    : std::bool_constant<(
          fixed_key_binding<InterfaceBindings, Entries>::is_unique(
              type_list<InterfaceBindings...>{}) &&
          ...)> {};

template <typename Bindings> struct dependency_bindings_are_resolved;

template <typename... Bindings>
struct dependency_bindings_are_resolved<type_list<Bindings...>>
    : std::bool_constant<(!std::is_void_v<Bindings> && ...)> {};

// A dependency is registered when its bindings are known and not empty.
template <typename Dependency, typename InterfaceBindings,
          typename Bindings = typename dependency_binding_list<
              Dependency, InterfaceBindings>::type>
inline constexpr bool dependency_is_registered_v =
    type_list_size_v<Bindings> != 0 &&
    dependency_bindings_are_resolved<Bindings>::value;

template <typename DependencyList, typename InterfaceBindings>
struct first_missing_declared_dependency {
  using type = void;
};

template <typename Head, typename... Tail, typename InterfaceBindings>
struct first_missing_declared_dependency<type_list<Head, Tail...>,
                                         InterfaceBindings> {
  using type = typename std::conditional_t<
      dependency_is_registered_v<Head, InterfaceBindings>,
      first_missing_declared_dependency<type_list<Tail...>, InterfaceBindings>,
      std::enable_if<true, Head>>::type;
};

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

template <typename BindingModel, typename InterfaceBindings>
struct binding_declared_dependency_diagnostic
    : declared_dependency_diagnostic<
          BindingModel, typename first_missing_declared_dependency<
                            binding_dependencies_t<BindingModel>,
                            effective_interface_bindings_t<
                                BindingModel, InterfaceBindings>>::type> {};

template <typename InterfaceBindings, typename... BindingModels>
struct static_registry_dependency_diagnostics
    // Keep dependency booleans available on the source itself so static-only
    // and static/runtime paths can branch on them. Emit the detailed
    // diagnostics only when a static graph path instantiates this helper.
    : binding_declared_dependency_diagnostic<BindingModels,
                                             InterfaceBindings>... {};

} // namespace detail
} // namespace dingo
