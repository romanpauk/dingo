//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/memory/allocator.h>
#include <dingo/registration/annotated.h>
#include <dingo/resolution/instance_factory.h>
#include <dingo/registration/collection_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/exceptions.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/invoke.h>
#include <dingo/resolution/interface_storage_traits.h>
#include <dingo/index.h>
#include <dingo/index/array.h>
#include <dingo/resolution/resolving_context.h>
#include <dingo/resolution/runtime_execution_plan.h>
#include <dingo/resolution/runtime_plan_format.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/memory/static_allocator.h>
#include <dingo/resolution/type_cache.h>
#include <dingo/type/type_map.h>
#include <dingo/registration/type_registration.h>

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <typeindex>
#include <utility>
#include <variant>

namespace dingo {

struct none_t {};
template <typename T> struct is_none : std::bool_constant<false> {};
template <> struct is_none<none_t> : std::bool_constant<true> {};
template <typename T> static constexpr auto is_none_v = is_none<T>::value;

template <typename T> struct is_auto_constructible : std::is_aggregate<T> {};

namespace detail {
template <typename Tag> struct index_array_traits {
    static constexpr bool is_array = false;
};

template <std::size_t N> struct index_array_traits<index_type::array<N>> {
    static constexpr bool is_array = true;
    static constexpr std::size_t value = N;
};
} // namespace detail
} // namespace dingo

#include <dingo/materialization/registration_builder.h>

namespace dingo {

struct dynamic_container_traits {
    template <typename> using rebind_t = dynamic_container_traits;

    using tag_type = none_t;
    using rtti_type = rtti<typeid_provider>;
    template <typename Value, typename Allocator>
    using type_map_type = dynamic_type_map<Value, rtti_type, Allocator>;
    template <typename Value, typename Allocator>
    using type_cache_type = dynamic_type_cache<Value, rtti_type, Allocator>;
    using allocator_type = std::allocator<char>;
    using index_definition_type = std::tuple<>;
    static constexpr bool cache_enabled = true;
};

template <typename Tag = void> struct static_container_traits {
    template <typename TagT> using rebind_t = static_container_traits<TagT>;

    using tag_type = Tag;
    using rtti_type = rtti<static_provider>;
    template <typename Value, typename Allocator>
    using type_map_type = static_type_map<Value, Tag, Allocator>;
    template <typename Value, typename Allocator>
    using type_cache_type = static_type_cache<void*, Tag, Allocator>;
    using allocator_type = static_allocator<char, Tag>;
    using index_definition_type = std::tuple<>;
    static constexpr bool cache_enabled = true;
};

// TODO: could this use is_none_v?
template <typename Traits>
static constexpr bool is_tagged_container_v =
    !std::is_same_v<typename Traits::template rebind_t<
                        type_list<typename Traits::tag_type, void>>,
                    Traits>;

// TODO wrt. multi-bindings, is this like map/multimap?
template <typename ContainerTraits = dynamic_container_traits,
          typename Allocator = typename ContainerTraits::allocator_type,
          typename ParentContainer = void>
class container : public allocator_base<Allocator> {
    friend class resolving_context;
    template <typename> friend struct detail::registration_builder;
    template <typename ContainerTraitsT, typename AllocatorT,
              typename ParentContainerT>
    friend class container;

    template <typename ContainerTraitsT, typename AllocatorT,
              typename ParentContainerT>
    using rebind_t = container<ContainerTraitsT, AllocatorT, ParentContainerT>;
    using container_type =
        container<ContainerTraits, Allocator, ParentContainer>;
    using parent_container_type =
        std::conditional_t<std::is_same_v<void, ParentContainer>,
                           container_type, ParentContainer>;

    static constexpr bool cache_enabled = ContainerTraits::cache_enabled;

  public:
    using container_traits_type = ContainerTraits;
    using allocator_type = Allocator;
    using rtti_type = typename ContainerTraits::rtti_type;
    using index_definition_type =
        typename ContainerTraits::index_definition_type;

    template <typename Tag>
    using child_container_type =
        container<typename container_traits_type::template rebind_t<
                      type_list<typename container_traits_type::tag_type, Tag>>,
                  Allocator, container_type>;

