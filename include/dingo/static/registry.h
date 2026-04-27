//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/registration/annotated.h>
#include <dingo/core/keyed.h>
#include <dingo/core/factory_traits.h>
#include <dingo/core/binding_model.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_list.h>

#include <type_traits>

namespace dingo {

template <typename... Args>
using bind = type_registration<Args...>;

namespace detail {

enum class dependency_resolution_status {
    resolved,
    missing,
    ambiguous,
};

template <typename BindingModel>
using binding_dependencies_t = typename BindingModel::dependencies_type::type;

template <typename...>
inline constexpr bool dependent_false_v = false;

template <typename T, typename List> struct binding_type_list_contains;

template <typename T>
struct binding_type_list_contains<T, type_list<>> : std::false_type {};

template <typename T, typename Head, typename... Tail>
struct binding_type_list_contains<T, type_list<Head, Tail...>>
    : std::bool_constant<std::is_same_v<T, Head> ||
                         binding_type_list_contains<T, type_list<Tail...>>::value> {
};

template <typename T, typename List>
inline constexpr bool binding_type_list_contains_v =
    binding_type_list_contains<T, List>::value;

template <typename Accumulated, typename Remaining>
struct type_list_unique_impl;

template <typename... Accumulated>
struct type_list_unique_impl<type_list<Accumulated...>, type_list<>> {
    using type = type_list<Accumulated...>;
};

template <typename... Accumulated, typename Head, typename... Tail>
struct type_list_unique_impl<type_list<Accumulated...>, type_list<Head, Tail...>> {
  private:
    using next_accumulated = std::conditional_t<
        binding_type_list_contains_v<Head, type_list<Accumulated...>>,
        type_list<Accumulated...>, type_list<Accumulated..., Head>>;

  public:
    using type =
        typename type_list_unique_impl<next_accumulated, type_list<Tail...>>::type;
};

template <typename List>
using type_list_unique_t = typename type_list_unique_impl<type_list<>, List>::type;

template <typename Head, typename List> struct type_list_prepend;

template <typename Head, typename... Tail>
struct type_list_prepend<Head, type_list<Tail...>> {
    using type = type_list<Head, Tail...>;
};

template <typename Head, typename Tuples> struct prepend_binding_to_tuples;

template <typename Head, typename... Tuples>
struct prepend_binding_to_tuples<Head, type_list<Tuples...>> {
    using type = type_list<typename type_list_prepend<Head, Tuples>::type...>;
};

template <typename Interface, typename RequestTypes>
struct annotated_request_types {
    using type = RequestTypes;
};

template <typename T, typename Tag, typename... RequestTypes>
struct annotated_request_types<annotated<T, Tag>, type_list<RequestTypes...>> {
    using type = type_list<annotated<RequestTypes, Tag>...>;
};

template <typename Key, typename RequestTypes>
struct keyed_request_types {
    using type = RequestTypes;
};

template <typename Key, typename... RequestTypes>
struct keyed_request_types<Key, type_list<RequestTypes...>> {
    using type = std::conditional_t<
        std::is_void_v<Key>, type_list<RequestTypes...>,
        type_list<keyed<RequestTypes, Key>...>>;
};

template <typename Request>
using binding_request_interface_t = normalized_type_t<keyed_type_t<Request>>;

template <typename Request>
using binding_request_key_t = keyed_key_t<Request>;

template <typename InterfaceBinding>
struct interface_binding_request_types {
  private:
    using interface_type = typename InterfaceBinding::interface_type;
    using binding_model_type = typename InterfaceBinding::binding_model_type;
    using raw_interface_type = typename annotated_traits<interface_type>::type;
    using storage_conversions =
        typename binding_model_type::storage_type::conversions;
    using base_types = type_list_cat_t<
        rebind_leaf_t<typename storage_conversions::value_types, raw_interface_type>,
        rebind_leaf_t<typename storage_conversions::lvalue_reference_types,
                      raw_interface_type>,
        rebind_leaf_t<typename storage_conversions::rvalue_reference_types,
                      raw_interface_type>,
        rebind_leaf_t<typename storage_conversions::pointer_types, raw_interface_type>>;

