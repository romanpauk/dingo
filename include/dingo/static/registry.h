//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/binding/model.h>
#include <dingo/binding/selection.h>
#include <dingo/factory/factory_traits.h>
#include <dingo/lookup/lookup.h>
#include <dingo/lookup/lookup_key.h>
#include <dingo/request/decomposition.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/shape.h>

#include <type_traits>
#include <utility>

namespace dingo {

template <typename... Args> using bind = type_registration<Args...>;

namespace detail {

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

// The declared dependencies of a binding, or those its factory detects.
template <typename BindingModel,
          typename Declared = binding_dependencies_t<BindingModel>>
struct binding_dependency_list {
  using type = Declared;
};

template <typename BindingModel>
struct binding_dependency_list<BindingModel, void> {
  using type = typename factory_traits<
      typename BindingModel::factory_type>::dependencies;
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

template <typename Interface, typename LookupKey, typename... InterfaceBindings>
struct bindings<Interface, LookupKey, type_list<InterfaceBindings...>> {
  using type = type_list_cat_t<std::conditional_t<
      binding_matches<Interface, LookupKey, InterfaceBindings>::value,
      type_list<InterfaceBindings>, type_list<>>...>;
};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
using bindings_t =
    typename bindings<Interface, LookupKey, InterfaceBindings>::type;

template <typename Interface, typename LookupKey, typename InterfaceBindings>
inline constexpr size_t binding_count_v =
    type_list_size_v<bindings_t<Interface, LookupKey, InterfaceBindings>>;

template <typename Interface, typename LookupKey> struct binding_lookup_tag {};

// The selection of the binding of an interface and a key among a list of
// bindings, as data: its status, and the index of the binding it found.
struct static_binding_selection {
  binding_status status;
  std::size_t index;
};

constexpr static_binding_selection select_matching(const bool *matches,
                                                   std::size_t count) {
  std::size_t found = 0;
  std::size_t index = 0;
  for (std::size_t i = 0; i < count; ++i) {
    if (matches[i] && found++ == 0) {
      index = i;
    }
  }
  return {status_of_count(found), index};
}

template <size_t Index, typename InterfaceBinding>
struct binding_lookup_index_entry {
  static std::integral_constant<size_t, Index>
      select(binding_lookup_tag<typename InterfaceBinding::interface_type,
                                typename InterfaceBinding::key_type>);
};

// The overload set of the bindings answers a lookup by overload resolution: a
// unique binding is the exact match, no binding falls through to the ellipsis,
// and several bindings of the same interface and key are ambiguous.
template <typename InterfaceBindings, typename Indexes>
struct binding_lookup_index_impl;

template <typename... InterfaceBindings, size_t... Indexes>
struct binding_lookup_index_impl<type_list<InterfaceBindings...>,
                                 std::index_sequence<Indexes...>>
    : binding_lookup_index_entry<Indexes, InterfaceBindings>... {
  using binding_lookup_index_entry<Indexes, InterfaceBindings>::select...;
  static void select(...);
};

template <typename InterfaceBindings>
using binding_lookup_index = binding_lookup_index_impl<
    InterfaceBindings,
    std::make_index_sequence<type_list_size_v<InterfaceBindings>>>;

template <typename Found> struct indexed_selection {
  static constexpr static_binding_selection value{binding_status::found,
                                                  Found::value};
};

template <> struct indexed_selection<void> {
  static constexpr static_binding_selection value{binding_status::not_found, 0};
};

// The bindings of a lookup without a key are indexed by interface, so the
// selection is read from the overload set instead of matching every binding.
// An ambiguous lookup has no viable overload.
template <typename Interface, typename LookupKey, typename InterfaceBindings,
          typename = void>
struct indexed_binding_selection {
  static constexpr static_binding_selection value{binding_status::ambiguous, 0};
};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
struct indexed_binding_selection<
    Interface, LookupKey, InterfaceBindings,
    std::void_t<decltype(binding_lookup_index<InterfaceBindings>::select(
        binding_lookup_tag<Interface, std::decay_t<LookupKey>>{}))>>
    : indexed_selection<
          decltype(binding_lookup_index<InterfaceBindings>::select(
              binding_lookup_tag<Interface, std::decay_t<LookupKey>>{}))> {};

// The bindings of a lookup with a key are those that match it.
template <typename Interface, typename LookupKey, typename InterfaceBindings>
struct listed_binding_selection;

template <typename Interface, typename LookupKey, typename... InterfaceBindings>
struct listed_binding_selection<Interface, LookupKey,
                                type_list<InterfaceBindings...>> {
  static constexpr bool matches[] = {
      binding_matches<Interface, LookupKey, InterfaceBindings>::value...,
      false};
  static constexpr static_binding_selection value =
      select_matching(matches, sizeof...(InterfaceBindings));
};

template <typename Interface, typename LookupKey, typename InterfaceBindings,
          bool UseIndex = is_no_lookup_key_v<LookupKey>>
struct binding_selection_of
    : listed_binding_selection<Interface, LookupKey, InterfaceBindings> {};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
struct binding_selection_of<Interface, LookupKey, InterfaceBindings, true>
    : indexed_binding_selection<Interface, LookupKey, InterfaceBindings> {};

// The choice of a selection, which the static container resolves by: the
// binding at the index of a selection that found one.
template <typename InterfaceBindings, binding_status Status, std::size_t Index>
struct selected_choice {
  using type = binding_choice<Status>;
};

template <typename InterfaceBindings, std::size_t Index>
struct selected_choice<InterfaceBindings, binding_status::found, Index> {
  using type = found_binding_choice_t<
      typename type_list_at<Index, InterfaceBindings>::type>;
};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
struct binding_selection {
  static constexpr const static_binding_selection &selected =
      binding_selection_of<Interface, LookupKey, InterfaceBindings>::value;
  using type = typename selected_choice<InterfaceBindings, selected.status,
                                        selected.index>::type;
};

template <typename Interface, typename LookupKey, typename InterfaceBindings>
using binding_selection_t =
    typename binding_selection<Interface, LookupKey, InterfaceBindings>::type;

template <typename Interface, typename LookupKey, typename InterfaceBindings>
using binding_t = typename binding_selection_t<Interface, LookupKey,
                                               InterfaceBindings>::binding_type;

template <typename InterfaceBindings>
struct dependency_bindings<void, InterfaceBindings> {
  using type = void;
};

// The bindings that answer a dependency: one, or void when none or several
// do, and every binding of the element of a collection.
template <typename Dependency, typename InterfaceBindings,
          typename Collection =
              collection_traits<binding_dependency_interface_t<Dependency>>,
          bool IsCollection = Collection::is_collection>
struct dependency_binding_list {
  using type = type_list<
      binding_t<binding_dependency_interface_t<Dependency>,
                binding_dependency_key_t<Dependency>, InterfaceBindings>>;
};

template <typename Dependency, typename InterfaceBindings, typename Collection>
struct dependency_binding_list<Dependency, InterfaceBindings, Collection,
                               true> {
  using type =
      bindings_t<normalized_type_t<typename Collection::resolve_type>,
                 binding_dependency_key_t<Dependency>, InterfaceBindings>;
};

template <typename... Dependencies, typename InterfaceBindings>
struct dependency_bindings<type_list<Dependencies...>, InterfaceBindings> {
  using type = type_list_cat_t<typename dependency_binding_list<
      Dependencies, InterfaceBindings>::type...>;
};

template <typename BindingModel, typename InterfaceBindings,
          typename LocalBindings = typename BindingModel::bindings_type>
struct effective_interface_bindings {
  using type = InterfaceBindings;
};

template <typename InterfaceBindings, typename LocalInterfaceBindings>
struct remove_shadowed_bindings;

template <typename... InterfaceBindings, typename LocalInterfaceBindings>
struct remove_shadowed_bindings<type_list<InterfaceBindings...>,
                                LocalInterfaceBindings> {
  using type = type_list_cat_t<std::conditional_t<
      binding_count_v<typename InterfaceBindings::interface_type,
                      typename InterfaceBindings::key_type,
                      LocalInterfaceBindings> != 0,
      type_list<>, type_list<InterfaceBindings>>...>;
};

template <typename BindingModel, typename InterfaceBindings,
          typename... LocalRegistrations>
struct effective_interface_bindings<BindingModel, InterfaceBindings,
                                    static_registry<LocalRegistrations...>> {
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
using resolved_dependency_bindings_t = typename dependency_bindings<
    typename binding_dependency_list<BindingModel>::type,
    effective_interface_bindings_t<BindingModel, InterfaceBindings>>::type;

template <typename BindingModel>
struct binding_factory_is_compile_time_bindable
    : std::bool_constant<factory_traits<
          typename BindingModel::factory_type>::is_compile_time_bindable> {};

template <typename... Registrations>
using static_registry_bindings_t = type_list_cat_t<typename binding_expansion<
    binding_model<Registrations>>::interface_bindings...>;

} // namespace detail

template <typename... Registrations> struct static_registry {
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

  // A request is answered by the bindings of its exact interface, or else by
  // those of its normalized interface.
  template <typename Interface, typename LookupKey,
            typename Exact = exact_bindings_t<Interface, LookupKey>,
            bool HasExact = !detail::selects_normalized_interface(
                detail::status_of_count(type_list_size_v<Exact>))>
  struct selected_bindings {
    using type = Exact;
  };

  template <typename Interface, typename LookupKey, typename Exact>
  struct selected_bindings<Interface, LookupKey, Exact, false> {
    using type = normalized_bindings_t<Interface, LookupKey>;
  };

  // Selects among the bindings of an interface.
  template <typename LookupKey> struct find_bindings {
    template <typename Interface>
    constexpr detail::static_binding_selection
    operator()(type_list_iterator<Interface>) const {
      return detail::binding_selection_of<Interface, LookupKey,
                                          interface_bindings>::value;
    }
  };

  // The selection of a request by the rule the runtime container selects
  // by: its exact interface, or else its normalized interface.
  template <typename Interface, typename LookupKey> struct selected_binding {
    using exact = detail::binding_exact_dependency_interface_t<Interface>;
    using normalized = detail::binding_dependency_interface_t<Interface>;
    static constexpr detail::static_binding_selection selected =
        detail::select_exact_or_normalized(
            find_bindings<binding_lookup_key_t<Interface, LookupKey>>{},
            type_list_iterator<exact>{}, type_list_iterator<normalized>{},
            !std::is_same_v<exact, normalized>);
    using type =
        typename detail::selected_choice<interface_bindings, selected.status,
                                         selected.index>::type;
  };

public:
  template <typename Interface, typename LookupKey>
  using bindings = typename selected_bindings<Interface, LookupKey>::type;

  template <typename Interface, typename LookupKey>
  using selection = typename selected_binding<Interface, LookupKey>::type;

  template <typename Interface, typename LookupKey>
  using binding = typename selection<Interface, LookupKey>::binding_type;
};

} // namespace dingo