    container()
        : allocator_base<allocator_type>(allocator_type()),
          type_factories_(get_allocator()), type_cache_(get_allocator()) {}

    container(allocator_type alloc)
        : allocator_base<allocator_type>(alloc),
          type_factories_(get_allocator()), type_cache_(get_allocator()) {}

    container(parent_container_type* parent,
              allocator_type alloc = allocator_type())
        : allocator_base<allocator_type>(alloc), parent_(parent),
          type_factories_(get_allocator()), type_cache_(get_allocator()) {

        static_assert(
            !is_tagged_container_v<container_traits_type> ||
                !std::is_same_v<typename container_traits_type::tag_type,
                                typename parent_container_type::
                                    container_traits_type::tag_type>,
            "static typemap based containers require parent and child "
            "container tags to be different");
    }

    allocator_type& get_allocator() {
        return allocator_base<allocator_type>::get_allocator();
    }

    // TODO: how to do this better?
    template <typename... TypeArgs> auto& register_type() {
        return register_type_impl<TypeArgs...>(none_t(), none_t());
    }

    template <typename... TypeArgs, typename Arg>
    auto& register_type(Arg&& arg) {
        return register_type_impl<TypeArgs...>(std::forward<Arg>(arg),
                                               none_t());
    }

    template <typename... TypeArgs, typename IdType>
    auto& register_indexed_type(IdType&& id) {
        return register_type_impl<TypeArgs...>(none_t(),
                                               std::forward<IdType>(id));
    }

    template <typename... TypeArgs, typename Arg, typename IdType>
    auto& register_indexed_type(Arg&& arg, IdType&& id) {
        return register_type_impl<TypeArgs...>(std::forward<Arg>(arg),
                                               std::forward<IdType>(id));
    }

    template <typename... TypeArgs, typename Fn>
    auto& register_type_collection(Fn&& fn) {
        using registration = type_registration<TypeArgs...>;
        return register_type<TypeArgs...>(callable([&] {
            return construct_collection<
                typename registration::storage_type::type>(fn);
        }));
    }

    template <typename... TypeArgs> auto& register_type_collection() {
        return register_type_collection<TypeArgs...>(
            default_collection_aggregator{});
    }

    template <typename T, typename IdType = none_t,
              typename R = typename annotated_traits<
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>>::type>
    R resolve(IdType&& id = IdType()) {
        // TODO: a crude way to check cache before context gets placed on the
        // stack
        if constexpr (cache_enabled) {
            if constexpr (is_none_v<std::decay_t<IdType>>) {
                void* cache = type_cache_.template get<T>();
                if (cache) {
                    return convert_cached_result<T>(cache);
                }
            } else {
                auto data = type_factories_.template get<normalized_type_t<T>>();
                if (data) {
                    using key_type = std::decay_t<IdType>;
                    auto* index = data->template try_get_index<key_type>();
                    auto indexed = index ? index->find(id) : nullptr;

                    if (indexed) {
                        if (indexed->cache) {
                            return convert_cached_result<T>(indexed->cache);
                        }
                    }
                }
            }
        }

        // TODO: this destructor is really slowing things down
        resolving_context context;
        return resolve<T, true, false>(context, std::forward<IdType>(id));
    }

    template <typename T,
              typename Factory = constructor_detection<normalized_type_t<T>>>
    T construct(Factory factory = Factory()) {
        // TODO: nothrow constructuble
        resolving_context context;
        return factory.template construct<T>(context, *this);
    }

    template <typename T> T construct_collection() {
        return construct_collection_plan<T, ir::collection_aggregation_kind::standard>(
            default_collection_aggregator{});
    }

    template <typename T, typename Fn> T construct_collection(Fn&& fn) {
        return construct_collection_plan<T, ir::collection_aggregation_kind::custom>(
            std::forward<Fn>(fn));
    }

  private:
    struct default_collection_aggregator {
        template <typename Collection, typename Value>
        void operator()(Collection& collection, Value&& value) const {
            collection_traits<std::decay_t<decltype(collection)>>::add(
                collection, std::move(value));
        }
    };

