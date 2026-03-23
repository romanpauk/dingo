//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/annotated.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/constructor_detection.h>
#include <dingo/rebind_type.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/type_list.h>
#include <dingo/type_registration.h>
#include <dingo/type_traits.h>

#include <functional>
#include <type_traits>
#include <utility>

namespace dingo {

template <typename... Policies> struct registration {
    using policies = type_list<Policies...>;
};

template <typename Id, typename... Policies> struct indexed_registration {
    using id_type = Id;
    using policies = type_list<Policies...>;
};

template <typename T, T Value> struct value_id {
    using value_type = T;
    static constexpr value_type value = Value;

    static constexpr value_type get() { return value; }
};

template <typename Request, typename Id> struct indexed_request {
    using request_type = Request;
    using id_type = Id;
};

template <typename... Entries> struct bindings {
    using entries = type_list<Entries...>;
};

namespace detail {

struct no_binding_id {};
struct ambiguous_binding {};

template <typename...> struct dependent_false : std::false_type {};

template <typename Request, typename Value>
using bound_value_storage_t =
    std::conditional_t<std::is_lvalue_reference_v<Request>,
                       std::reference_wrapper<std::remove_reference_t<Value>>,
                       std::decay_t<Value>>;

template <typename Request, typename Id, typename Stored>
class bound_value {
  public:
    using request_type = Request;
    using id_type = Id;

    template <typename T>
    explicit bound_value(T&& value) : value_(std::forward<T>(value)) {}

    decltype(auto) get() & {
        if constexpr (std::is_lvalue_reference_v<Request>) {
            return value_.get();
        } else {
            return (value_);
        }
    }

    decltype(auto) get() && {
        if constexpr (std::is_lvalue_reference_v<Request>) {
            return value_.get();
        } else {
            return std::move(value_);
        }
    }

  private:
    Stored value_;
};

template <typename T> struct is_bound_value : std::false_type {};

template <typename Request, typename Id, typename Stored>
struct is_bound_value<bound_value<Request, Id, Stored>>
    : std::true_type {};

template <typename T>
static constexpr bool is_bound_value_v =
    is_bound_value<T>::value;

template <typename Bind, typename RequestKey, typename Id>
struct bound_value_matches_request
    : std::bool_constant<
          is_bound_value_v<Bind> &&
          std::is_same_v<decay_t<typename Bind::request_type>, RequestKey> &&
          std::is_same_v<typename Bind::id_type, Id>> {};

template <typename Binds, typename RequestKey, typename Id>
struct find_bound_value;

template <typename RequestKey, typename Id>
struct find_bound_value<type_list<>, RequestKey, Id> {
    using type = no_binding_id;
};

template <typename RequestKey, typename Id, typename Head, typename... Tail>
struct find_bound_value<type_list<Head, Tail...>, RequestKey, Id> {
    using type = std::conditional_t<
        bound_value_matches_request<Head, RequestKey, Id>::value, Head,
        typename find_bound_value<type_list<Tail...>, RequestKey,
                                            Id>::type>;
};

template <typename Binds, typename RequestKey, typename Id>
using find_bound_value_t =
    typename find_bound_value<Binds, RequestKey, Id>::type;

template <typename Entry> struct binding_definitions;

template <typename... Policies> struct binding_definition {
    using registration_type = type_registration<Policies...>;
    using interface_list = typename registration_type::interface_type::type;
    using scope = typename registration_type::scope_type::type;
    using storage = typename registration_type::storage_type::type;
    using factory = typename registration_type::factory_type::type;
    using conversions = typename registration_type::conversions_type::type;
    using implementation = decay_t<storage>;
    using id_type = no_binding_id;
};

template <typename Id, typename... Policies> struct indexed_binding_definition {
    using registration_type = type_registration<Policies...>;
    using interface_list = typename registration_type::interface_type::type;
    using scope = typename registration_type::scope_type::type;
    using storage = typename registration_type::storage_type::type;
    using factory = typename registration_type::factory_type::type;
    using conversions = typename registration_type::conversions_type::type;
    using implementation = decay_t<storage>;
    using id_type = Id;
};

template <typename... Policies>
struct binding_definitions<registration<Policies...>> {
    using type = type_list<binding_definition<Policies...>>;
};

template <typename Id, typename... Policies>
struct binding_definitions<indexed_registration<Id, Policies...>> {
    using type = type_list<indexed_binding_definition<Id, Policies...>>;
};

template <typename... Entries> struct binding_definitions<bindings<Entries...>> {
    using type = type_list_cat_t<typename binding_definitions<Entries>::type...>;
};

template <typename Entry>
using binding_definitions_t = typename binding_definitions<Entry>::type;

template <typename Binding, typename RequestKey, typename Id>
struct binding_matches
    : std::bool_constant<
          std::is_same_v<typename Binding::id_type, Id> &&
          type_list_contains_v<typename Binding::interface_list, RequestKey>> {};

template <typename Bindings, typename RequestKey, typename Id,
          typename Found = no_binding_id>
struct find_binding_definition;

template <typename RequestKey, typename Id, typename Found>
struct find_binding_definition<type_list<>, RequestKey, Id, Found> {
    using type = Found;
};

template <typename RequestKey, typename Id, typename Found, typename Head,
          typename... Tail>
struct find_binding_definition<type_list<Head, Tail...>, RequestKey, Id, Found> {
  private:
    using next_found = std::conditional_t<
        binding_matches<Head, RequestKey, Id>::value,
        std::conditional_t<std::is_same_v<Found, no_binding_id>, Head,
                           ambiguous_binding>,
        Found>;

