//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/registration/annotated.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/rebind_type.h>
#include <dingo/type/type_list.h>

#include <cstddef>
#include <type_traits>

namespace dingo {
namespace ir {
enum class conversion_family {
    native,
    array,
    alternative,
    handle,
    borrow,
    wrapper
};

enum class collection_aggregation_kind { standard, custom };
enum class lookup_mode { single, indexed, collection };
enum class registration_materialization_kind {
    single_interface,
    shared_factory_data
};
enum class registration_payload_kind {
    default_constructed,
    external_instance,
    factory_payload
};

enum class conversion_kind {
    value_copy_or_move,
    lvalue_reference,
    rvalue_reference,
    pointer,
    copy_or_move_from_lvalue_reference,
    copy_or_move_from_rvalue_reference,
    dereference_pointer_to_value,
    dereference_pointer_to_lvalue_reference,
    dereference_pointer_to_rvalue_reference,
    address_of_lvalue_reference
};

struct value_request {};
struct lvalue_reference_request {};
struct rvalue_reference_request {};
struct pointer_request {};

template <typename RequestType, typename ServiceType, typename LookupType,
          typename RuntimeLookupType, typename RequestKind>
struct request {
    using request_type = RequestType;
    using service_type = ServiceType;
    using lookup_type = LookupType;
    using runtime_lookup_type = RuntimeLookupType;
    using request_kind = RequestKind;
};

template <typename CollectionType, typename ElementRequest,
          collection_aggregation_kind AggregationKind>
struct collection_request {
    using collection_type = CollectionType;
    using element_request_type = ElementRequest;
    using element_lookup_type = typename ElementRequest::lookup_type;
    static constexpr auto aggregation_kind = AggregationKind;
};

template <typename RequestType, lookup_mode Mode, typename KeyType = void>
struct lookup_request {
    using request_type = RequestType;
    using lookup_type = typename RequestType::lookup_type;
    using key_type = KeyType;
    static constexpr auto mode = Mode;
};

template <typename InterfaceType, typename RegisteredStorageType,
          typename IndexKeyType = void>
struct registration_key {
    using interface_type = InterfaceType;
    using registered_storage_type = RegisteredStorageType;
    using index_key_type = IndexKeyType;
    static constexpr bool indexed = !std::is_same_v<IndexKeyType, void>;
};

template <typename InterfaceList, typename ScopeTag,
          typename RegisteredStorageType, typename StoredType,
          typename StorageType, typename FactoryType, typename ConversionsType,
          typename InvocationType, typename IndexKeyType,
          registration_payload_kind PayloadKind,
          registration_materialization_kind MaterializationKind>
struct registration {
    using interface_types = InterfaceList;
    using scope_tag = ScopeTag;
    using registered_storage_type = RegisteredStorageType;
    using stored_type = StoredType;
    using storage_type = StorageType;
    using factory_type = FactoryType;
    using conversions_type = ConversionsType;
    using invocation_type = InvocationType;
    using index_key_type = IndexKeyType;
    template <typename InterfaceType>
    using key = registration_key<InterfaceType, RegisteredStorageType,
                                 IndexKeyType>;
    static constexpr bool indexed = !std::is_same_v<IndexKeyType, void>;
    static constexpr auto payload_kind = PayloadKind;
    static constexpr auto materialization_kind = MaterializationKind;
};

template <typename InterfaceType, typename RegisteredType, typename StoredType,
          typename ScopeTag, typename StorageType, typename FactoryType,
          typename ConversionsType>
struct binding {
    using interface_type = InterfaceType;
    using registered_type = RegisteredType;
    using stored_type = StoredType;
    using scope_tag = ScopeTag;
    using storage_type = StorageType;
    using factory_type = FactoryType;
    using conversions_type = ConversionsType;
};

template <typename StorageTag, typename StorageType, bool Cacheable>
struct acquisition {
    using storage_tag = StorageTag;
    using storage_type = StorageType;
    static constexpr bool cacheable = Cacheable;
};

template <typename FactoryType>
struct factory_invocation {
    using factory_type = FactoryType;
};

template <typename ConstructedType, typename Dependencies>
struct constructor_invocation {
    using constructed_type = ConstructedType;
    using dependencies = Dependencies;
    static constexpr std::size_t arity = type_list_size_v<Dependencies>;
};

template <typename ConstructedType, typename DetectionTag, std::size_t Position>
struct detected_constructor_dependency {
    using constructed_type = ConstructedType;
    using detection_tag = DetectionTag;
    static constexpr std::size_t position = Position;
};

template <typename ConstructedType, typename DetectionTag, typename Dependencies>
struct detected_constructor_invocation {
    using constructed_type = ConstructedType;
    using detection_tag = DetectionTag;
    using dependencies = Dependencies;
    static constexpr std::size_t arity = type_list_size_v<Dependencies>;
};

template <typename ReturnType, typename Dependencies, typename CallableType>
struct function_invocation {
    using return_type = ReturnType;
    using dependencies = Dependencies;
    using callable_type = CallableType;
    static constexpr std::size_t arity = type_list_size_v<Dependencies>;
};

template <typename StorageTag, typename TargetType, typename SourceType,
          typename LookupType, conversion_family Family,
          conversion_kind Kind>
struct conversion {
    using storage_tag = StorageTag;
    using target_type = TargetType;
    using source_type = SourceType;
    using lookup_type = LookupType;
    static constexpr auto family = Family;
    static constexpr auto kind = Kind;
};

struct erased_binding {};
struct erased_acquisition {};
struct erased_conversion {};

template <typename FactoryType>
struct erased_invocation {
    using factory_type = FactoryType;
};

template <typename Request, typename Binding, typename Acquisition,
          typename Invocation, typename Conversion>
struct resolution {
    using request_type = Request;
    using binding_type = Binding;
    using acquisition_type = Acquisition;
    using invocation_type = Invocation;
    using conversion_type = Conversion;
};

template <typename Request>
struct lookup_resolution {
    using request_type = Request;
    using lookup_type = typename Request::lookup_type;
    using key_type = typename Request::key_type;
    static constexpr auto mode = Request::mode;
};

template <typename Request> struct collection_resolution {
    using request_type = Request;
    using collection_type = typename Request::collection_type;
    using element_request_type = typename Request::element_request_type;
    using element_lookup_type = typename Request::element_lookup_type;
    static constexpr auto aggregation_kind = Request::aggregation_kind;
};
} // namespace ir

namespace detail {
template <typename T> struct request_kind_ir {
    using type = ir::value_request;
};

template <typename T> struct request_kind_ir<T&> {
    using type = ir::lvalue_reference_request;
};

template <typename T> struct request_kind_ir<const T&> {
    using type = ir::lvalue_reference_request;
};

template <typename T> struct request_kind_ir<T&&> {
    using type = ir::rvalue_reference_request;
};

template <typename T> struct request_kind_ir<T*> {
    using type = ir::pointer_request;
};

template <typename RequestType,
          typename ServiceType = typename annotated_traits<RequestType>::type>
struct request_ir {
    using type = ir::request<RequestType, ServiceType,
                             normalized_type_t<RequestType>,
                             rebind_leaf_t<ServiceType, runtime_type>,
                             typename request_kind_ir<ServiceType>::type>;
};

template <typename RequestType>
using request_ir_t = typename request_ir<RequestType>::type;

template <typename CollectionType, typename ElementRequest,
          ir::collection_aggregation_kind AggregationKind>
using collection_request_ir_t =
    ir::collection_request<CollectionType, ElementRequest, AggregationKind>;

template <typename CollectionType, typename ElementRequest,
          ir::collection_aggregation_kind AggregationKind>
using collection_plan_ir_t = ir::collection_resolution<
    collection_request_ir_t<CollectionType, ElementRequest, AggregationKind>>;

template <typename RequestType>
using single_lookup_plan_ir_t = ir::lookup_resolution<
    ir::lookup_request<request_ir_t<RequestType>, ir::lookup_mode::single>>;

template <typename RequestType, typename KeyType>
using indexed_lookup_plan_ir_t = ir::lookup_resolution<ir::lookup_request<
    request_ir_t<RequestType>, ir::lookup_mode::indexed, KeyType>>;

template <typename InterfaceType, typename StorageType>
struct binding_ir {
    using type = ir::binding<
        InterfaceType, typename StorageType::type, typename StorageType::stored_type,
        typename StorageType::tag_type, StorageType,
        typename StorageType::factory_type, typename StorageType::conversions>;
};

template <typename InterfaceType, typename StorageType>
using binding_ir_t = typename binding_ir<InterfaceType, StorageType>::type;

template <typename StorageType>
using acquisition_ir_t =
    ir::acquisition<typename StorageType::tag_type, StorageType,
                    StorageType::cacheable>;

template <typename RequestKind> struct request_conversion_kind_ir;

template <> struct request_conversion_kind_ir<ir::value_request> {
    static constexpr auto value = ir::conversion_kind::value_copy_or_move;
};

template <> struct request_conversion_kind_ir<ir::lvalue_reference_request> {
    static constexpr auto value = ir::conversion_kind::lvalue_reference;
};

template <> struct request_conversion_kind_ir<ir::rvalue_reference_request> {
    static constexpr auto value = ir::conversion_kind::rvalue_reference;
};

template <> struct request_conversion_kind_ir<ir::pointer_request> {
    static constexpr auto value = ir::conversion_kind::pointer;
};

template <typename StorageType, typename TargetType>
struct conversion_family_ir {
  private:
    using storage_value_type =
        std::remove_cv_t<std::remove_reference_t<typename StorageType::type>>;