  public:
    using type = type_list_unique_t<typename keyed_request_types<
        typename InterfaceBinding::key_type,
        typename annotated_request_types<interface_type, base_types>::type>::type>;
};

template <typename Request> struct binding_request_converter {
    operator Request() const;
};

template <typename RequestTypes> struct binding_argument_placeholder;

template <typename... RequestTypes>
struct binding_argument_placeholder<type_list<RequestTypes...>>
    : binding_request_converter<RequestTypes>... {};

template <typename RequestTypes>
using binding_argument_placeholder_t =
    binding_argument_placeholder<RequestTypes>;

template <typename Implementation, typename BindingTuple>
struct binding_tuple_constructible;

template <typename Implementation, typename... InterfaceBindings>
struct binding_tuple_constructible<Implementation,
                                   type_list<InterfaceBindings...>>
    : std::bool_constant<
          is_list_initializable_v<
              Implementation,
              binding_argument_placeholder_t<
                  typename interface_binding_request_types<
                      InterfaceBindings>::type>...> ||
          is_direct_initializable_v<
              Implementation,
              binding_argument_placeholder_t<
                  typename interface_binding_request_types<
                      InterfaceBindings>::type>...>> {};

template <typename InterfaceBindings, size_t Arity, typename = void>
struct interface_binding_tuples;

template <typename InterfaceBindings>
struct interface_binding_tuples<InterfaceBindings, 0, void> {
    using type = type_list<type_list<>>;
};

template <typename... InterfaceBindings, size_t Arity>
struct interface_binding_tuples<type_list<InterfaceBindings...>, Arity,
                                std::enable_if_t<(Arity > 0)>> {
  private:
    using tail_tuples =
        typename interface_binding_tuples<type_list<InterfaceBindings...>,
                                          Arity - 1>::type;

  public:
    using type = type_list_cat_t<
        typename prepend_binding_to_tuples<InterfaceBindings, tail_tuples>::type...>;
};

template <typename Implementation, typename CandidateTuples>
struct unique_constructible_binding_tuple;

template <typename Implementation>
struct unique_constructible_binding_tuple<Implementation, type_list<>> {
    static constexpr size_t count = 0;
    using type = void;
};

template <typename Implementation, typename Head, typename... Tail>
struct unique_constructible_binding_tuple<Implementation,
                                          type_list<Head, Tail...>> {
  private:
    using tail_result =
        unique_constructible_binding_tuple<Implementation, type_list<Tail...>>;
    static constexpr bool head_matches =
        binding_tuple_constructible<Implementation, Head>::value;

  public:
    static constexpr size_t count = tail_result::count + (head_matches ? 1 : 0);
    using type = std::conditional_t<
        count == 1, std::conditional_t<head_matches, Head, typename tail_result::type>,
        void>;
};

template <typename Factory> struct constructor_factory_target;

template <typename T>
struct constructor_factory_target<constructor<T>> {
    using type = normalized_type_t<T>;
};

template <typename BindingModel, typename InterfaceBindings, typename = void>
struct inferred_binding_dependencies {
    using type = void;
    static constexpr dependency_resolution_status status =
        dependency_resolution_status::missing;
};

template <typename BindingModel, typename InterfaceBindings>
struct inferred_binding_dependencies<
    BindingModel, InterfaceBindings,
    std::enable_if_t<is_plain_constructor_factory<
        typename BindingModel::factory_type>::value>> {
  private:
    using factory_type = typename BindingModel::factory_type;
    using implementation_type =
        typename constructor_factory_target<factory_type>::type;
    using candidate_tuples = typename interface_binding_tuples<
        InterfaceBindings, factory_type::arity>::type;
    using selection =
        unique_constructible_binding_tuple<implementation_type, candidate_tuples>;

  public:
    using type = std::conditional_t<factory_type::arity == 0, type_list<>,
                                    typename selection::type>;
    static constexpr dependency_resolution_status status =
        factory_type::arity == 0
            ? dependency_resolution_status::resolved
            : (selection::count == 0 ? dependency_resolution_status::missing
                                     : selection::count == 1
                                           ? dependency_resolution_status::resolved
                                           : dependency_resolution_status::ambiguous);
};

template <typename DependencyList, typename InterfaceBindings>
struct dependency_bindings;

template <typename ResolvedBindings>
struct dependency_bindings_are_resolved;

template <typename BindingModel, typename InterfaceBindings,
          bool HasKnownDependencies =
              !std::is_same_v<binding_dependencies_t<BindingModel>, void>>
struct binding_dependency_resolution;

template <typename BindingModel, typename InterfaceBindings>
struct binding_dependency_resolution<BindingModel, InterfaceBindings, true> {
    using type = typename dependency_bindings<binding_dependencies_t<BindingModel>,
                                             InterfaceBindings>::type;
    static constexpr dependency_resolution_status status =
        dependency_bindings_are_resolved<type>::value
            ? dependency_resolution_status::resolved
            : dependency_resolution_status::missing;
};

template <typename BindingModel, typename InterfaceBindings>
struct binding_dependency_resolution<BindingModel, InterfaceBindings, false> {
    using type =
        typename inferred_binding_dependencies<BindingModel, InterfaceBindings>::type;
    static constexpr dependency_resolution_status status =
        inferred_binding_dependencies<BindingModel, InterfaceBindings>::status;
};

template <typename Interface, typename Key, typename InterfaceBinding>
struct binding_matches
    : std::bool_constant<
          std::is_same_v<Interface, typename InterfaceBinding::interface_type> &&
          (std::is_void_v<Key> ||
           std::is_same_v<Key, typename InterfaceBinding::key_type>)> {};

template <typename Interface, typename Key, typename InterfaceBindings>
struct bindings;

template <typename Interface, typename Key>
struct bindings<Interface, Key, type_list<>> {
    using type = type_list<>;
};

template <typename Interface, typename Key, typename Head, typename... Tail>
struct bindings<Interface, Key, type_list<Head, Tail...>> {
  private:
    using tail_type =
        typename bindings<Interface, Key, type_list<Tail...>>::type;