    template <typename T, ir::collection_aggregation_kind AggregationKind,
              typename Fn>
    T construct_collection_plan(Fn&& fn) {
        using collection_type = collection_traits<T>;
        using resolve_type = typename collection_type::resolve_type;
        using plan_type = detail::collection_plan_ir_t<
            T, detail::request_ir_t<resolve_type>, AggregationKind>;
        using lookup_type = typename plan_type::element_lookup_type;
        constexpr auto collection_plan =
            detail::make_collection_plan_summary<plan_type>();

        static_assert(collection_type::is_collection,
                      "missing collection_traits specialization for type T");

        T results;
        resolving_context context;
        auto data = type_factories_.template get<lookup_type>();
        if (!data)
            throw_collection_type_not_found_with_plan(
                collection_plan.collection_type,
                collection_plan.element_request_type, collection_plan);

        collection_type::reserve(results, data->entries.size());
        for (auto&& p : data->entries) {
            try {
                fn(results, resolve_collection_type<resolve_type>(p.second.binding,
                                                                  context));
            } catch (const type_not_found_exception& e) {
                throw_collection_exception_with_plan<type_not_found_exception>(
                    e, collection_plan, p.second.binding);
            } catch (const type_not_convertible_exception& e) {
                throw_collection_exception_with_plan<
                    type_not_convertible_exception>(e, collection_plan,
                                                    p.second.binding);
            } catch (const type_ambiguous_exception& e) {
                throw_collection_exception_with_plan<type_ambiguous_exception>(
                    e, collection_plan, p.second.binding);
            } catch (const type_recursion_exception& e) {
                throw_collection_exception_with_plan<type_recursion_exception>(
                    e, collection_plan, p.second.binding);
            } catch (const type_not_constructed_exception& e) {
                throw_collection_exception_with_plan<
                    type_not_constructed_exception>(e, collection_plan,
                                                    p.second.binding);
            }
        }
        return results;
    }

  public:
    template< typename Callable > auto invoke(Callable&& callable) {
        resolving_context context;
        return ::dingo::invoke< std::remove_reference_t<Callable> >::construct(
            context, *this, std::forward<Callable>(callable));
    }

