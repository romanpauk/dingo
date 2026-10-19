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
#include <dingo/factory/traits.h>
#include <dingo/key/key.h>
#include <dingo/registration/type_registration.h>
#include <dingo/static/dependencies.h>
#include <dingo/static/index.h>
#include <dingo/static/validation.h>

#include <type_traits>

namespace dingo {

template <typename... Args> using bind = type_registration<Args...>;

namespace detail {

template <typename BindingModel>
struct binding_factory_is_compile_time_bindable
    : std::bool_constant<factory_traits<
          typename BindingModel::factory_type>::is_compile_time_bindable> {};

template <typename... Registrations>
using static_registry_bindings_t = type_list_cat_t<typename binding_expansion<
    binding_model<Registrations>>::interface_bindings...>;

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

  template <typename Interface, typename LookupKey>
  using exact_selection_t = detail::binding_selection_t<
      detail::binding_exact_dependency_interface_t<Interface>,
      binding_lookup_key_t<Interface, LookupKey>, interface_bindings>;

  template <typename Interface, typename LookupKey>
  using normalized_selection_t = detail::binding_selection_t<
      detail::binding_dependency_interface_t<Interface>,
      binding_lookup_key_t<Interface, LookupKey>, interface_bindings>;

  template <typename Interface, typename LookupKey, typename Exact,
            bool HasExact = (type_list_size_v<Exact> != 0)>
  struct select_bindings {
    using type = Exact;
  };

  template <typename Interface, typename LookupKey, typename Exact>
  struct select_bindings<Interface, LookupKey, Exact, false> {
    using type = normalized_bindings_t<Interface, LookupKey>;
  };

  template <typename Interface, typename LookupKey,
            bool SameLookup = std::is_same_v<
                detail::binding_exact_dependency_interface_t<Interface>,
                detail::binding_dependency_interface_t<Interface>>>
  struct selected_bindings
      : select_bindings<Interface, LookupKey,
                        exact_bindings_t<Interface, LookupKey>> {};

  template <typename Interface, typename LookupKey>
  struct selected_bindings<Interface, LookupKey, true> {
    using type = exact_bindings_t<Interface, LookupKey>;
  };

  template <typename Interface, typename LookupKey, typename ExactSelection,
            bool HasExact =
                ExactSelection::status != detail::binding_status::not_found>
  struct selected_binding {
    using type = ExactSelection;
  };

  template <typename Interface, typename LookupKey, typename ExactSelection>
  struct selected_binding<Interface, LookupKey, ExactSelection, false> {
    using type = normalized_selection_t<Interface, LookupKey>;
  };

  template <typename Interface, typename LookupKey, bool SameLookup>
  struct selected_binding_for_request;

  template <typename Interface, typename LookupKey>
  struct selected_binding_for_request<Interface, LookupKey, false>
      : selected_binding<Interface, LookupKey,
                         exact_selection_t<Interface, LookupKey>> {};

  template <typename Interface, typename LookupKey>
  struct selected_binding_for_request<Interface, LookupKey, true> {
    using type = exact_selection_t<Interface, LookupKey>;
  };

public:
  template <typename Interface, typename LookupKey>
  using bindings = typename selected_bindings<Interface, LookupKey>::type;

  template <typename Interface, typename LookupKey>
  using selection = typename selected_binding_for_request<
      Interface, LookupKey,
      std::is_same_v<detail::binding_exact_dependency_interface_t<Interface>,
                     detail::binding_dependency_interface_t<Interface>>>::type;

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

namespace detail {

template <typename... Bindings> struct static_bindings_cat;

template <> struct static_bindings_cat<> {
  using type = static_bindings<>;
};

template <typename... Registrations>
struct static_bindings_cat<static_bindings<Registrations...>> {
  using type = static_bindings<Registrations...>;
};

template <typename... FirstRegistrations, typename... SecondRegistrations,
          typename... Tail>
struct static_bindings_cat<static_bindings<FirstRegistrations...>,
                           static_bindings<SecondRegistrations...>, Tail...>
    : static_bindings_cat<
          static_bindings<FirstRegistrations..., SecondRegistrations...>,
          Tail...> {};

template <typename... Bindings>
using static_bindings_cat_t = typename static_bindings_cat<Bindings...>::type;

} // namespace detail
} // namespace dingo