  public:
    using type = std::conditional_t<
        binding_matches<Interface, Key, Head>::value,
        type_list_cat_t<type_list<Head>, tail_type>, tail_type>;
};

template <typename Interface, typename Key, typename InterfaceBindings>
using bindings_t =
    typename bindings<Interface, Key, InterfaceBindings>::type;

template <typename Interface, typename Key, typename InterfaceBindings>
struct binding_count;

template <typename Interface, typename Key>
struct binding_count<Interface, Key, type_list<>>
    : std::integral_constant<size_t, 0> {};

template <typename Interface, typename Key, typename Head, typename... Tail>
struct binding_count<Interface, Key, type_list<Head, Tail...>>
    : std::integral_constant<
          size_t,
          (binding_matches<Interface, Key, Head>::value ? 1 : 0) +
              binding_count<Interface, Key,
                                      type_list<Tail...>>::value> {};

template <typename Interface, typename Key, typename InterfaceBindings>
inline constexpr size_t binding_count_v =
    binding_count<Interface, Key, InterfaceBindings>::value;

template <typename Bindings, bool HasSingleBinding = (type_list_size_v<Bindings> == 1)>
struct single_binding;

template <typename Bindings>
struct single_binding<Bindings, false> {
    using type = void;
};

template <typename Head, typename... Tail>
struct single_binding<type_list<Head, Tail...>, true> {
    using type = Head;
};

template <typename Interface, typename Key, typename InterfaceBindings>
struct binding {
    using type = typename single_binding<
        bindings_t<Interface, Key, InterfaceBindings>>::type;
};

template <typename Interface, typename Key, typename InterfaceBindings>
using binding_t =
    typename binding<Interface, Key, InterfaceBindings>::type;

template <typename InterfaceBindings>
struct keyed_interface_bindings_are_unique;

template <>
struct keyed_interface_bindings_are_unique<type_list<>> : std::true_type {};

template <typename Head, typename... Tail>
struct keyed_interface_bindings_are_unique<type_list<Head, Tail...>>
    : std::bool_constant<
          (std::is_void_v<typename Head::key_type> ||
           binding_count_v<typename Head::interface_type,
                                     typename Head::key_type,
                                     type_list<Head, Tail...>> == 1) &&
          keyed_interface_bindings_are_unique<type_list<Tail...>>::value> {};

template <typename DependencyList, typename InterfaceBindings>
struct dependencies_registered;

template <typename DependencyList, typename InterfaceBindings>
struct first_missing_declared_dependency;

template <typename InterfaceBindings>
struct first_missing_declared_dependency<void, InterfaceBindings> {
    using type = void;
};

template <typename InterfaceBindings>
struct first_missing_declared_dependency<type_list<>, InterfaceBindings> {
    using type = void;
};

template <typename Head, typename... Tail, typename InterfaceBindings>
struct first_missing_declared_dependency<type_list<Head, Tail...>, InterfaceBindings> {
  private:
    using dependency_type = binding_request_interface_t<Head>;
    using dependency_key = binding_request_key_t<Head>;

