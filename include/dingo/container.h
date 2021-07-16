#pragma once

#include <dingo/decay.h>
#include <dingo/collection_traits.h>
#include <dingo/class_instance.h>
#include <dingo/exceptions.h>
#include <dingo/class_instance_factory.h>
#include <dingo/memory/arena_allocator.h>
#include <dingo/resolving_context.h>
#include <dingo/scope_guard.h>

#include <map>
#include <vector>

namespace dingo
{
    class container;

    template < typename T > struct TypeRecursionGuard
    {
        TypeRecursionGuard(bool typeInitialized)
            : typeInitialized_(typeInitialized)
        {
            if (typeInitialized_) return;

            if (!visited_)
            {
                visited_ = true;
            }
            else
            {
                throw type_recursion_exception();
            }
        }

        ~TypeRecursionGuard()
        {
            if (typeInitialized_) return;
            visited_ = false;
        }

        static thread_local bool visited_;
        bool typeInitialized_;
    };

    template < typename T > thread_local bool TypeRecursionGuard< T >::visited_ = false;

    class container
    {
        friend class resolving_context;

    public:
        container()
        {}

        template <
            typename TypeStorage,
            typename TypeInterface = decay_t< typename TypeStorage::Type >
        > void register_binding()
        {
            check_interface_requirements< TypeInterface, TypeStorage::Type >();
            type_factories_.emplace(typeid(TypeInterface), std::make_unique< class_instance_factory< TypeInterface, TypeStorage::Type, TypeStorage > >());
        }

        template <
            typename TypeStorage,
            typename... TypeInterfaces,
            typename = std::enable_if_t< (sizeof...(TypeInterfaces) > 1) >
        > void register_binding()
        {
            auto storage = std::make_shared< TypeStorage >();

            for_each((type_list< TypeInterfaces... >*)0, [&](auto element)
            {
                typedef typename decltype(element)::type TypeInterface;

                check_interface_requirements< TypeInterface, TypeStorage::Type >();
                type_factories_.emplace(typeid(TypeInterface), std::make_unique< class_instance_factory< TypeInterface, TypeStorage::Type, std::shared_ptr< TypeStorage > > >(storage));
            });
        }

        template <
            typename T
            , typename R = std::conditional_t< std::is_rvalue_reference_v< T >, std::remove_reference_t< T >, T >
        > R resolve()
        {
            resolving_context context(*this);
            auto guard = make_scope_guard([&context] { if (!std::uncaught_exceptions()) { context.finalize(); } });
            return resolve< T, true >(context);
        }

    private:
        template <
            typename T
            , bool RemoveRvalueReferences
            , typename R = std::conditional_t<
                RemoveRvalueReferences,
                std::conditional_t < std::is_rvalue_reference_v< T >, std::remove_reference_t< T >, T >, T
            >
        > R resolve(resolving_context& context)
        {
            typedef decay_t< T > Type;

            auto range = type_factories_.equal_range(typeid(Type));
            if (range.first != range.second)
            {
                auto it = range.first;
                TypeRecursionGuard< Type > guard(it->second->is_resolved());
                auto instance = it->second->resolve(context);
                return class_instance_getter< T >::get(*instance);
            }

            return resolve_multiple< T >(context);
        }

        template <
            typename T
        > std::enable_if_t< !collection_traits< decay_t< T > >::is_collection, T > resolve_multiple(resolving_context& context)
        {
            throw type_not_found_exception();
        }

        template <
            typename T
        > std::enable_if_t< collection_traits< decay_t< T > >::is_collection, T > resolve_multiple(resolving_context& context)
        {
            typedef decay_t< T > Type;
            typedef decay_t< typename Type::value_type > ValueType;

            auto range = type_factories_.equal_range(typeid(ValueType));
            
            auto results = context.get_allocator< Type >().allocate(1);
            new (results) Type;
            collection_traits< Type >::reserve(*results, std::distance(range.first, range.second));
            
            if (range.first != range.second)
            {
                for (auto it = range.first; it != range.second; ++it)
                {
                    TypeRecursionGuard< ValueType > guard(it->second->is_resolved());
                    auto instance = it->second->resolve(context);
                    collection_traits< Type >::add(*results, class_instance_getter< typename Type::value_type >::get(*instance));
                }

                return *results;
            }

            throw type_not_found_exception();
        }

        template < class TypeInterface, class Type > void check_interface_requirements()
        {
            static_assert(!std::is_reference_v< TypeInterface >);
            static_assert(std::is_convertible_v< decay_t< Type >*, decay_t< TypeInterface >* >);
        }

        std::multimap< std::type_index, std::unique_ptr< class_instance_factory_i > > type_factories_;
    };

    template < typename T > T resolving_context::resolve()
    {
        return container_.resolve< T, false >(*this);
    }
}