  public:
    using type =
        typename find_binding_definition<type_list<Tail...>, RequestKey, Id,
                                        next_found>::type;
};

template <typename Bindings, typename RequestKey, typename Id>
using find_binding_definition_t =
    typename find_binding_definition<Bindings, RequestKey, Id>::type;

template <typename Component, typename Bind>
using bound_binding_t = find_binding_definition_t<
    binding_definitions_t<Component>, decay_t<typename Bind::request_type>,
    typename Bind::id_type>;

template <typename Binding, typename = void>
struct bound_targets_external : std::false_type {};

template <typename Binding>
struct bound_targets_external<
    Binding, std::void_t<typename Binding::scope>>
    : std::bool_constant<std::is_same_v<typename Binding::scope, external>> {};

template <typename Component, typename Bind>
struct bound_valid
    : std::bool_constant<
          bound_targets_external<
              bound_binding_t<Component, Bind>>::value> {};

template <typename Binding, typename Component, typename Bind>
struct bound_matches_binding
    : std::bool_constant<
          bound_valid<Component, Bind>::value &&
          std::is_same_v<Binding, bound_binding_t<Component, Bind>>> {};

template <typename Binding, typename Component, typename... Binds>
struct binding_bound_count
    : std::integral_constant<size_t,
                             (0u + ... +
                              (bound_matches_binding<
                                   Binding, Component, Binds>::value
                                   ? 1u
                                   : 0u))> {};

template <typename Bindings> struct external_binding_count_for_bindings;

template <typename... Bindings>
struct external_binding_count_for_bindings<type_list<Bindings...>>
    : std::integral_constant<
          size_t,
          (0u + ... +
           (std::is_same_v<typename Bindings::scope, external> ? 1u : 0u))> {};

template <typename Component, typename Bindings, typename... Binds>
struct external_bindings_satisfied;

template <typename Component, typename... Bindings, typename... Binds>
struct external_bindings_satisfied<Component, type_list<Bindings...>,
                                   Binds...>
    : std::bool_constant<
          (((!std::is_same_v<typename Bindings::scope, external>) ||
            binding_bound_count<Bindings, Component, Binds...>::value == 1) &&
           ...)> {};

template <typename Component, typename Bindings, typename... Binds>
struct bind_argument_validation;

template <typename Component, typename... Bindings, typename... Binds>
struct bind_argument_validation<Component, type_list<Bindings...>, Binds...>
    : std::true_type {
    static_assert((is_bound_value_v<Binds> && ...),
                  "register_bindings expects dingo::bind(...) arguments");
    static_assert((bound_valid<Component, Binds>::value && ...),
                  "bind does not match an external bindings entry");
    static_assert(
        external_bindings_satisfied<Component, type_list<Bindings...>,
                                    Binds...>::value,
        "external bindings entries must be supplied exactly once");
};

template <typename Binding, typename Component>
void find_external_bind() {
    static_assert(dependent_false<Binding, Component>::value,
                  "missing external bindings entry");
}

template <typename Binding, typename Component, typename FirstBind,
          typename... RestBinds>
decltype(auto) find_external_bind(FirstBind&& first_bind,
                                  RestBinds&&... rest_binds) {
    if constexpr (bound_matches_binding<Binding, Component,
                                        decay_t<FirstBind>>::value) {
        return std::forward<FirstBind>(first_bind);
    } else {
        return find_external_bind<Binding, Component>(
            std::forward<RestBinds>(rest_binds)...);
    }
}

template <typename Request>
using request_runtime_type_t =
    rebind_type_t<typename annotated_traits<Request>::type, runtime_type>;

template <typename Binding, typename RequestRuntimeType>
struct binding_supports_request {
  private:
    using value_types = typename Binding::conversions::value_types;
    using lvalue_reference_types =
        typename Binding::conversions::lvalue_reference_types;
    using rvalue_reference_types =
        typename Binding::conversions::rvalue_reference_types;
    using pointer_types = typename Binding::conversions::pointer_types;