  public:
    static constexpr auto value =
        std::is_array_v<typename StorageType::type>
            ? ir::conversion_family::array
            : (is_alternative_type_v<storage_value_type>
                   ? ir::conversion_family::alternative
                   : (type_traits<storage_value_type>::enabled
                          ? (is_pointer_like_type_v<TargetType>
                                 ? ir::conversion_family::handle
                                 : (type_traits<storage_value_type>::is_value_borrowable
                                        ? ir::conversion_family::borrow
                                        : ir::conversion_family::wrapper))
                          : ir::conversion_family::native));
};

template <typename RequestKind, typename SourceType>
struct source_access_conversion_kind_ir {
    static constexpr auto value = request_conversion_kind_ir<RequestKind>::value;
};

template <typename SourceType>
struct source_access_conversion_kind_ir<ir::value_request, SourceType> {
    static constexpr auto value =
        std::is_pointer_v<SourceType>
            ? ir::conversion_kind::dereference_pointer_to_value
            : (std::is_lvalue_reference_v<SourceType>
                   ? ir::conversion_kind::copy_or_move_from_lvalue_reference
                   : (std::is_rvalue_reference_v<SourceType>
                          ? ir::conversion_kind::
                                copy_or_move_from_rvalue_reference
                          : ir::conversion_kind::value_copy_or_move));
};

template <typename SourceType>
struct source_access_conversion_kind_ir<ir::lvalue_reference_request, SourceType> {
    static constexpr auto value =
        std::is_pointer_v<SourceType>
            ? ir::conversion_kind::dereference_pointer_to_lvalue_reference
            : ir::conversion_kind::lvalue_reference;
};

template <typename SourceType>
struct source_access_conversion_kind_ir<ir::rvalue_reference_request, SourceType> {
    static constexpr auto value =
        std::is_pointer_v<SourceType>
            ? ir::conversion_kind::dereference_pointer_to_rvalue_reference
            : ir::conversion_kind::rvalue_reference;
};

template <typename SourceType>
struct source_access_conversion_kind_ir<ir::pointer_request, SourceType> {
    static constexpr auto value =
        std::is_lvalue_reference_v<SourceType>
            ? ir::conversion_kind::address_of_lvalue_reference
            : ir::conversion_kind::pointer;
};

template <typename RequestKind, typename StorageType, typename SourceType,
          typename TargetType>
struct conversion_kind_ir {
  private:
    static constexpr auto family =
        conversion_family_ir<StorageType, TargetType>::value;

