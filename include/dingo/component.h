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
#include <dingo/storage/shared_cyclical.h>
#include <dingo/type_list.h>
#include <dingo/type_registration.h>

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

template <typename... Entries> struct component {
    using entries = type_list<Entries...>;
};

namespace detail {

struct component_no_id {};
struct component_ambiguous {};

template <typename...> struct dependent_false : std::false_type {};

template <typename Entry> struct component_bindings;

template <typename... Policies> struct component_binding {
    using registration_type = type_registration<Policies...>;
    using interface_list = typename registration_type::interface_type::type;
    using scope = typename registration_type::scope_type::type;
    using storage = typename registration_type::storage_type::type;
    using factory = typename registration_type::factory_type::type;
    using conversions = typename registration_type::conversions_type::type;
    using implementation = decay_t<storage>;
    using id_type = component_no_id;
};

template <typename Id, typename... Policies> struct indexed_component_binding {
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
struct component_bindings<registration<Policies...>> {
    using type = type_list<component_binding<Policies...>>;
};

template <typename Id, typename... Policies>
struct component_bindings<indexed_registration<Id, Policies...>> {
    using type = type_list<indexed_component_binding<Id, Policies...>>;
};

template <typename... Entries> struct component_bindings<component<Entries...>> {
    using type = type_list_cat_t<typename component_bindings<Entries>::type...>;
};

template <typename Entry>
using component_bindings_t = typename component_bindings<Entry>::type;

template <typename Binding, typename RequestKey, typename Id>
struct binding_matches
    : std::bool_constant<
          std::is_same_v<typename Binding::id_type, Id> &&
          type_list_contains_v<typename Binding::interface_list, RequestKey>> {};

template <typename Bindings, typename RequestKey, typename Id,
          typename Found = component_no_id>
struct find_component_binding;

template <typename RequestKey, typename Id, typename Found>
struct find_component_binding<type_list<>, RequestKey, Id, Found> {
    using type = Found;
};

template <typename RequestKey, typename Id, typename Found, typename Head,
          typename... Tail>
struct find_component_binding<type_list<Head, Tail...>, RequestKey, Id, Found> {
  private:
    using next_found = std::conditional_t<
        binding_matches<Head, RequestKey, Id>::value,
        std::conditional_t<std::is_same_v<Found, component_no_id>, Head,
                           component_ambiguous>,
        Found>;

  public:
    using type =
        typename find_component_binding<type_list<Tail...>, RequestKey, Id,
                                        next_found>::type;
};

template <typename Bindings, typename RequestKey, typename Id>
using find_component_binding_t =
    typename find_component_binding<Bindings, RequestKey, Id>::type;

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
          typename Id = component_no_id>
struct component_resolvable_impl;

template <typename Component, typename SeenBindings, typename BaseTag>
struct component_probe_tag {};

template <typename DisabledType, typename Component, typename SeenBindings>
struct constructor_argument<
    DisabledType, component_probe_tag<Component, SeenBindings, automatic>> {
    using tag_type = component_probe_tag<Component, SeenBindings, automatic>;

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  component_resolvable_impl<Component, T&, SeenBindings>::value>>
    operator T&() const;

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  component_resolvable_impl<Component, const T&, SeenBindings>::value>>
    operator const T&() const;

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  component_resolvable_impl<Component, T&&, SeenBindings>::value>>
    operator T&&() const;

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  component_resolvable_impl<Component, T, SeenBindings>::value>>
    operator T();

    template <typename T, typename Tag,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  component_resolvable_impl<Component, annotated<T, Tag>,
                                            SeenBindings>::value>>
    operator annotated<T, Tag>();
};

template <typename DisabledType, typename Component, typename SeenBindings>
struct constructor_argument<
    DisabledType, component_probe_tag<Component, SeenBindings, reference>> {
    using tag_type = component_probe_tag<Component, SeenBindings, reference>;

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  component_resolvable_impl<Component, T*, SeenBindings>::value>>
    operator T*();

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  component_resolvable_impl<Component, T&, SeenBindings>::value>>
    operator T&();

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  component_resolvable_impl<Component, T&&, SeenBindings>::value>>
    operator T&&();

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  component_resolvable_impl<Component, std::unique_ptr<T>,
                                            SeenBindings>::value>>
    operator std::unique_ptr<T>();

    template <typename T, typename Tag,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  component_resolvable_impl<Component, annotated<T, Tag>,
                                            SeenBindings>::value>>
    operator annotated<T, Tag>();
};

template <typename DisabledType, typename Component, typename SeenBindings>
struct constructor_argument<
    DisabledType, component_probe_tag<Component, SeenBindings, value>> {
    using tag_type = component_probe_tag<Component, SeenBindings, value>;

    template <typename T,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  component_resolvable_impl<Component, T, SeenBindings>::value>>
    operator T();

    template <typename T, typename Tag,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>> &&
                  component_resolvable_impl<Component, annotated<T, Tag>,
                                            SeenBindings>::value>>
    operator annotated<T, Tag>();
};

template <typename Component, typename SeenBindings> struct component_context {
    template <typename T, typename Container,
              typename = std::enable_if_t<
                  component_resolvable_impl<Component, T, SeenBindings>::value>>
    T resolve(Container&);
};

struct component_container {};

struct component_no_matching_arguments {};

template <typename Argument, typename Component, typename SeenBindings,
          typename = void>
struct component_candidate_argument {};

template <typename Argument, typename Component, typename SeenBindings>
struct component_candidate_argument<
    Argument, Component, SeenBindings,
    std::enable_if_t<
        component_resolvable_impl<Component, Argument, SeenBindings>::value>> {
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

template <typename Interface, typename Request> struct bind_component_request {
    using type = rebind_type_t<Request, decay_t<typename annotated_traits<Interface>::type>>;
};

template <typename T, typename Tag, typename Request>
struct bind_component_request<annotated<T, Tag>, Request> {
    using type = annotated<rebind_type_t<Request, decay_t<T>>, Tag>;
};

template <typename Request, typename = void>
struct component_const_lvalue_request {
    using type = std::add_lvalue_reference_t<
        std::add_const_t<std::remove_reference_t<Request>>>;
};

template <typename T, typename Tag>
struct component_const_lvalue_request<annotated<T, Tag>> {
    using type = annotated<
        std::add_lvalue_reference_t<std::add_const_t<std::remove_reference_t<T>>>,
        Tag>;
};

template <typename Request>
using component_const_lvalue_request_t =
    typename component_const_lvalue_request<Request>::type;

template <typename TypeList, typename Interface> struct bind_component_request_list;

template <typename... Requests, typename Interface>
struct bind_component_request_list<type_list<Requests...>, Interface> {
    using type =
        type_list<typename bind_component_request<Interface, Requests>::type...>;
};

template <typename TypeList, typename Interface>
using bind_component_request_list_t =
    typename bind_component_request_list<TypeList, Interface>::type;

template <typename TypeList> struct component_const_lvalue_request_list;

template <typename... Requests>
struct component_const_lvalue_request_list<type_list<Requests...>> {
    using type = type_list<component_const_lvalue_request_t<Requests>...>;
};

template <typename TypeList>
using component_const_lvalue_request_list_t =
    typename component_const_lvalue_request_list<TypeList>::type;

template <typename T> struct component_is_unique_ptr_request : std::false_type {};

template <typename T, typename Deleter>
struct component_is_unique_ptr_request<std::unique_ptr<T, Deleter>>
    : std::true_type {};

template <typename T, typename Tag>
struct component_is_unique_ptr_request<annotated<T, Tag>>
    : component_is_unique_ptr_request<T> {};

template <typename Interface, typename Binding, typename DetectionType>
struct component_interface_candidate_requests;

template <typename Interface, typename Binding>
struct component_interface_candidate_requests<Interface, Binding, automatic> {
  private:
    using lvalue_requests = bind_component_request_list_t<
        typename Binding::conversions::lvalue_reference_types, Interface>;
    using const_lvalue_requests =
        component_const_lvalue_request_list_t<lvalue_requests>;
    using rvalue_requests = bind_component_request_list_t<
        typename Binding::conversions::rvalue_reference_types, Interface>;
    using value_requests = bind_component_request_list_t<
        typename Binding::conversions::value_types, Interface>;

  public:
    using type = type_list_cat_t<rvalue_requests, const_lvalue_requests,
                                 lvalue_requests, value_requests>;
};

template <typename Interface, typename Binding>
struct component_interface_candidate_requests<Interface, Binding, reference> {
  private:
    using pointer_requests = bind_component_request_list_t<
        typename Binding::conversions::pointer_types, Interface>;
    using lvalue_requests = bind_component_request_list_t<
        typename Binding::conversions::lvalue_reference_types, Interface>;
    using rvalue_requests = bind_component_request_list_t<
        typename Binding::conversions::rvalue_reference_types, Interface>;
    using unique_requests = type_list_filter_t<
        component_is_unique_ptr_request,
        bind_component_request_list_t<typename Binding::conversions::value_types,
                                      Interface>>;

  public:
    using type = type_list_cat_t<pointer_requests, lvalue_requests,
                                 rvalue_requests, unique_requests>;
};

template <typename Interface, typename Binding>
struct component_interface_candidate_requests<Interface, Binding, value> {
    using type = bind_component_request_list_t<
        typename Binding::conversions::value_types, Interface>;
};

template <typename Binding, typename DetectionType, typename Interfaces>
struct component_binding_candidate_requests;

template <typename Binding, typename DetectionType, typename... Interfaces>
struct component_binding_candidate_requests<Binding, DetectionType,
                                           type_list<Interfaces...>> {
    using type = type_list_cat_t<
        typename component_interface_candidate_requests<Interfaces, Binding,
                                                        DetectionType>::type...>;
};

template <typename Component, typename DetectionType, typename Bindings>
struct component_candidate_arguments_impl;

template <typename Component, typename DetectionType, typename... Bindings>
struct component_candidate_arguments_impl<Component, DetectionType,
                                          type_list<Bindings...>> {
    using type = type_list_unique_t<type_list_cat_t<
        typename component_binding_candidate_requests<
            Bindings, DetectionType, typename Bindings::interface_list>::type...>>;
};

template <typename Component, typename DetectionType>
using component_candidate_arguments_t =
    typename component_candidate_arguments_impl<
        Component, DetectionType, component_bindings_t<Component>>::type;

template <typename T, typename Probe, typename Candidate, typename Component,
          typename SeenBindings, size_t Slot, size_t Arity>
struct component_slot_candidate_constructible {
  private:
    using probe_tuple = tuple_repeat_t<Probe, Arity>;
    using candidate_tuple = typename tuple_replace<
        probe_tuple, Slot,
        component_candidate_argument<Candidate, Component, SeenBindings>>::type;

  public:
    static constexpr bool value =
        detail::is_list_initializable_v<T, candidate_tuple> ||
        detail::is_direct_initializable_v<T, candidate_tuple>;
};

template <typename T, typename CandidateList, typename Probe, typename Component,
          typename SeenBindings, size_t Slot, size_t Arity>
struct component_slot_candidate_arguments;

template <typename T, typename Probe, typename Component, typename SeenBindings,
          size_t Slot, size_t Arity>
struct component_slot_candidate_arguments<T, type_list<>, Probe, Component,
                                          SeenBindings, Slot, Arity> {
    using type = type_list<>;
};

template <typename T, typename Head, typename... Tail, typename Probe,
          typename Component, typename SeenBindings, size_t Slot, size_t Arity>
struct component_slot_candidate_arguments<T, type_list<Head, Tail...>, Probe,
                                          Component, SeenBindings, Slot, Arity> {
  private:
    using tail_type =
        typename component_slot_candidate_arguments<
            T, type_list<Tail...>, Probe, Component, SeenBindings, Slot,
            Arity>::type;

  public:
    using type = std::conditional_t<
        component_slot_candidate_constructible<T, Probe, Head, Component,
                                              SeenBindings, Slot,
                                              Arity>::value,
        type_list_cat_t<type_list<Head>, tail_type>, tail_type>;
};

template <typename T, typename CandidateList, typename Probe, typename Component,
          typename SeenBindings, size_t Slot, size_t Arity>
using component_slot_candidate_arguments_t =
    typename component_slot_candidate_arguments<
        T, CandidateList, Probe, Component, SeenBindings, Slot, Arity>::type;

template <typename T, typename Tuple> struct component_argument_tuple_constructible;

template <typename T, typename... Requests>
struct component_argument_tuple_constructible<T, std::tuple<Requests...>>
    : std::bool_constant<detail::is_list_initializable_v<T, Requests...> ||
                         detail::is_direct_initializable_v<T, Requests...>> {};

template <typename T, typename RemainingSlotLists, typename SelectedTuple>
struct component_argument_tuple_search;

template <typename T, typename SelectedTuple>
struct component_argument_tuple_search<T, type_list<>, SelectedTuple> {
    using type = std::conditional_t<
        component_argument_tuple_constructible<T, SelectedTuple>::value,
        SelectedTuple, component_no_matching_arguments>;
};

template <typename T, typename CandidateList, typename RemainingSlotLists,
          typename SelectedTuple>
struct component_argument_tuple_search_candidates;

template <typename T, typename RemainingSlotLists, typename SelectedTuple>
struct component_argument_tuple_search_candidates<T, type_list<>,
                                                 RemainingSlotLists,
                                                 SelectedTuple> {
    using type = component_no_matching_arguments;
};

template <typename T, typename Head, typename... Tail,
          typename RemainingSlotLists, typename SelectedTuple>
struct component_argument_tuple_search_candidates<T, type_list<Head, Tail...>,
                                                 RemainingSlotLists,
                                                 SelectedTuple> {
  private:
    using selected_type = typename component_argument_tuple_search<
        T, RemainingSlotLists, tuple_append_t<SelectedTuple, Head>>::type;

  public:
    using type = std::conditional_t<
        !std::is_same_v<selected_type, component_no_matching_arguments>,
        selected_type,
        typename component_argument_tuple_search_candidates<
            T, type_list<Tail...>, RemainingSlotLists, SelectedTuple>::type>;
};

template <typename T, typename HeadSlotList, typename... TailSlotLists,
          typename SelectedTuple>
struct component_argument_tuple_search<
    T, type_list<HeadSlotList, TailSlotLists...>, SelectedTuple> {
    using type = typename component_argument_tuple_search_candidates<
        T, HeadSlotList, type_list<TailSlotLists...>, SelectedTuple>::type;
};

template <typename T, typename DetectionType, typename Probe,
          typename CandidateRequests, typename Component, typename SeenBindings,
          typename Seq>
struct component_autodetected_argument_types_impl;

template <typename T, typename DetectionType, typename Probe,
          typename CandidateRequests, typename Component, typename SeenBindings,
          size_t... Slots>
struct component_autodetected_argument_types_impl<
    T, DetectionType, Probe, CandidateRequests, Component, SeenBindings,
    std::index_sequence<Slots...>> {
    // Autodetection only gives us the winning arity. Re-run that arity with
    // component-constrained requests so the checked component and
    // component_container share one concrete dependency tuple.
    using type = typename component_argument_tuple_search<
        T,
        type_list<component_slot_candidate_arguments_t<
            T, CandidateRequests, Probe, Component, SeenBindings, Slots,
            sizeof...(Slots)>...>,
        std::tuple<>>::type;
};

template <typename T, typename DetectionType, typename Component,
          typename SeenBindings, typename = void>
struct component_autodetected_argument_types : std::false_type {
    using type = component_no_matching_arguments;
    static constexpr bool valid = false;
};

template <typename T, typename DetectionType, typename Component,
          typename SeenBindings>
struct component_autodetected_arguments {
  private:
    using probe =
        constructor_argument<T,
                             component_probe_tag<Component, SeenBindings,
                                                 DetectionType>>;
    using resolved_arguments = typename component_autodetected_argument_types_impl<
        T, DetectionType, probe,
        component_candidate_arguments_t<Component, DetectionType>, Component,
        SeenBindings,
        std::make_index_sequence<
            ::dingo::constructor_detection<T, DetectionType>::arity>>::type;

  public:
    using type = resolved_arguments;
};

template <typename T, typename DetectionType, typename Component,
          typename SeenBindings>
struct component_autodetected_argument_types<
    T, DetectionType, Component, SeenBindings,
    std::enable_if_t<::dingo::constructor_detection<T, DetectionType>::valid>>
    : std::bool_constant<!std::is_same_v<
          typename component_autodetected_arguments<T, DetectionType, Component,
                                                    SeenBindings>::type,
          component_no_matching_arguments>> {
    using arguments = component_autodetected_arguments<
        T, DetectionType, Component, SeenBindings>;

  public:
    using type = typename arguments::type;

    static constexpr bool valid =
        !std::is_same_v<type, component_no_matching_arguments>;
};

template <typename Tuple, typename Component, typename SeenBindings>
struct component_argument_tuple_resolvable;

template <typename Component, typename SeenBindings, typename... Requests>
struct component_argument_tuple_resolvable<std::tuple<Requests...>, Component,
                                           SeenBindings>
    : std::conjunction<
          component_resolvable_impl<Component, Requests, SeenBindings>...> {};

template <typename Factory, typename = void>
struct component_factory_argument_types {
    using type = component_no_matching_arguments;
};

template <typename Factory>
struct component_factory_argument_types<
    Factory, std::void_t<typename Factory::direct_argument_types>> {
    using type = typename Factory::direct_argument_types;
};

template <typename Binding, typename Component, typename SeenBindings,
          typename Factory = typename Binding::factory>
struct component_binding_factory_argument_types {
    using type = typename component_factory_argument_types<Factory>::type;
};

template <typename Binding, typename Component, typename SeenBindings,
          typename T, typename DetectionType>
struct component_binding_factory_argument_types<
    Binding, Component, SeenBindings,
    ::dingo::constructor_detection<T, DetectionType>> {
    using type = typename component_autodetected_argument_types<
        T, DetectionType, Component, SeenBindings>::type;
};

template <typename Binding, typename Component,
          typename SeenBindings = type_list_push_unique_t<type_list<>, Binding>>
using component_binding_factory_argument_types_t =
    typename component_binding_factory_argument_types<Binding, Component,
                                                     SeenBindings>::type;

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
          typename SeenBindings>
struct generic_factory_constructible
    : std::bool_constant<
          std::is_default_constructible_v<Factory> &&
          (member_factory_constructible<Factory, StorageType,
                                        component_context<Component, SeenBindings>,
                                        component_container>::value ||
           static_factory_constructible<Factory, StorageType,
                                        component_context<Component, SeenBindings>,
                                        component_container>::value)> {};

template <typename Factory, typename Binding, typename Component,
          typename SeenBindings, typename = void>
struct factory_constructibility_dispatch
    : generic_factory_constructible<Factory, typename Binding::storage,
                                    Component, SeenBindings> {};

template <typename Factory, typename Binding, typename Component,
          typename SeenBindings, typename = void>
struct component_direct_factory_constructible : std::false_type {};

template <typename Factory, typename Binding, typename Component,
          typename SeenBindings>
struct component_direct_factory_constructible<
    Factory, Binding, Component, SeenBindings,
    std::void_t<typename component_factory_argument_types<Factory>::type>>
    : std::bool_constant<
          !std::is_same_v<typename component_factory_argument_types<Factory>::type,
                          component_no_matching_arguments> &&
          component_argument_tuple_resolvable<
              typename component_factory_argument_types<Factory>::type,
              Component, SeenBindings>::value> {};

template <typename Factory, typename Binding, typename Component,
          typename SeenBindings>
struct factory_constructibility_dispatch<
    Factory, Binding, Component, SeenBindings,
    std::enable_if_t<
        !std::is_same_v<typename component_factory_argument_types<Factory>::type,
                        component_no_matching_arguments>>>
    : component_direct_factory_constructible<Factory, Binding, Component,
                                             SeenBindings> {};

template <typename T, typename DetectionType, typename Binding,
          typename Component, typename SeenBindings>
struct factory_constructibility_dispatch<
    ::dingo::constructor_detection<T, DetectionType>, Binding, Component,
    SeenBindings, void>
    : component_autodetected_argument_types<T, DetectionType, Component,
                                            SeenBindings> {};

template <typename Binding, typename Component, typename SeenBindings>
struct component_factory_constructible
    : factory_constructibility_dispatch<typename Binding::factory, Binding,
                                        Component, SeenBindings> {};

template <typename Binding, typename Component, typename SeenBindings>
struct binding_constructible
    : component_factory_constructible<Binding, Component, SeenBindings> {};

template <typename Component, typename Request, typename SeenBindings,
          typename Id>
struct component_resolvable_impl;

template <typename Binding, typename Component, typename SeenBindings,
          bool RequestSupported, bool BindingSeen>
struct component_resolvable_state : std::false_type {};

template <typename Binding, typename Component, typename SeenBindings,
          bool BindingSeen>
struct component_resolvable_state<Binding, Component, SeenBindings, false,
                                  BindingSeen> : std::false_type {};

template <typename Binding, typename Component, typename SeenBindings>
struct component_resolvable_state<Binding, Component, SeenBindings, true, true>
    : std::bool_constant<std::is_same_v<typename Binding::scope, shared_cyclical>> {};

template <typename Binding, typename Component, typename SeenBindings>
struct component_resolvable_state<Binding, Component, SeenBindings, true, false>
    : binding_constructible<Binding, Component,
                            type_list_push_unique_t<SeenBindings, Binding>> {};

template <typename Component, typename Request, typename SeenBindings,
          typename Id, typename Binding>
struct component_resolvable_binding_impl
    : component_resolvable_state<
          Binding, Component, SeenBindings,
          binding_supports_request<Binding, request_runtime_type_t<Request>>::value,
          type_list_contains_v<SeenBindings, Binding>> {};

template <typename Component, typename Request, typename SeenBindings,
          typename Id>
struct component_resolvable_binding_impl<Component, Request, SeenBindings, Id,
                                         component_no_id> : std::false_type {};

template <typename Component, typename Request, typename SeenBindings,
          typename Id>
struct component_resolvable_binding_impl<Component, Request, SeenBindings, Id,
                                         component_ambiguous> : std::false_type {};

template <typename Component, typename Request, typename SeenBindings,
          typename Id>
struct component_resolvable_impl
    : component_resolvable_binding_impl<
          Component, Request, SeenBindings, Id,
          find_component_binding_t<component_bindings_t<Component>, decay_t<Request>,
                                   Id>> {};

template <typename Root> struct component_request_traits {
    using request_type = Root;
    using id_type = component_no_id;
};

template <typename Request, typename Id>
struct component_request_traits<indexed_request<Request, Id>> {
    using request_type = Request;
    using id_type = Id;
};

template <typename Component, typename Root>
struct component_root_constructible
    : component_resolvable_impl<
          Component, typename component_request_traits<Root>::request_type,
          type_list<>, typename component_request_traits<Root>::id_type> {};

template <typename Component, typename... Roots>
struct component_constructible : std::conjunction<component_root_constructible<Component, Roots>...> {};

template <typename Entry> struct component_installer {
    template <typename Container> static void install(Container&) {
        static_assert(dependent_false<Entry>::value,
                      "unsupported component entry");
    }
};

template <typename... Policies>
struct component_installer<registration<Policies...>> {
    template <typename Container> static void install(Container& container) {
        container.template register_type<Policies...>();
    }
};

template <typename Id, typename... Policies>
struct component_installer<indexed_registration<Id, Policies...>> {
    template <typename Container> static void install(Container& container) {
        container.template register_indexed_type<Policies...>(Id::get());
    }
};

template <typename... Entries> struct component_installer<component<Entries...>> {
    template <typename Container> static void install(Container& container) {
        using swallow = int[];
        (void)swallow{0, (component_installer<Entries>::install(container), 0)...};
    }
};

} // namespace detail

template <typename Component, typename Request,
          typename Id = detail::component_no_id>
struct component_resolvable
    : detail::component_resolvable_impl<Component, Request, type_list<>, Id> {};

template <typename Component, typename Request,
          typename Id = detail::component_no_id>
static constexpr bool component_resolvable_v =
    component_resolvable<Component, Request, Id>::value;

template <typename Component, typename... Roots>
struct component_constructible
    : detail::component_constructible<Component, Roots...> {};

template <typename Component, typename... Roots>
static constexpr bool component_constructible_v =
    component_constructible<Component, Roots...>::value;

template <typename Component, typename Container>
void install(Container& container) {
    detail::component_installer<Component>::install(container);
}

} // namespace dingo