  public:
    static constexpr bool value =
        std::conditional_t<std::is_pointer_v<RequestRuntimeType>,
                           type_list_contains<pointer_types, RequestRuntimeType>,
                           std::conditional_t<
                               std::is_lvalue_reference_v<RequestRuntimeType>,
                               type_list_contains<lvalue_reference_types,
                                                  RequestRuntimeType>,
                               std::conditional_t<
                                   std::is_rvalue_reference_v<RequestRuntimeType>,
                                   type_list_contains<rvalue_reference_types,
                                                      RequestRuntimeType>,
                                   type_list_contains<value_types,
                                                      RequestRuntimeType>>>>::value;
};

template <typename Component, typename Request, typename SeenBindings,
          typename ProvidedBindings = type_list<>,
          typename Id = no_binding_id>
struct bindings_resolvable_impl;

template <typename Component, typename SeenBindings, typename ProvidedBindings,
          typename BaseTag>
struct bindings_probe_tag {};

template <typename DisabledType, typename Component, typename SeenBindings,
          typename ProvidedBindings>
struct constructor_argument<
    DisabledType,
    bindings_probe_tag<Component, SeenBindings, ProvidedBindings, automatic>> {
    using tag_type = bindings_probe_tag<Component, SeenBindings,
                                         ProvidedBindings, automatic>;

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  bindings_resolvable_impl<Component, T&, SeenBindings,
                                            ProvidedBindings>::value>>
    operator T&() const;

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  bindings_resolvable_impl<Component, const T&, SeenBindings,
                                            ProvidedBindings>::value>>
    operator const T&() const;

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  bindings_resolvable_impl<Component, T&&, SeenBindings,
                                            ProvidedBindings>::value>>
    operator T&&() const;

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  bindings_resolvable_impl<Component, T, SeenBindings,
                                            ProvidedBindings>::value>>
    operator T();

    template <typename T, typename Tag,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  bindings_resolvable_impl<Component, annotated<T, Tag>,
                                            SeenBindings,
                                            ProvidedBindings>::value>>
    operator annotated<T, Tag>();
};

template <typename DisabledType, typename Component, typename SeenBindings,
          typename ProvidedBindings>
struct constructor_argument<
    DisabledType,
    bindings_probe_tag<Component, SeenBindings, ProvidedBindings, reference>> {
    using tag_type = bindings_probe_tag<Component, SeenBindings,
                                         ProvidedBindings, reference>;

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  bindings_resolvable_impl<Component, T*, SeenBindings,
                                            ProvidedBindings>::value>>
    operator T*();

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  bindings_resolvable_impl<Component, T&, SeenBindings,
                                            ProvidedBindings>::value>>
    operator T&();

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  bindings_resolvable_impl<Component, T&&, SeenBindings,
                                            ProvidedBindings>::value>>
    operator T&&();

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  bindings_resolvable_impl<Component, std::unique_ptr<T>,
                                            SeenBindings,
                                            ProvidedBindings>::value>>
    operator std::unique_ptr<T>();

    template <typename T, typename Tag,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  bindings_resolvable_impl<Component, annotated<T, Tag>,
                                            SeenBindings,
                                            ProvidedBindings>::value>>
    operator annotated<T, Tag>();
};

template <typename DisabledType, typename Component, typename SeenBindings,
          typename ProvidedBindings>
struct constructor_argument<
    DisabledType,
    bindings_probe_tag<Component, SeenBindings, ProvidedBindings, value>> {
    using tag_type = bindings_probe_tag<Component, SeenBindings,
                                         ProvidedBindings, value>;

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  bindings_resolvable_impl<Component, T, SeenBindings,
                                            ProvidedBindings>::value>>
    operator T();

    template <typename T, typename Tag,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  bindings_resolvable_impl<Component, annotated<T, Tag>,
                                            SeenBindings,
                                            ProvidedBindings>::value>>
    operator annotated<T, Tag>();
};

template <typename Component, typename SeenBindings,
          typename ProvidedBindings = type_list<>>
struct bindings_context {
    template <typename T, typename Container,
              typename = std::enable_if_t<
                  bindings_resolvable_impl<Component, T, SeenBindings,
                                            ProvidedBindings>::value>>
    T resolve(Container&);
};

struct bindings_container {};

struct bindings_no_matching_arguments {};

template <typename Argument, typename Component, typename SeenBindings,
          typename ProvidedBindings = type_list<>, typename = void>
struct bindings_candidate_argument {};

template <typename Argument, typename Component, typename SeenBindings,
          typename ProvidedBindings>
struct bindings_candidate_argument<
    Argument, Component, SeenBindings, ProvidedBindings,
    std::enable_if_t<
        bindings_resolvable_impl<Component, Argument, SeenBindings,
                                  ProvidedBindings>::value>> {
    operator Argument();
};

template <typename...> struct tuple_repeat_impl;

template <typename T, typename... Types>
struct tuple_repeat_impl<std::integral_constant<size_t, 0>, T, Types...> {
    using type = std::tuple<Types...>;
};

template <size_t N, typename T, typename... Types>
struct tuple_repeat_impl<std::integral_constant<size_t, N>, T, Types...>
    : tuple_repeat_impl<std::integral_constant<size_t, N - 1>, T, Types..., T> {};

template <typename T, size_t N>
using tuple_repeat_t =
    typename tuple_repeat_impl<std::integral_constant<size_t, N>, T>::type;

template <typename Tuple, typename T> struct tuple_append;

template <typename... Types, typename T>
struct tuple_append<std::tuple<Types...>, T> {
    using type = std::tuple<Types..., T>;
};

template <typename Tuple, typename T>
using tuple_append_t = typename tuple_append<Tuple, T>::type;

template <typename Interface, typename Request> struct rebind_request {
    using type = rebind_type_t<Request, decay_t<typename annotated_traits<Interface>::type>>;
};

template <typename T, typename Tag, typename Request>
struct rebind_request<annotated<T, Tag>, Request> {
    using type = annotated<rebind_type_t<Request, decay_t<T>>, Tag>;
};

template <typename Request, typename = void>
struct bindings_const_lvalue_request {
    using type = std::add_lvalue_reference_t<
        std::add_const_t<std::remove_reference_t<Request>>>;
};

template <typename T, typename Tag>
struct bindings_const_lvalue_request<annotated<T, Tag>> {
    using type = annotated<
        std::add_lvalue_reference_t<std::add_const_t<std::remove_reference_t<T>>>,
        Tag>;
};

template <typename Request>
using bindings_const_lvalue_request_t =
    typename bindings_const_lvalue_request<Request>::type;

template <typename TypeList, typename Interface> struct rebind_request_list;

template <typename... Requests, typename Interface>
struct rebind_request_list<type_list<Requests...>, Interface> {
    using type =
        type_list<typename rebind_request<Interface, Requests>::type...>;
};

template <typename TypeList, typename Interface>
using rebind_request_list_t =
    typename rebind_request_list<TypeList, Interface>::type;

template <typename TypeList> struct bindings_const_lvalue_request_list;

template <typename... Requests>
struct bindings_const_lvalue_request_list<type_list<Requests...>> {
    using type = type_list<bindings_const_lvalue_request_t<Requests>...>;
};

template <typename TypeList>
using bindings_const_lvalue_request_list_t =
    typename bindings_const_lvalue_request_list<TypeList>::type;

template <typename T> struct bindings_is_unique_ptr_request : std::false_type {};

template <typename T, typename Deleter>
struct bindings_is_unique_ptr_request<std::unique_ptr<T, Deleter>>
    : std::true_type {};

template <typename T, typename Tag>
struct bindings_is_unique_ptr_request<annotated<T, Tag>>
    : bindings_is_unique_ptr_request<T> {};

template <typename Interface, typename Binding, typename DetectionType>
struct bindings_interface_candidate_requests;

template <typename Interface, typename Binding>
struct bindings_interface_candidate_requests<Interface, Binding, automatic> {
  private:
    using lvalue_requests = rebind_request_list_t<
        typename Binding::conversions::lvalue_reference_types, Interface>;
    using const_lvalue_requests =
        bindings_const_lvalue_request_list_t<lvalue_requests>;
    using rvalue_requests = rebind_request_list_t<
        typename Binding::conversions::rvalue_reference_types, Interface>;
    using value_requests = rebind_request_list_t<
        typename Binding::conversions::value_types, Interface>;

