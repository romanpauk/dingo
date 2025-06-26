//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/allocator.h>
#include <dingo/annotated.h>
#include <dingo/class_instance_factory.h>
#include <dingo/class_instance_factory_traits.h>
#include <dingo/collection_traits.h>
#include <dingo/decay.h>
#include <dingo/exceptions.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/invoke.h>
#include <dingo/index.h>
#include <dingo/resolving_context.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/static_allocator.h>
#include <dingo/storage/unique.h> // TODO
#include <dingo/type_cache.h>
#include <dingo/type_map.h>
#include <dingo/type_registration.h>

#include <functional>
#include <map>
#include <optional>
#include <typeindex>
#include <variant>

namespace dingo {

struct none_t {};
template <typename T> struct is_none : std::bool_constant<false> {};
template <> struct is_none<none_t> : std::bool_constant<true> {};
template <typename T> static constexpr auto is_none_v = is_none<T>::value;

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
            [](auto& collection, auto&& value) {
                collection_traits<std::decay_t<decltype(collection)>>::add(
                    collection, std::move(value));
            });
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
                    return class_instance_factory_traits<
                        rtti_type,
                        typename annotated_traits<T>::type>::convert(cache);
                }
            } else {
                auto data = type_factories_.template get<decay_t<T>>();
                if (data) {
                    auto indexed =
                        data->template get_index<IdType>(get_allocator())
                            .find(id);

                    if (indexed) {
                        if (indexed->cache) {
                            return class_instance_factory_traits<
                                rtti_type, typename annotated_traits<T>::type>::
                                convert(indexed->cache);
                        }
                    }
                }
            }
        }

        // TODO: this destructor is really slowing things down
        resolving_context context;
        return resolve<T, true, false>(context, std::forward<IdType>(id));
    }

    template <typename T, typename Factory = constructor_detection<decay_t<T>>>
    T construct(Factory factory = Factory()) {
        // TODO: nothrow constructuble
        resolving_context context;
        return factory.template construct<T>(context, *this);
    }

    template <typename T> T construct_collection() {
        return construct_collection<T>([](auto& collection, auto&& value) {
            collection_traits<std::decay_t<decltype(collection)>>::add(
                collection, std::move(value));
        });
    }

    template <typename T, typename Fn> T construct_collection(Fn&& fn) {
        static_assert(collection_traits<T>::is_collection,
                      "missing collection_traits specialization for type T");

        T results;
        resolving_context context;
        auto data = type_factories_.template get<decay_t<decay_t<
            typename collection_traits<T>::resolve_type>>>(); // TODO: needs to
                                                              // be
        // more generic...
        if (!data)
            throw type_not_found_exception();

        collection_traits<T>::reserve(results, data->factories.size());
        for (auto&& p : data->factories) {
            fn(results,
               resolve_collection_type<typename collection_traits<T>::resolve_type>(
                   *p.second, context));
        }
        return results;
    }

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
        (void)arg;
        //
        // An optimization and a feature: if storage type can be only queried by
        // single interface and the interface allows for proper deletion through
        // virtual destructor, store the type as the interface so temporaries
        // don't need to be created and because of that, the stored element
        // could be referenced in most usages as it will no longer be a
        // temporary object. The code below does the rewrite of storage type
        // into stored type.
        //
        using interface_type_0 = std::tuple_element_t<
            0, typename registration::interface_type::type_tuple>;
        using stored_type = rebind_type_t<
            typename registration::storage_type::type,
            std::conditional_t<
                std::tuple_size_v<
                    typename registration::interface_type::type_tuple> == 1 &&
                    std::has_virtual_destructor_v<interface_type_0> &&
                    type_traits<typename registration::storage_type::type>::
                        is_pointer_type,
                interface_type_0,
                decay_t<typename registration::storage_type::type>>>;

        using storage_type =
            detail::storage<typename registration::scope_type::type,
                            typename registration::storage_type::type,
                            stored_type,
                            typename registration::factory_type::type,
                            typename registration::conversions_type::type>;

        using class_instance_container_type =
            typename container_type::template rebind_t<
                typename ContainerTraits::template rebind_t<
                    type_list<typename ContainerTraits::tag_type,
                              typename registration::interface_type>>,
                allocator_type, container_type>;

        using class_instance_data_type =
            class_instance_data<class_instance_container_type, storage_type>;

        if constexpr (std::tuple_size_v<
                          typename registration::interface_type::type_tuple> ==
                      1) {
            using interface_type = std::tuple_element_t<
                0, typename registration::interface_type::type_tuple>;

            using class_instance_factory_type = class_instance_factory<
                container_type, typename annotated_traits<interface_type>::type,
                storage_type, class_instance_data_type>;

            if constexpr (!is_none_v<std::decay_t<Arg>>) {
                auto&& [factory, factory_container] =
                    allocate_factory<class_instance_factory_type>(
                        this, std::forward<Arg>(arg));
                register_type_factory<interface_type, storage_type>(
                    std::move(factory), std::move(id));
                return *factory_container;
            } else {
                auto&& [factory, factory_container] =
                    allocate_factory<class_instance_factory_type>(this);
                register_type_factory<interface_type, storage_type>(
                    std::move(factory), std::move(id));
                return *factory_container;
            }
        } else {
            std::shared_ptr<class_instance_data_type> data;
            if constexpr (!is_none_v<std::decay_t<Arg>>) {
                data = std::allocate_shared<class_instance_data_type>(
                    allocator_traits::rebind<class_instance_data_type>(
                        get_allocator()),
                    this, std::forward<Arg>(arg));
            } else {
                data = std::allocate_shared<class_instance_data_type>(
                    allocator_traits::rebind<class_instance_data_type>(
                        get_allocator()),
                    this);
            }

            for_each(
                typename registration::interface_type::type{},
                [&](auto element) {
                    using interface_type = typename decltype(element)::type;

                    using class_instance_factory_type = class_instance_factory<
                        container_type,
                        typename annotated_traits<interface_type>::type,
                        storage_type,
                        std::shared_ptr<class_instance_data_type>>;

                    register_type_factory<interface_type, storage_type>(
                        allocate_factory<class_instance_factory_type>(data)
                            .first,
                        id);
                });
            return data->container;
        }
    }

    template <typename TypeInterface, typename TypeStorage, typename Factory,
              typename IdType>
    void register_type_factory(Factory&& factory, IdType&& id) {
        // static_allocator returns null in the case an allocation (eg. the
        // factory can be null) There is no need to throw here as the insertion
        // will not be done later. This is TODO.

        check_interface_requirements<
            TypeStorage, typename annotated_traits<TypeInterface>::type,
            typename TypeStorage::type>();

        // TODO: this very crudely assumes types have different storages
        // for indexed types.
        auto factory_ptr = factory.get();
        auto pb =
            type_factories_.template insert<TypeInterface>(get_allocator());
        auto& data = pb.first;
        if (!data.factories
                 .template insert<
                     type_list<TypeInterface, typename TypeStorage::type>>(
                     std::forward<Factory>(factory))
                 .second) {
            throw type_already_registered_exception();
        }

        if constexpr (!is_none_v<std::decay_t<IdType>>) {
            if (!data.template get_index<IdType>(get_allocator())
                     .emplace(std::forward<IdType>(id),
                              index_data{factory_ptr, nullptr})) {
                bool erased = data.factories.template erase<
                    type_list<TypeInterface, typename TypeStorage::type>>();
                assert(erased);
                (void)erased;
                throw type_index_already_registered_exception();
            }
        }
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
        using Type = decay_t<T>;
        static_assert(!std::is_const_v<Type>);

        if constexpr (cache_enabled && CheckCache) {
            void* cache = type_cache_.template get<T>();
            if (cache) {
                return class_instance_factory_traits<
                    rtti_type,
                    typename annotated_traits<T>::type>::convert(cache);
            }
        }

        auto data = type_factories_.template get<Type>();
        if (data) {
            if constexpr (is_none_v<std::decay_t<IdType>>) {
                if (data->factories.size() == 1) {
                    auto& factory = data->factories.front();
                    return resolve<T, typename annotated_traits<T>::type>(
                        *factory, context);
                } else {
                    throw type_ambiguous_exception();
                }
            } else {
                auto indexed =
                    data->template get_index<IdType>(get_allocator()).find(id);

                if (indexed) {
                    if constexpr (cache_enabled && CheckCache) {
                        if (indexed->cache) {
                            return class_instance_factory_traits<
                                rtti_type, typename annotated_traits<T>::type>::
                                convert(indexed->cache);
                        }
                    }

                    return resolve<T, typename annotated_traits<T>::type>(
                        *indexed->factory, context, *indexed);
                }
            }
        } else if constexpr (!std::is_same_v<void*, decltype(parent_)>) {
            if (parent_) {
                return parent_->template resolve<T, RemoveRvalueReferences>(
                    context, std::forward<IdType>(id));
            }
        }

        // If we are trying to construct T and it is not wrapped in any way
        // and it is an aggregate type (needed so the code below compiles for
        // types with ambiguous construction like std::map<>)
        if constexpr (std::is_same_v< Type, std::decay_t<T> > &&
            std::is_aggregate_v< std::decay_t<T> >
        ) {
            // And it is constructible
            using type_detection = detail::automatic;
            using type_constructor = detail::constructor_detection< Type, type_detection, detail::list_initialization, false >;
            if constexpr(type_constructor::valid) {
                // Construct temporary through context so it can be referenced
                return context.template construct_temporary< typename annotated_traits<T>::type, type_detection >(*this);
            }
        }

        throw type_not_found_exception();
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    // TODO: two different resolve() calls due to different caches
    template <typename CachedT, typename T, typename Factory, typename Context>
    T resolve(Factory& factory, Context& context) {
        void* ptr = class_instance_factory_traits<rtti_type, T>::resolve(
            factory, context);
        if constexpr (cache_enabled) {
            if (factory.cacheable)
                type_cache_.template insert<CachedT>(ptr);
        }
        return class_instance_factory_traits<rtti_type, T>::convert(ptr);
    }

    struct index_data;

    template <typename CachedT, typename T, typename Factory, typename Context>
    T resolve(Factory& factory, Context& context, index_data& data) {
        void* ptr = class_instance_factory_traits<rtti_type, T>::resolve(
            factory, context);
        if constexpr (cache_enabled) {
            if (factory.cacheable)
                data.cache = ptr;
        }
        return class_instance_factory_traits<rtti_type, T>::convert(ptr);
    }

    template <typename T, typename Factory, typename Context>
    T resolve_collection_type(Factory& factory, Context& context) {
        void* ptr = class_instance_factory_traits<rtti_type, T>::resolve(
            factory, context);
        return class_instance_factory_traits<rtti_type, T>::convert(ptr);
    }

    template <class Storage, class TypeInterface, class Type>
    void check_interface_requirements() {
        static_assert(!std::is_reference_v<TypeInterface>);
        static_assert(
            std::is_convertible_v<decay_t<Type>*, decay_t<TypeInterface>*>);
        if constexpr (!std::is_same_v<decay_t<Type>, decay_t<TypeInterface>>) {
            static_assert(detail::storage_interface_requirements_v<
                              Storage, decay_t<Type>, decay_t<TypeInterface>>,
                          "storage requirements not met");
        }
    }

    template <typename U, typename... Args>
    std::pair<
        class_instance_factory_ptr<class_instance_factory_i<container_type>>,
        typename U::data_traits::container_type*>
    allocate_factory(Args&&... args) {
        auto alloc = allocator_traits::rebind<U>(get_allocator());
        U* instance = allocator_traits::allocate(alloc, 1);
        if (!instance)
            return {nullptr, nullptr};

        // TODO: should be nothrow-constructible
        // static_assert(std::is_nothrow_constructible_v<U, Args...>);
        allocator_traits::construct(alloc, instance,
                                    std::forward<Args>(args)...);

        instance->cacheable = U::storage_type::cacheable;

        return {instance, &instance->get_container()};
    }

    parent_container_type* parent_ = nullptr;

    struct index_data {
        class_instance_factory_i<container_type>* factory;
        void* cache;

        operator bool() const { return factory != nullptr; }
    };

    using index_type =
        index<typename container_traits_type::index_definition_type, index_data,
              allocator_type>;

    struct type_factory_data : index_type {
        type_factory_data(allocator_type& allocator)
            : index_type(allocator), factories(allocator) {}

        typename ContainerTraits::template type_map_type<
            class_instance_factory_ptr<
                class_instance_factory_i<container_type>>,
            allocator_type>
            factories;
    };

    typename ContainerTraits::template type_map_type<type_factory_data,
                                                     allocator_type>
        type_factories_;

    // Due to conversions, there is no 1:1 mapping between cached types and factories
    typename ContainerTraits::template type_cache_type<void*, allocator_type>
        type_cache_;
};
} // namespace dingo