  public:
    static constexpr auto value =
        family == ir::conversion_family::native
            ? source_access_conversion_kind_ir<RequestKind, SourceType>::value
            : request_conversion_kind_ir<RequestKind>::value;
};

template <typename RequestKind, typename Conversions>
struct request_conversion_lookups;

template <typename Conversions>
struct request_conversion_lookups<ir::value_request, Conversions> {
    using type = typename Conversions::value_types;
};

template <typename Conversions>
struct request_conversion_lookups<ir::lvalue_reference_request, Conversions> {
    using type = typename Conversions::lvalue_reference_types;
};

template <typename Conversions>
struct request_conversion_lookups<ir::rvalue_reference_request, Conversions> {
    using type = typename Conversions::rvalue_reference_types;
};

template <typename Conversions>
struct request_conversion_lookups<ir::pointer_request, Conversions> {
    using type = typename Conversions::pointer_types;
};

template <typename RequestKind, typename Conversions>
using request_conversion_lookups_t =
    typename request_conversion_lookups<RequestKind, Conversions>::type;

template <typename RuntimeLookupType, typename Lookups>
struct select_conversion_lookup;

template <typename RuntimeLookupType>
struct select_conversion_lookup<RuntimeLookupType, type_list<>> {
    using type = void;
};

template <typename RuntimeLookupType, typename Head, typename... Tail>
struct select_conversion_lookup<RuntimeLookupType, type_list<Head, Tail...>> {
    using type = std::conditional_t<
        std::is_same_v<lookup_type_t<Head>, RuntimeLookupType>, Head,
        typename select_conversion_lookup<RuntimeLookupType,
                                          type_list<Tail...>>::type>;
};

template <typename RequestKind, typename RuntimeLookupType, typename Conversions>
using selected_conversion_lookup_t =
    typename select_conversion_lookup<
        RuntimeLookupType,
        request_conversion_lookups_t<RequestKind, Conversions>>::type;

template <typename RequestKind, typename InterfaceType, typename StorageType,
          typename SourceType, typename LookupType>
struct selected_conversion_ir {
    using interface_service_type = typename annotated_traits<InterfaceType>::type;
    using target_type =
        std::remove_reference_t<resolved_type_t<LookupType, interface_service_type>>;
    static constexpr auto family =
        conversion_family_ir<StorageType, target_type>::value;
    static constexpr auto kind =
        conversion_kind_ir<RequestKind, StorageType, SourceType,
                           target_type>::value;
    using type = ir::conversion<typename StorageType::tag_type, target_type,
                                SourceType, LookupType, family, kind>;
};

template <typename Request, typename InterfaceType, typename StorageType,
          typename SourceType, typename LookupType>
using selected_conversion_ir_t =
    typename selected_conversion_ir<typename Request::request_kind,
                                   InterfaceType, StorageType, SourceType,
                                   LookupType>::type;

template <typename RequestKind, typename InterfaceType, typename StorageType,
          typename SourceType, typename LookupType>
using selected_conversion_for_kind_t =
    typename selected_conversion_ir<RequestKind, InterfaceType, StorageType,
                                    SourceType, LookupType>::type;

template <typename Request, typename InterfaceType, typename StorageType,
          typename SourceType, typename Conversions, typename SelectedLookup>
struct selected_request_conversion_ir {
    using type = selected_conversion_ir_t<Request, InterfaceType, StorageType,
                                          SourceType, SelectedLookup>;
};

template <typename Request, typename InterfaceType, typename StorageType,
          typename SourceType, typename Conversions>
struct selected_request_conversion_ir<Request, InterfaceType, StorageType,
                                      SourceType, Conversions, void> {
    using type = ir::erased_conversion;
};

template <typename Request, typename InterfaceType, typename StorageType,
          typename SourceType, typename Conversions>
using selected_request_conversion_ir_t =
    typename selected_request_conversion_ir<
        Request, InterfaceType, StorageType, SourceType, Conversions,
        selected_conversion_lookup_t<typename Request::request_kind,
                                     typename Request::runtime_lookup_type,
                                     Conversions>>::type;

template <typename FactoryType, typename = void>
struct factory_invocation_ir {
    using type = ir::factory_invocation<FactoryType>;
};

template <typename FactoryType>
using factory_invocation_ir_t = typename factory_invocation_ir<FactoryType>::type;
} // namespace detail
} // namespace dingo