  public:
    using type = type_list_cat_t<rvalue_requests, const_lvalue_requests,
                                 lvalue_requests, value_requests>;
};

template <typename Interface, typename Binding>
struct bindings_interface_candidate_requests<Interface, Binding, reference> {
  private:
    using pointer_requests = rebind_request_list_t<
        typename Binding::conversions::pointer_types, Interface>;
    using lvalue_requests = rebind_request_list_t<
        typename Binding::conversions::lvalue_reference_types, Interface>;
    using rvalue_requests = rebind_request_list_t<
        typename Binding::conversions::rvalue_reference_types, Interface>;
    using unique_requests = type_list_filter_t<
        bindings_is_unique_ptr_request,
        rebind_request_list_t<typename Binding::conversions::value_types,
                                      Interface>>;

  public:
    using type = type_list_cat_t<pointer_requests, lvalue_requests,
                                 rvalue_requests, unique_requests>;
};

template <typename Interface, typename Binding>
struct bindings_interface_candidate_requests<Interface, Binding, value> {
    using type = rebind_request_list_t<
        typename Binding::conversions::value_types, Interface>;
};

template <typename Binding, typename DetectionType, typename Interfaces>
struct binding_candidate_requests;

template <typename Binding, typename DetectionType, typename... Interfaces>
struct binding_candidate_requests<Binding, DetectionType,
                                           type_list<Interfaces...>> {
    using type = type_list_cat_t<
        typename bindings_interface_candidate_requests<Interfaces, Binding,
                                                        DetectionType>::type...>;
};

template <typename Component, typename DetectionType, typename Bindings>
struct bindings_candidate_arguments_impl;

template <typename ProvidedBindings> struct bindings_provided_arguments;

template <typename... Binds>
struct bindings_provided_arguments<type_list<Binds...>> {
    using type = type_list<typename Binds::request_type...>;
};

template <typename ProvidedBindings>
using bindings_provided_arguments_t =
    typename bindings_provided_arguments<ProvidedBindings>::type;

template <typename Component, typename DetectionType, typename... Bindings>
struct bindings_candidate_arguments_impl<Component, DetectionType,
                                          type_list<Bindings...>> {
    using type = type_list_unique_t<type_list_cat_t<
        typename binding_candidate_requests<
            Bindings, DetectionType, typename Bindings::interface_list>::type...>>;
};

template <typename Component, typename DetectionType,
          typename ProvidedBindings = type_list<>>
using bindings_candidate_arguments_t =
    type_list_unique_t<type_list_cat_t<
        typename bindings_candidate_arguments_impl<
            Component, DetectionType, binding_definitions_t<Component>>::type,
        bindings_provided_arguments_t<ProvidedBindings>>>;

template <typename T, typename Probe, typename Candidate, typename Component,
          typename SeenBindings, typename ProvidedBindings, size_t Slot,
          size_t Arity>
struct bindings_slot_candidate_constructible {
  private:
    using probe_tuple = tuple_repeat_t<Probe, Arity>;
    using candidate_tuple = typename tuple_replace<
        probe_tuple, Slot,
        bindings_candidate_argument<Candidate, Component, SeenBindings,
                                     ProvidedBindings>>::type;