  private:
    template <typename... TypeArgs, typename Arg, typename IdType>
    auto& register_type_impl(Arg&& arg, IdType&& id) {
        using registration =
            std::conditional_t<!is_none_v<std::decay_t<Arg>>,
                               type_registration<TypeArgs..., factory<Arg>>,
                               type_registration<TypeArgs...>>;
        using payload_type =
            std::conditional_t<is_none_v<std::decay_t<Arg>>, void,
                               std::decay_t<Arg>>;
        using index_key_type =
            std::conditional_t<is_none_v<std::decay_t<IdType>>, void,
                               std::decay_t<IdType>>;
        using registration_ir =
            detail::registration_plan_t<registration, payload_type,
                                        index_key_type>;
        auto descriptor =
            detail::registration_builder<container_type>::template build<
                registration_ir>(
                    *this, std::forward<Arg>(arg));
        auto* result_container = descriptor.container;
        insert_materialized_registration(std::move(descriptor),
                                         std::forward<IdType>(id));
        return *result_container;
    }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
    template <typename T, bool RemoveRvalueReferences, bool CheckCache = true,
              typename IdType = none_t,
              typename R = std::conditional_t<
                  RemoveRvalueReferences,
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>,
                  T>>
    R resolve(resolving_context& context, IdType&& id = IdType()) {
        using Type = normalized_type_t<T>;
        static_assert(!std::is_const_v<Type>);

        if constexpr (cache_enabled && CheckCache) {
            void* cache = type_cache_.template get<T>();
            if (cache) {
                return convert_cached_result<T>(cache);
            }
        }

        auto data = type_factories_.template get<Type>();
        if (data) {
            if constexpr (is_none_v<std::decay_t<IdType>>) {
                if (data->entries.size() == 1) {
                    auto& binding = data->entries.front().binding;
                    return resolve<T, typename annotated_traits<T>::type>(
                        binding, context);
                } else {
                    throw_type_ambiguous_with_candidates(
                        describe_type<T>(),
                        detail::make_lookup_plan_summary<
                            detail::single_lookup_plan_ir_t<T>>(),
                        data->entries);
                }
            } else {
                using key_type = std::decay_t<IdType>;
                auto* index = data->template try_get_index<key_type>();
                auto indexed = index ? index->find(id) : nullptr;

                if (indexed) {
                    if constexpr (cache_enabled && CheckCache) {
                        if (indexed->cache) {
                            return convert_cached_result<T>(indexed->cache);
                        }
                    }

                    return resolve<T, typename annotated_traits<T>::type>(
                        *indexed->binding, context, *indexed);
                }
            }
        } else if constexpr (!std::is_same_v<void*, decltype(parent_)>) {
            if (parent_) {
                return parent_->template resolve<T, RemoveRvalueReferences>(
                    context, std::forward<IdType>(id));
            }
        }

        // If we are trying to construct T and it is not wrapped in any way
        // and it is marked as auto-constructible. Aggregates are enabled by
        // default, while user types can opt in by specializing the trait.
        if constexpr (std::is_same_v< Type, std::decay_t<T> > &&
            is_auto_constructible< std::decay_t<T> >::value
        ) {
            // And it is constructible
            using type_detection = detail::automatic;
            using type_constructor = detail::constructor_detection< Type, type_detection, detail::list_initialization, false >;
            if constexpr(type_constructor::valid) {
                // Construct temporary through context so it can be referenced
                return context.template construct_temporary< typename annotated_traits<T>::type, type_detection >(*this);
            }
        }

        if constexpr (is_none_v<std::decay_t<IdType>>) {
            throw_type_not_found_with_lookup_plan(
                describe_type<T>(),
                detail::make_lookup_plan_summary<
                    detail::single_lookup_plan_ir_t<T>>());
        } else {
            if (data) {
                throw_type_not_found_with_candidates(
                    describe_type<T>(), describe_type<std::decay_t<IdType>>(),
                    detail::index_value_to_string(id),
                    detail::make_lookup_plan_summary<
                        detail::indexed_lookup_plan_ir_t<
                            T, std::decay_t<IdType>>>(),
                    data->entries);
            }
            throw_type_not_found_with_lookup_plan(
                describe_type<T>(), describe_type<std::decay_t<IdType>>(),
                detail::index_value_to_string(id),
                detail::make_lookup_plan_summary<
                    detail::indexed_lookup_plan_ir_t<
                        T, std::decay_t<IdType>>>());
        }
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    template <typename T>
    using erased_request_plan_t = detail::resolution_plan_for_t<
        typename annotated_traits<T>::type,
        instance_factory_interface<container_type>>;

    template <typename T>
    T convert_cached_result(void* ptr) {
        using plan_type = erased_request_plan_t<T>;
        return detail::convert_runtime_plan<rtti_type>(
            ptr, detail::lower_runtime_conversion_kind<rtti_type>(plan_type{}),
            plan_type{});
    }

    template <typename RuntimePlan>
    [[noreturn]] void throw_type_not_convertible_with_plan(
        const type_not_convertible_exception& e,
        const RuntimePlan& runtime_plan) {
        std::string message = e.what();
        message += "\nresolution plan: ";
        detail::append_execution_plan(message, runtime_plan);
        throw type_not_convertible_exception(std::move(message));
    }

    template <typename Entries>
    [[noreturn]] void throw_type_ambiguous_with_candidates(
        type_descriptor request_type,
        const detail::lookup_plan_summary& lookup_plan, Entries& entries) {
        std::string message = "type resolution is ambiguous: ";
        append_type_name(message, request_type);
        message += "\nlookup plan: ";
        detail::append_lookup_plan(message, lookup_plan);
        append_registration_binding_candidates(message, entries);
        append_registration_candidates(message, entries);
        throw type_ambiguous_exception(std::move(message));
    }

    template <typename Entries>
    [[noreturn]] void throw_type_not_found_with_candidates(
        type_descriptor request_type, type_descriptor index_key_type,
        const std::string& index_value,
        const detail::lookup_plan_summary& lookup_plan, Entries& entries) {
        std::string message = "type not found: ";
        detail::append_text(message, request_type, " (index type: ",
                            index_key_type, ", index value ", index_value, ")");
        message += "\nlookup plan: ";
        detail::append_lookup_plan(message, lookup_plan);
        append_registration_binding_candidates(message, entries);
        append_registration_candidates(message, entries);
        throw type_not_found_exception(std::move(message));
    }

    [[noreturn]] void throw_type_not_found_with_lookup_plan(
        type_descriptor request_type,
        const detail::lookup_plan_summary& lookup_plan) {
        std::string message = "type not found: ";
        append_type_name(message, request_type);
        message += "\nlookup plan: ";
        detail::append_lookup_plan(message, lookup_plan);
        throw type_not_found_exception(std::move(message));
    }

    [[noreturn]] void throw_type_not_found_with_lookup_plan(
        type_descriptor request_type, type_descriptor index_key_type,
        const std::string& index_value,
        const detail::lookup_plan_summary& lookup_plan) {
        std::string message = "type not found: ";
        detail::append_text(message, request_type, " (index type: ",
                            index_key_type, ", index value ", index_value, ")");
        message += "\nlookup plan: ";
        detail::append_lookup_plan(message, lookup_plan);
        throw type_not_found_exception(std::move(message));
    }

    [[noreturn]] void throw_collection_type_not_found_with_plan(
        type_descriptor collection_type, type_descriptor element_request_type,
        const detail::collection_plan_summary& collection_plan) {
        std::string message = "type not found for collection ";
        detail::append_text(message, collection_type, " (element type: ",
                            element_request_type, ")");
        message += "\ncollection plan: ";
        detail::append_collection_plan(message, collection_plan);
        throw type_not_found_exception(std::move(message));
    }

    template <typename Exception, typename Binding>
    [[noreturn]] void throw_collection_exception_with_plan(
        const Exception& e,
        const detail::collection_plan_summary& collection_plan,
        const Binding& binding) {
        std::string message = e.what();
        if (message.find("\ncollection plan: ") == std::string::npos) {
            message += "\ncollection plan: ";
            detail::append_collection_plan(message, collection_plan);
        }
        if (message.find("\ncollection binding: ") == std::string::npos) {
            message += "\ncollection binding: ";
            detail::append_runtime_binding_summary(message, binding);
        }
        throw Exception(std::move(message));
    }

    struct index_data;

    template <typename CachedT, typename Binding>
    void cache_resolved_result(Binding& binding, void* ptr,
                               index_data* indexed_cache) {
        if constexpr (cache_enabled) {
            if (!binding.cacheable) {
                return;
            }

            if (indexed_cache) {
                indexed_cache->cache = ptr;
            } else if constexpr (!is_none_v<CachedT>) {
                type_cache_.template insert<CachedT>(ptr);
            }
        } else {
            (void)binding;
            (void)ptr;
            (void)indexed_cache;
        }
    }

    template <typename CachedT, typename T, typename Binding, typename Context>
    T execute_resolve_plan(Binding& binding, Context& context,
                           index_data* indexed_cache) {
        using factory_type = std::remove_pointer_t<decltype(binding.factory)>;
        using plan_type = detail::resolution_plan_for_t<T, factory_type>;
        auto runtime_plan =
            detail::lower_runtime_plan<rtti_type>(plan_type{}, binding);
        try {
            void* ptr = detail::execute_runtime_plan(context, runtime_plan);
            cache_resolved_result<CachedT>(binding, ptr, indexed_cache);
            return detail::convert_runtime_plan<rtti_type>(ptr, runtime_plan,
                                                           plan_type{});
        } catch (const type_not_convertible_exception& e) {
            throw_type_not_convertible_with_plan(e, runtime_plan);
        }
    }

    template <typename CachedT, typename T, typename Binding, typename Context>
    T resolve(Binding& binding, Context& context) {
        return execute_resolve_plan<CachedT, T>(binding, context, nullptr);
    }

    template <typename CachedT, typename T, typename Binding, typename Context>
    T resolve(Binding& binding, Context& context, index_data& data) {
        return execute_resolve_plan<CachedT, T>(binding, context, &data);
    }

    template <typename T, typename Binding, typename Context>
    T resolve_collection_type(Binding& binding, Context& context) {
        return execute_resolve_plan<none_t, T>(binding, context, nullptr);
    }

    template <typename Descriptor, typename IdType>
    void insert_materialized_registration(Descriptor&& descriptor, IdType&& id) {
        validate_materialized_registration(descriptor, id);
        auto&& index_id = id;
        insert_materialized_registration_entries(
            std::move(descriptor.entries), index_id,
            typename std::remove_reference_t<Descriptor>::interface_types{},
            type_list<typename std::remove_reference_t<
                Descriptor>::registered_storage_type>{},
            std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<
                decltype(descriptor.entries)>>>{});
    }

    template <typename InterfaceType, typename RegisteredStorageType,
              typename Entry, typename IdType>
    void insert_materialized_entry(Entry&& entry, IdType&& id) {
        using interface_type = InterfaceType;
        using registered_storage_type = RegisteredStorageType;
        using key_type = std::decay_t<IdType>;
        auto binding_id = next_binding_id_++;
        entry.binding.id = binding_id;
        entry.registration.binding_id = binding_id;
        auto pb = type_factories_.template insert<interface_type>(get_allocator());
        auto& data = pb.first;
        auto registration = entry.registration;
        auto inserted =
            data.entries
                .template insert<type_list<interface_type, registered_storage_type>>(
                    std::move(entry));
        if (!inserted.second) {
            throw_type_already_registered(registration);
        }

        if constexpr (!is_none_v<key_type>) {
            key_type index_key(std::forward<IdType>(id));
            if (!data.template get_index<key_type>(get_allocator())
                     .emplace(std::move(index_key),
                              index_data{&inserted.first.binding, nullptr})) {
                bool erased = data.entries.template erase<
                    type_list<interface_type, registered_storage_type>>();
                assert(erased);
                (void)erased;
                throw_type_index_already_registered(registration);
            }
        }
    }

    template <typename Descriptor, typename IdType>
    void validate_materialized_registration(Descriptor& descriptor,
                                            const IdType& id) {
        constexpr auto entry_count =
            std::tuple_size_v<std::remove_reference_t<decltype(descriptor.entries)>>;
        auto validation_entries = std::apply(
            [](auto&... entries) {
                return std::array<const detail::registration_metadata*,
                                  sizeof...(entries)>{
                    &entries.registration...};
            },
            descriptor.entries);

        validate_materialized_registration_entries(
            descriptor.entries, id, typename Descriptor::interface_types{},
            type_list<typename Descriptor::registered_storage_type>{},
            std::make_index_sequence<entry_count>{});
        for (std::size_t i = 0; i < entry_count; ++i) {
            for (std::size_t j = i + 1; j < entry_count; ++j) {
                auto& left = *validation_entries[i];
                auto& right = *validation_entries[j];

                if (left.interface_type == right.interface_type &&
                    left.registered_storage_type ==
                        right.registered_storage_type) {
                    throw_type_already_registered(left);
                }

                if constexpr (!is_none_v<std::decay_t<IdType>>) {
                    if (left.interface_type == right.interface_type) {
                        throw_type_index_already_registered(left);
                    }
                }
            }
        }
    }

    template <typename KeyType>
    void validate_registration_index_key(const KeyType& id) {
        using index_tag_type =
            typename index_tag<KeyType, index_definition_type>::type;
        if constexpr (detail::index_array_traits<index_tag_type>::is_array) {
            constexpr std::size_t bound =
                detail::index_array_traits<index_tag_type>::value;
            if (id >= bound) {
                throw detail::make_type_index_out_of_range_exception(id, bound);
            }
        }
    }

    template <typename InterfaceType, typename RegisteredStorageType,
              typename Entry, typename IdType>
    void validate_materialized_entry(const Entry& entry, const IdType& id) {
        using interface_type = InterfaceType;
        using registered_storage_type = RegisteredStorageType;
        using key_type = std::decay_t<IdType>;

        auto data = type_factories_.template get<interface_type>();
        if (data && data->entries.template get<
                        type_list<interface_type, registered_storage_type>>()) {
            throw_type_already_registered(entry.registration);
        }

        if constexpr (!is_none_v<key_type>) {
            validate_registration_index_key<key_type>(id);
            if (data) {
                auto* index = data->template try_get_index<key_type>();
                if (index && index->find(id)) {
                    throw_type_index_already_registered(entry.registration);
                }
            }
        }
    }

    template <typename... Entries, typename IdType, typename... Interfaces,
              typename RegisteredStorageType, std::size_t... I>
    void insert_materialized_registration_entries(
        std::tuple<Entries...>&& entries, IdType&& id,
        type_list<Interfaces...>, type_list<RegisteredStorageType>,
        std::index_sequence<I...>) {
        (insert_materialized_entry<type_list_element_t<I, type_list<Interfaces...>>,
                                   RegisteredStorageType>(
             std::get<I>(std::move(entries)), id),
         ...);
    }

    template <typename Tuple, typename IdType, typename... Interfaces,
              typename RegisteredStorageType, std::size_t... I>
    void validate_materialized_registration_entries(
        Tuple& entries, const IdType& id, type_list<Interfaces...>,
        type_list<RegisteredStorageType>, std::index_sequence<I...>) {
        (validate_materialized_entry<
             type_list_element_t<I, type_list<Interfaces...>>,
             RegisteredStorageType>(std::get<I>(entries), id),
         ...);
    }

    template <typename Registrations>
    void append_registration_candidates(std::string& message,
                                        Registrations& registrations) {
        message += "\nregistered plans:";
        bool first = true;
        for (auto&& entry : registrations) {
            auto& record = entry.second.registration;
            message += first ? " " : ", ";
            append_registration_candidate(message, record);
            first = false;
        }
    }

    template <typename Entries>
    void append_registration_binding_candidates(std::string& message,
                                                Entries& entries) {
        message += "\ncandidates:";
        bool first = true;
        for (auto&& entry : entries) {
            message += first ? " " : ", ";
            detail::append_runtime_binding_summary(message, entry.second.binding);
            first = false;
        }
    }

    [[noreturn]] void throw_type_already_registered(
        const detail::registration_metadata& registration) {
        std::string message = "type already registered: interface ";
        append_registration_identity(message, registration);
        message += "\nregistration plan: ";
        registration.append_plan(message);
        throw type_already_registered_exception(std::move(message));
    }

    [[noreturn]] void throw_type_index_already_registered(
        const detail::registration_metadata& registration) {
        std::string message = "type index already registered: interface ";
        append_registration_identity(message, registration);
        message += ", index type ";
        append_type_name(message, registration.index_key_type);
        message += "\nregistration plan: ";
        registration.append_plan(message);
        throw type_index_already_registered_exception(std::move(message));
    }

    void append_registration_identity(
        std::string& message,
        const detail::registration_metadata& registration) {
        append_type_name(message, registration.interface_type);
        message += ", storage ";
        append_type_name(message, registration.registered_storage_type);
    }

    void append_registration_candidate(
        std::string& message,
        const detail::registration_metadata& registration) {
        message += "{ binding ";
        message += std::to_string(registration.binding_id);
        message += ", interface ";
        append_type_name(message, registration.interface_type);
        if (registration.indexed) {
            message += ", index key ";
            append_type_name(message, registration.index_key_type);
        }
        message += ", plan ";
        registration.append_plan(message);
        message += " }";
    }

    using binding_record = detail::runtime_binding_record<
        instance_factory_interface<container_type>*,
        typename rtti_type::type_index>;

    parent_container_type* parent_ = nullptr;

    struct index_data {
        binding_record* binding;
        void* cache;

        operator bool() const { return binding != nullptr; }
    };

    using index_definition_list_type = to_type_list_t<index_definition_type>;
    using index_type =
        detail::index_impl<index_definition_list_type, index_data,
                           allocator_type>;

    struct type_factory_data : index_type {
        type_factory_data(allocator_type& allocator)
            : index_type(allocator), entries(allocator) {}

        typename ContainerTraits::template type_map_type<
            detail::registry_entry<
                binding_record,
                instance_factory_ptr<
                    instance_factory_interface<container_type>>>,
            allocator_type>
            entries;
    };

    typename ContainerTraits::template type_map_type<type_factory_data,
                                                     allocator_type>
        type_factories_;

    // Due to conversions, there is no 1:1 mapping between cached types and factories
    typename ContainerTraits::template type_cache_type<void*, allocator_type>
        type_cache_;
    std::size_t next_binding_id_ = 0;
};
} // namespace dingo