  public:
    using type = std::conditional_t<
        std::is_void_v<binding_t<dependency_type, dependency_key,
                                               InterfaceBindings>>,
        Head,
        typename first_missing_declared_dependency<type_list<Tail...>,
                                                  InterfaceBindings>::type>;
};

template <typename DependencyList, typename InterfaceBindings>
using first_missing_declared_dependency_t =
    typename first_missing_declared_dependency<DependencyList, InterfaceBindings>::type;

template <typename InterfaceBindings>
struct dependencies_registered<void, InterfaceBindings> : std::false_type {};

template <typename InterfaceBindings>
struct dependencies_registered<type_list<>, InterfaceBindings> : std::true_type {
};

template <typename Head, typename... Tail, typename InterfaceBindings>
struct dependencies_registered<type_list<Head, Tail...>, InterfaceBindings>
    : std::bool_constant<
          binding_count_v<binding_request_interface_t<Head>,
                                    binding_request_key_t<Head>,
                                    InterfaceBindings> == 1 &&
          dependencies_registered<type_list<Tail...>, InterfaceBindings>::value> {
};

template <typename InterfaceBindings>
struct dependency_bindings<void, InterfaceBindings> {
    using type = void;
};

template <typename InterfaceBindings>
struct dependency_bindings<type_list<>, InterfaceBindings> {
    using type = type_list<>;
};

template <typename... Dependencies, typename InterfaceBindings>
struct dependency_bindings<type_list<Dependencies...>, InterfaceBindings> {
    using type = type_list<
        binding_t<binding_request_interface_t<Dependencies>,
                                binding_request_key_t<Dependencies>,
                                InterfaceBindings>...>;
};

template <typename ResolvedBindings>
struct dependency_bindings_are_resolved : std::false_type {};

template <>
struct dependency_bindings_are_resolved<void> : std::false_type {};

template <>
struct dependency_bindings_are_resolved<type_list<>> : std::true_type {};

template <typename Head, typename... Tail>
struct dependency_bindings_are_resolved<type_list<Head, Tail...>>
    : std::bool_constant<!std::is_void_v<Head> &&
                         dependency_bindings_are_resolved<
                             type_list<Tail...>>::value> {};

template <typename BindingModel, typename InterfaceBindings>
using resolved_dependency_bindings_t =
    typename binding_dependency_resolution<BindingModel, InterfaceBindings>::type;

template <typename BindingModel, typename InterfaceBindings>
inline constexpr dependency_resolution_status binding_dependency_resolution_status_v =
    binding_dependency_resolution<BindingModel, InterfaceBindings>::status;

template <typename BindingModel, typename StatusTag, typename = void>
struct inferred_dependency_problem_type {
    using type = void;
};

template <typename BindingModel>
struct inferred_dependency_problem_type<
    BindingModel, std::integral_constant<dependency_resolution_status,
                                         dependency_resolution_status::missing>,
    std::enable_if_t<
        is_plain_constructor_factory<typename BindingModel::factory_type>::value>> {
    using type =
        typename constructor_factory_target<typename BindingModel::factory_type>::type;
};

template <typename BindingModel>
struct inferred_dependency_problem_type<
    BindingModel, std::integral_constant<dependency_resolution_status,
                                         dependency_resolution_status::ambiguous>,
    std::enable_if_t<
        is_plain_constructor_factory<typename BindingModel::factory_type>::value>> {
    using type =
        typename constructor_factory_target<typename BindingModel::factory_type>::type;
};

template <typename BindingModel>
using inferred_missing_problem_type_t = typename inferred_dependency_problem_type<
    BindingModel, std::integral_constant<dependency_resolution_status,
                                         dependency_resolution_status::missing>>::type;

template <typename BindingModel>
using inferred_ambiguous_problem_type_t = typename inferred_dependency_problem_type<
    BindingModel, std::integral_constant<dependency_resolution_status,
                                         dependency_resolution_status::ambiguous>>::type;

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
struct binding_declared_dependencies_resolved<BindingModel, InterfaceBindings, true>
    : std::bool_constant<
          std::is_void_v<first_missing_declared_dependency_t<
              binding_dependencies_t<BindingModel>, InterfaceBindings>>> {};

template <typename BindingModel, typename InterfaceBindings>
struct binding_declared_dependencies_resolved<BindingModel, InterfaceBindings, false>
    : std::true_type {};

template <typename BindingModel, typename InterfaceBindings,
          bool DependenciesResolved =
              binding_declared_dependencies_resolved<BindingModel,
                                                    InterfaceBindings>::value>
struct binding_declared_dependency_diagnostic;

template <typename BindingModel, typename InterfaceBindings>
struct binding_declared_dependency_diagnostic<BindingModel, InterfaceBindings, true>
    : std::true_type {};

template <typename BindingModel, typename InterfaceBindings>
struct binding_declared_dependency_diagnostic<BindingModel, InterfaceBindings, false>
    : declared_dependency_diagnostic<
          BindingModel,
          first_missing_declared_dependency_t<binding_dependencies_t<BindingModel>,
                                             InterfaceBindings>> {};

template <typename BindingModel, typename InterfaceBindings,
          bool HasKnownDependencies =
              !std::is_same_v<binding_dependencies_t<BindingModel>, void>>
struct binding_inferred_dependencies_resolved;

template <typename BindingModel, typename ProblemType,
          bool Valid = std::is_void_v<ProblemType>>
struct inferred_missing_dependency_diagnostic;

template <typename BindingModel, typename ProblemType>
struct inferred_missing_dependency_diagnostic<BindingModel, ProblemType, true>
    : std::true_type {};

template <typename BindingModel, typename ProblemType>
struct inferred_missing_dependency_diagnostic<BindingModel, ProblemType, false> {
    static_assert(
        dependent_false_v<BindingModel, ProblemType>,
        "bindings<...> source requires every inferred constructor "
        "dependency to map to an interface binding");
    static constexpr bool value = false;
};

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_dependencies_resolved<BindingModel, InterfaceBindings, true>
    : std::true_type {};

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_dependencies_resolved<BindingModel, InterfaceBindings, false>
    : std::bool_constant<
          binding_dependency_resolution_status_v<BindingModel, InterfaceBindings> !=
          dependency_resolution_status::missing> {};

template <typename BindingModel, typename InterfaceBindings,
          bool DependenciesResolved =
              binding_inferred_dependencies_resolved<BindingModel,
                                                    InterfaceBindings>::value>
struct binding_inferred_dependency_diagnostic;

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_dependency_diagnostic<BindingModel, InterfaceBindings, true>
    : std::true_type {};

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_dependency_diagnostic<BindingModel, InterfaceBindings, false>
    : inferred_missing_dependency_diagnostic<
          BindingModel, std::conditional_t<
                            binding_dependency_resolution_status_v<BindingModel,
                                                                   InterfaceBindings> ==
                                dependency_resolution_status::missing,
                            inferred_missing_problem_type_t<BindingModel>, void>> {
};

template <typename BindingModel, typename InterfaceBindings,
          bool HasKnownDependencies =
              !std::is_same_v<binding_dependencies_t<BindingModel>, void>>
struct binding_inferred_dependencies_unambiguous;

template <typename BindingModel, typename ProblemType,
          bool Valid = std::is_void_v<ProblemType>>
struct inferred_ambiguity_diagnostic;

template <typename BindingModel, typename ProblemType>
struct inferred_ambiguity_diagnostic<BindingModel, ProblemType, true>
    : std::true_type {};

template <typename BindingModel, typename ProblemType>
struct inferred_ambiguity_diagnostic<BindingModel, ProblemType, false> {
    static_assert(
        dependent_false_v<BindingModel, ProblemType>,
        "bindings<...> source requires every inferred constructor "
        "dependency to map to exactly one interface binding");
    static constexpr bool value = false;
};

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_dependencies_unambiguous<BindingModel, InterfaceBindings,
                                                 true> : std::true_type {};

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_dependencies_unambiguous<BindingModel, InterfaceBindings,
                                                 false>
    : std::bool_constant<
          binding_dependency_resolution_status_v<BindingModel, InterfaceBindings> !=
          dependency_resolution_status::ambiguous> {};

template <typename BindingModel, typename InterfaceBindings,
          bool DependenciesUnambiguous =
              binding_inferred_dependencies_unambiguous<BindingModel,
                                                        InterfaceBindings>::value>
struct binding_inferred_ambiguity_diagnostic;

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_ambiguity_diagnostic<BindingModel, InterfaceBindings, true>
    : std::true_type {};

template <typename BindingModel, typename InterfaceBindings>
struct binding_inferred_ambiguity_diagnostic<BindingModel, InterfaceBindings, false>
    : inferred_ambiguity_diagnostic<
          BindingModel, std::conditional_t<
                            binding_dependency_resolution_status_v<BindingModel,
                                                                   InterfaceBindings> ==
                                dependency_resolution_status::ambiguous,
                            inferred_ambiguous_problem_type_t<BindingModel>, void>> {
};

template <typename BindingModel>
struct binding_factory_is_compile_time_bindable
    : std::bool_constant<
          factory_traits<typename BindingModel::factory_type>::is_compile_time_bindable> {
};

template <typename... Registrations>
using static_registry_interface_bindings_t =
    type_list_cat_t<typename binding_expansion<
        binding_model<Registrations>>::interface_bindings...>;

template <typename InterfaceBindings, typename... BindingModels>
struct static_registry_dependency_diagnostics
    // Keep dependency booleans available on the source itself so hybrid and
    // partial-static paths can branch on them. Emit the detailed diagnostics
    // only when a fully static path instantiates this helper.
    : binding_declared_dependency_diagnostic<BindingModels, InterfaceBindings>...,
      binding_inferred_dependency_diagnostic<BindingModels, InterfaceBindings>...,
      binding_inferred_ambiguity_diagnostic<BindingModels, InterfaceBindings>... {
};

} // namespace detail

template <typename... Registrations>
struct static_registry {
    using registration_types = type_list<Registrations...>;
    using binding_models = type_list<detail::binding_model<Registrations>...>;
    using interface_bindings =
        detail::static_registry_interface_bindings_t<Registrations...>;

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

    static_assert(registrations_valid,
                  "bindings<...> source requires valid compile-time registrations");
    static_assert(
        factories_are_compile_time_bindable,
        "bindings<...> source requires compile-time-bindable factories");
    template <typename Interface, typename Key = void>
    using bindings =
        detail::bindings_t<
            detail::binding_request_interface_t<Interface>,
            std::conditional_t<std::is_void_v<Key>,
                               detail::binding_request_key_t<Interface>, Key>,
            interface_bindings>;

    template <typename Interface, typename Key = void>
    using binding =
        detail::binding_t<
            detail::binding_request_interface_t<Interface>,
            std::conditional_t<std::is_void_v<Key>,
                               detail::binding_request_key_t<Interface>, Key>,
            interface_bindings>;

    template <typename Interface>
    using model = typename binding<Interface>::binding_model_type;

    template <typename Interface>
    using dependencies =
        typename model<Interface>::dependencies_type::type;

    template <typename Interface>
    using dependency_bindings = detail::resolved_dependency_bindings_t<
        model<Interface>, interface_bindings>;
};

} // namespace dingo