  public:
    static constexpr bool value =
        detail::is_list_initializable_v<T, candidate_tuple> ||
        detail::is_direct_initializable_v<T, candidate_tuple>;
};

template <typename T, typename CandidateList, typename Probe, typename Component,
          typename SeenBindings, typename ProvidedBindings, size_t Slot,
          size_t Arity>
struct bindings_slot_candidate_arguments;

template <typename T, typename Probe, typename Component, typename SeenBindings,
          typename ProvidedBindings, size_t Slot, size_t Arity>
struct bindings_slot_candidate_arguments<T, type_list<>, Probe, Component,
                                          SeenBindings, ProvidedBindings, Slot,
                                          Arity> {
    using type = type_list<>;
};

template <typename T, typename Head, typename... Tail, typename Probe,
          typename Component, typename SeenBindings, typename ProvidedBindings,
          size_t Slot, size_t Arity>
struct bindings_slot_candidate_arguments<T, type_list<Head, Tail...>, Probe,
                                          Component, SeenBindings,
                                          ProvidedBindings, Slot, Arity> {
  private:
    using tail_type =
        typename bindings_slot_candidate_arguments<
            T, type_list<Tail...>, Probe, Component, SeenBindings,
            ProvidedBindings, Slot,
            Arity>::type;

  public:
    using type = std::conditional_t<
        bindings_slot_candidate_constructible<T, Probe, Head, Component,
                                              SeenBindings, ProvidedBindings,
                                              Slot,
                                              Arity>::value,
        type_list_cat_t<type_list<Head>, tail_type>, tail_type>;
};

template <typename T, typename CandidateList, typename Probe, typename Component,
          typename SeenBindings, typename ProvidedBindings, size_t Slot,
          size_t Arity>
using bindings_slot_candidate_arguments_t =
    typename bindings_slot_candidate_arguments<
        T, CandidateList, Probe, Component, SeenBindings, ProvidedBindings,
        Slot, Arity>::type;

template <typename T, typename Tuple> struct bindings_argument_tuple_constructible;

template <typename T, typename... Requests>
struct bindings_argument_tuple_constructible<T, std::tuple<Requests...>>
    : std::bool_constant<detail::is_list_initializable_v<T, Requests...> ||
                         detail::is_direct_initializable_v<T, Requests...>> {};

template <typename T, typename RemainingSlotLists, typename SelectedTuple>
struct bindings_argument_tuple_search;

template <typename T, typename SelectedTuple>
struct bindings_argument_tuple_search<T, type_list<>, SelectedTuple> {
    using type = std::conditional_t<
        bindings_argument_tuple_constructible<T, SelectedTuple>::value,
        SelectedTuple, bindings_no_matching_arguments>;
};

template <typename T, typename CandidateList, typename RemainingSlotLists,
          typename SelectedTuple>
struct bindings_argument_tuple_search_candidates;

template <typename T, typename RemainingSlotLists, typename SelectedTuple>
struct bindings_argument_tuple_search_candidates<T, type_list<>,
                                                 RemainingSlotLists,
                                                 SelectedTuple> {
    using type = bindings_no_matching_arguments;
};

template <typename T, typename Head, typename... Tail,
          typename RemainingSlotLists, typename SelectedTuple>
struct bindings_argument_tuple_search_candidates<T, type_list<Head, Tail...>,
                                                 RemainingSlotLists,
                                                 SelectedTuple> {
  private:
    using selected_type = typename bindings_argument_tuple_search<
        T, RemainingSlotLists, tuple_append_t<SelectedTuple, Head>>::type;

  public:
    using type = std::conditional_t<
        !std::is_same_v<selected_type, bindings_no_matching_arguments>,
        selected_type,
        typename bindings_argument_tuple_search_candidates<
            T, type_list<Tail...>, RemainingSlotLists, SelectedTuple>::type>;
};

template <typename T, typename HeadSlotList, typename... TailSlotLists,
          typename SelectedTuple>
struct bindings_argument_tuple_search<
    T, type_list<HeadSlotList, TailSlotLists...>, SelectedTuple> {
    using type = typename bindings_argument_tuple_search_candidates<
        T, HeadSlotList, type_list<TailSlotLists...>, SelectedTuple>::type;
};

template <typename T, typename DetectionType, typename Probe,
          typename CandidateRequests, typename Component, typename SeenBindings,
          typename ProvidedBindings, typename Seq>
struct bindings_autodetected_argument_types_impl;

template <typename T, typename DetectionType, typename Probe,
          typename CandidateRequests, typename Component, typename SeenBindings,
          typename ProvidedBindings, size_t... Slots>
struct bindings_autodetected_argument_types_impl<
    T, DetectionType, Probe, CandidateRequests, Component, SeenBindings,
    ProvidedBindings, std::index_sequence<Slots...>> {
    // Autodetection only gives us the winning arity. Re-run that arity with
    // Re-run that arity with bindings-constrained requests so the checked
    // bindings graph and bindings_container share one concrete dependency tuple.
    using type = typename bindings_argument_tuple_search<
        T,
        type_list<bindings_slot_candidate_arguments_t<
            T, CandidateRequests, Probe, Component, SeenBindings,
            ProvidedBindings, Slots,
            sizeof...(Slots)>...>,
        std::tuple<>>::type;
};

template <typename T, typename DetectionType, typename Component,
          typename SeenBindings, typename ProvidedBindings = type_list<>,
          typename = void>
struct bindings_autodetected_argument_types : std::false_type {
    using type = bindings_no_matching_arguments;
    static constexpr bool valid = false;
};

template <typename T, typename DetectionType, typename Component,
          typename SeenBindings, typename ProvidedBindings>
struct bindings_autodetected_arguments {
  private:
    using probe =
        constructor_argument<T,
                             bindings_probe_tag<Component, SeenBindings,
                                                 ProvidedBindings,
                                                 DetectionType>>;
    using resolved_arguments = typename bindings_autodetected_argument_types_impl<
        T, DetectionType, probe,
        bindings_candidate_arguments_t<Component, DetectionType,
                                        ProvidedBindings>,
        Component,
        SeenBindings, ProvidedBindings,
        std::make_index_sequence<
            ::dingo::constructor_detection<T, DetectionType>::arity>>::type;

