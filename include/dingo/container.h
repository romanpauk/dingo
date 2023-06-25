#pragma once

#include <dingo/decay.h>
#include <dingo/collection_traits.h>
#include <dingo/class_instance.h>
#include <dingo/exceptions.h>
#include <dingo/class_instance_factory.h>
#include <dingo/class_recursion_guard.h>
#include <dingo/memory/arena_allocator.h>
#include <dingo/resolving_context.h>
#include <dingo/scope_guard.h>
#include <dingo/annotated.h>

#include <map>
#include <typeindex>

namespace dingo
{
    template < typename Allocator = std::allocator< char > > class container
    {
        friend class resolving_context< container< Allocator > >;

    public:
        typedef Allocator allocator_type;

        allocator_type& get_allocator() { return allocator_; }

        template <
            typename TypeStorage,
            typename TypeInterface = decay_t< typename TypeStorage::Type >,
            typename... Args
        > void register_binding(Args&&... args)
        {
            check_interface_requirements< typename annotated_traits< TypeInterface >::type, typename TypeStorage::Type >();
            type_factories_.emplace(typeid(TypeInterface), std::make_unique<
                class_instance_factory< container< Allocator >, typename annotated_traits< TypeInterface >::type, typename TypeStorage::Type, TypeStorage > >(std::forward< Args >(args)...));
        }

        template <
            typename TypeStorage,
            typename... TypeInterfaces,
            typename... Args,
            typename = std::enable_if_t< (sizeof...(TypeInterfaces) > 1) >
        > void register_binding(Args&&... args)
        {
            auto storage = std::make_shared< TypeStorage >(std::forward< Args >(args)...);

            for_each((type_list< TypeInterfaces... >*)0, [&](auto element)
            {
                typedef typename decltype(element)::type TypeInterface;

                check_interface_requirements< typename annotated_traits< TypeInterface >::type, typename TypeStorage::Type >();
                type_factories_.emplace(typeid(TypeInterface), std::make_unique< class_instance_factory< container< Allocator >, typename annotated_traits< TypeInterface >::type, typename TypeStorage::Type, std::shared_ptr< TypeStorage > > >(storage));
            });
        }

        template <
            typename T
            , typename R = typename annotated_traits< std::conditional_t< std::is_rvalue_reference_v< T >, std::remove_reference_t< T >, T > >::type
        > R resolve()
        {
            resolving_context< container< Allocator > > context(*this);
            auto guard = make_scope_guard([&context] { if (!std::uncaught_exceptions()) { context.finalize(); } });
            return resolve< T, true >(context);
        }

    private:
        template <
            typename T
            , bool RemoveRvalueReferences
            , typename R = 
                std::conditional_t<
                RemoveRvalueReferences,
                std::conditional_t < std::is_rvalue_reference_v< T >, std::remove_reference_t< T >, T >, T
            >
        > R resolve(resolving_context< container< Allocator > >& context)
        {
            typedef decay_t< T > Type;

            auto range = type_factories_.equal_range(typeid(Type));
            if (range.first != range.second)
            {
                auto it = range.first;
                class_recursion_guard< Type > guard(!it->second->is_resolved());
                auto instance = it->second->resolve(context);
                return class_instance_traits< typename annotated_traits< T >::type >::get(*instance);
            }

            return resolve_multiple< T >(context);
        }

        template <
            typename T
        > std::enable_if_t< !collection_traits< decay_t< T > >::is_collection, T > resolve_multiple(resolving_context< container< Allocator > >& context)
        {
            throw type_not_found_exception();
        }

        template <
            typename T
        > std::enable_if_t< collection_traits< decay_t< T > >::is_collection, T > resolve_multiple(resolving_context< container< Allocator > >& context)
        {
            typedef decay_t< T > Type;
            typedef decay_t< typename Type::value_type > ValueType;

            auto range = type_factories_.equal_range(typeid(ValueType));

            // TODO: destructor for results is not called, leaking the memory.
            // Unfortunatelly for the case when this is called from resolve(), return value is T&.
            auto results = context.template get_allocator< Type >().allocate(1);
            new (results) Type;
            collection_traits< Type >::reserve(*results, std::distance(range.first, range.second));

            if (range.first != range.second)
            {
                for (auto it = range.first; it != range.second; ++it)
                {
                    class_recursion_guard< ValueType > guard(!it->second->is_resolved());
                    auto instance = it->second->resolve(context);
                    collection_traits< Type >::add(*results, class_instance_traits< typename Type::value_type >::get(*instance));
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

        allocator_type allocator_;

        std::multimap <
            std::type_index,
            std::unique_ptr< class_instance_factory_i< container< Allocator > > >,
            std::less< std::type_index >
            //Allocator
            /*
            std::allocator_traits< Allocator >::rebind_alloc <
                std::pair <
                    const std::type_index,
                    std::unique_ptr< class_instance_factory_i< container< Allocator > > >
                >
            >
            */
        > type_factories_;
    };
}