  public:
    using type = resolved_arguments;
};

template <typename T, typename DetectionType, typename Component,
          typename SeenBindings, typename ProvidedBindings>
struct bindings_autodetected_argument_types<
    T, DetectionType, Component, SeenBindings, ProvidedBindings,
    std::enable_if_t<::dingo::constructor_detection<T, DetectionType>::valid>>
    : std::bool_constant<!std::is_same_v<
          typename bindings_autodetected_arguments<T, DetectionType, Component,
                                                    SeenBindings,
                                                    ProvidedBindings>::type,
          bindings_no_matching_arguments>> {
    using arguments = bindings_autodetected_arguments<
        T, DetectionType, Component, SeenBindings, ProvidedBindings>;

  public:
    using type = typename arguments::type;

    static constexpr bool valid =
        !std::is_same_v<type, bindings_no_matching_arguments>;
};

template <typename Tuple, typename Component, typename SeenBindings,
          typename ProvidedBindings = type_list<>>
struct bindings_argument_tuple_resolvable;

template <typename Component, typename SeenBindings, typename ProvidedBindings,
          typename... Requests>
struct bindings_argument_tuple_resolvable<std::tuple<Requests...>, Component,
                                           SeenBindings, ProvidedBindings>
    : std::conjunction<
          bindings_resolvable_impl<Component, Requests, SeenBindings,
                                    ProvidedBindings>...> {};

template <typename Factory, typename = void>
struct bindings_factory_argument_types {
    using type = bindings_no_matching_arguments;
};

template <typename Factory>
struct bindings_factory_argument_types<
    Factory, std::void_t<typename Factory::direct_argument_types>> {
    using type = typename Factory::direct_argument_types;
};

template <typename Binding, typename Component, typename SeenBindings,
          typename ProvidedBindings = type_list<>,
          typename Factory = typename Binding::factory>
struct binding_factory_adapter_argument_types {
    using type = typename bindings_factory_argument_types<Factory>::type;
};

template <typename Binding, typename Component, typename SeenBindings,
          typename ProvidedBindings,
          typename T, typename DetectionType>
struct binding_factory_adapter_argument_types<
    Binding, Component, SeenBindings,
    ProvidedBindings,
    ::dingo::constructor_detection<T, DetectionType>> {
    using type = typename bindings_autodetected_argument_types<
        T, DetectionType, Component, SeenBindings, ProvidedBindings>::type;
};

template <typename Binding, typename Component,
          typename SeenBindings = type_list_push_unique_t<type_list<>, Binding>,
          typename ProvidedBindings = type_list<>>
using binding_factory_adapter_argument_types_t =
    typename binding_factory_adapter_argument_types<Binding, Component,
                                                     SeenBindings,
                                                     ProvidedBindings>::type;

template <typename Factory, typename Type, typename Context, typename Container,
          typename = void>
struct member_factory_constructible : std::false_type {};

template <typename Factory, typename Type, typename Context, typename Container>
struct member_factory_constructible<
    Factory, Type, Context, Container,
    std::void_t<decltype(std::declval<Factory>().template construct<Type>(
        std::declval<Context&>(), std::declval<Container&>()))>>
    : std::true_type {};

template <typename Factory, typename Type, typename Context, typename Container,
          typename = void>
struct static_factory_constructible : std::false_type {};

template <typename Factory, typename Type, typename Context, typename Container>
struct static_factory_constructible<
    Factory, Type, Context, Container,
    std::void_t<decltype(Factory::template construct<Type>(
        std::declval<Context&>(), std::declval<Container&>()))>>
    : std::true_type {};

template <typename Factory, typename StorageType, typename Component,
          typename SeenBindings, typename ProvidedBindings = type_list<>>
struct generic_factory_constructible
    : std::bool_constant<
          std::is_default_constructible_v<Factory> &&
          (member_factory_constructible<Factory, StorageType,
                                        bindings_context<Component, SeenBindings,
                                                          ProvidedBindings>,
                                        bindings_container>::value ||
           static_factory_constructible<Factory, StorageType,
                                        bindings_context<Component, SeenBindings,
                                                          ProvidedBindings>,
                                        bindings_container>::value)> {};

template <typename Factory, typename Binding, typename Component,
          typename SeenBindings, typename ProvidedBindings = type_list<>,
          typename = void>
struct factory_constructibility_dispatch
    : generic_factory_constructible<Factory, typename Binding::storage,
                                    Component, SeenBindings, ProvidedBindings> {};

template <typename Factory, typename Binding, typename Component,
          typename SeenBindings, typename ProvidedBindings = type_list<>,
          typename = void>
struct direct_factory_adapter_constructible : std::false_type {};

template <typename Factory, typename Binding, typename Component,
          typename SeenBindings, typename ProvidedBindings>
struct direct_factory_adapter_constructible<
    Factory, Binding, Component, SeenBindings, ProvidedBindings,
    std::void_t<typename bindings_factory_argument_types<Factory>::type>>
    : std::bool_constant<
          !std::is_same_v<typename bindings_factory_argument_types<Factory>::type,
                          bindings_no_matching_arguments> &&
          bindings_argument_tuple_resolvable<
              typename bindings_factory_argument_types<Factory>::type,
              Component, SeenBindings, ProvidedBindings>::value> {};

template <typename Factory, typename Binding, typename Component,
          typename SeenBindings, typename ProvidedBindings>
struct factory_constructibility_dispatch<
    Factory, Binding, Component, SeenBindings, ProvidedBindings,
    std::enable_if_t<
        !std::is_same_v<typename bindings_factory_argument_types<Factory>::type,
                        bindings_no_matching_arguments>>>
    : direct_factory_adapter_constructible<Factory, Binding, Component,
                                             SeenBindings, ProvidedBindings> {};

template <typename T, typename DetectionType, typename Binding,
          typename Component, typename SeenBindings, typename ProvidedBindings>
struct factory_constructibility_dispatch<
    ::dingo::constructor_detection<T, DetectionType>, Binding, Component,
    SeenBindings, ProvidedBindings, void>
    : bindings_autodetected_argument_types<T, DetectionType, Component,
                                            SeenBindings, ProvidedBindings> {};

template <typename Binding, typename Component, typename SeenBindings,
          typename ProvidedBindings = type_list<>>
struct bindings_factory_constructible
    : factory_constructibility_dispatch<typename Binding::factory, Binding,
                                        Component, SeenBindings,
                                        ProvidedBindings> {};

template <typename Binding, typename Component, typename SeenBindings,
          typename ProvidedBindings = type_list<>>
struct binding_constructible
    : std::conditional_t<std::is_same_v<typename Binding::scope, external>,
                         std::true_type,
                         bindings_factory_constructible<Binding, Component,
                                                         SeenBindings,
                                                         ProvidedBindings>> {};

template <typename Component, typename Request, typename SeenBindings,
          typename ProvidedBindings, typename Id>
struct bindings_resolvable_impl;

template <typename Binding, typename Component, typename SeenBindings,
          typename ProvidedBindings,
          bool RequestSupported, bool BindingSeen>
struct bindings_resolvable_state : std::false_type {};

template <typename Binding, typename Component, typename SeenBindings,
          typename ProvidedBindings,
          bool BindingSeen>
struct bindings_resolvable_state<Binding, Component, SeenBindings,
                                  ProvidedBindings, false, BindingSeen>
    : std::false_type {};

template <typename Binding, typename Component, typename SeenBindings,
          typename ProvidedBindings>
struct bindings_resolvable_state<Binding, Component, SeenBindings,
                                  ProvidedBindings, true, true>
    : std::bool_constant<std::is_same_v<typename Binding::scope, shared_cyclical>> {};

template <typename Binding, typename Component, typename SeenBindings,
          typename ProvidedBindings>
struct bindings_resolvable_state<Binding, Component, SeenBindings,
                                  ProvidedBindings, true, false>
    : binding_constructible<Binding, Component,
                            type_list_push_unique_t<SeenBindings, Binding>,
                            ProvidedBindings> {};

template <typename Component, typename Request, typename SeenBindings,
          typename ProvidedBindings, typename Id, typename Binding>
struct bindings_resolvable_binding_impl
    : bindings_resolvable_state<
          Binding, Component, SeenBindings, ProvidedBindings,
          binding_supports_request<Binding, request_runtime_type_t<Request>>::value,
          type_list_contains_v<SeenBindings, Binding>> {};

template <typename Component, typename Request, typename SeenBindings,
          typename ProvidedBindings, typename Id>
struct bindings_resolvable_binding_impl<Component, Request, SeenBindings,
                                         ProvidedBindings, Id, no_binding_id>
    : std::bool_constant<!std::is_same_v<
          find_bound_value_t<ProvidedBindings, decay_t<Request>, Id>,
          no_binding_id>> {};

template <typename Component, typename Request, typename SeenBindings,
          typename ProvidedBindings, typename Id>
struct bindings_resolvable_binding_impl<Component, Request, SeenBindings,
                                         ProvidedBindings, Id,
                                         ambiguous_binding> : std::false_type {};

template <typename Component, typename Request, typename SeenBindings,
          typename ProvidedBindings, typename Id>
struct bindings_resolvable_impl
    : bindings_resolvable_binding_impl<
          Component, Request, SeenBindings, ProvidedBindings, Id,
          find_binding_definition_t<binding_definitions_t<Component>, decay_t<Request>,
                                   Id>> {};

template <typename Root> struct request_traits {
    using request_type = Root;
    using id_type = no_binding_id;
};

template <typename Request, typename Id>
struct request_traits<indexed_request<Request, Id>> {
    using request_type = Request;
    using id_type = Id;
};

template <typename Root>
struct open_root_supported
    : std::bool_constant<
          std::is_same_v<typename request_traits<Root>::id_type,
                         no_binding_id> &&
          !std::is_reference_v<
              typename request_traits<Root>::request_type> &&
          !has_type_traits_v<std::remove_cv_t<std::remove_reference_t<
              typename request_traits<Root>::request_type>>>> {};

template <typename Component, typename Root,
          typename ProvidedBindings = type_list<>, typename = void>
struct open_root_constructible : std::false_type {
    using type = bindings_no_matching_arguments;
};

template <typename Component, typename Root, typename ProvidedBindings>
struct open_root_constructible<
    Component, Root, ProvidedBindings,
    std::enable_if_t<open_root_supported<Root>::value>> {
    using request_type = typename request_traits<Root>::request_type;
    using type = typename bindings_autodetected_argument_types<
        decay_t<request_type>, automatic, Component, type_list<>,
        ProvidedBindings>::type;

    static constexpr bool value =
        !std::is_same_v<type, bindings_no_matching_arguments>;
};

template <typename Component, typename Root,
          typename ProvidedBindings = type_list<>>
struct root_constructible
    : std::bool_constant<
          bindings_resolvable_impl<
              Component, typename request_traits<Root>::request_type,
              type_list<>, ProvidedBindings,
              typename request_traits<Root>::id_type>::value ||
          open_root_constructible<Component, Root,
                                            ProvidedBindings>::value> {};

template <typename Component, typename ProvidedBindings, typename... Roots>
struct bindings_constructible
    : std::conjunction<
          root_constructible<Component, Roots, ProvidedBindings>...> {};

template <typename Bindings> struct bindings_has_external_bindings;

template <typename... Entries>
struct bindings_has_external_bindings<bindings<Entries...>>
    : std::bool_constant<
          external_binding_count_for_bindings<
              binding_definitions_t<bindings<Entries...>>>::value != 0> {};

} // namespace detail

template <typename Component, typename Request,
          typename Id = detail::no_binding_id>
struct bindings_resolvable
    : detail::bindings_resolvable_impl<Component, Request, type_list<>,
                                        type_list<>, Id> {};

template <typename Component, typename Request,
          typename Id = detail::no_binding_id>
static constexpr bool bindings_resolvable_v =
    bindings_resolvable<Component, Request, Id>::value;

template <typename Component, typename... Roots>
struct bindings_constructible
    : detail::bindings_constructible<Component, type_list<>, Roots...> {};

template <typename Component, typename... Roots>
static constexpr bool bindings_constructible_v =
    bindings_constructible<Component, Roots...>::value;

template <typename Root, typename Value>
auto bind(Value&& value) {
    using traits = detail::request_traits<Root>;
    using request_type = typename traits::request_type;
    using id_type = typename traits::id_type;
    using stored_type = detail::bound_value_storage_t<request_type, Value>;

    return detail::bound_value<request_type, id_type, stored_type>(
        std::forward<Value>(value));
}

} // namespace dingo
