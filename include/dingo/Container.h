#pragma once

#include "dingo/TypeDecay.h"
#include "dingo/TypeInstance.h"
#include "dingo/Exceptions.h"
#include "dingo/TypeInstanceFactory.h"
#include "dingo/ArenaAllocator.h"

#include <unordered_map>
#include <forward_list>

namespace dingo
{
    class Container;

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
                throw TypeRecursionException();
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

    class Context
    {
    public:
        Context(Container& container)
            : container_(container)
            , arena_(buffer_)
            , allocator_(arena_)
            , instances_(allocator_)
        {}

        template < typename T > T Resolve();

        template < typename T > ArenaAllocator< T > GetAllocator() { return allocator_; }
          
        void AddInstance(ITypeInstance* instance)
        {
            instances_.push_front(instance);
        }

        ~Context()
        {
            for (auto& instance : instances_)
            {
                instance->~ITypeInstance();
            }
        }
        
    private:
        Container& container_;

        std::array< unsigned char, 1024 > buffer_;
        Arena arena_;
        ArenaAllocator< void > allocator_;     
        std::forward_list< ITypeInstance*, ArenaAllocator< ITypeInstance* > > instances_;
    };

    class Container
    {
        friend class Context;

    public:
        template < typename DisabledType > class ConstructorArgument
        {
        public:
            ConstructorArgument(Context& context)
                : context_(context)
            {}

            template < typename T, typename = std::enable_if_t< !std::is_same_v< DisabledType, std::decay_t< T > > > > operator T* () { return context_.Resolve< T* >(); }
            template < typename T, typename = std::enable_if_t< !std::is_same_v< DisabledType, std::decay_t< T > > > > operator T& () { return context_.Resolve< T& >(); }
            template < typename T, typename = std::enable_if_t< !std::is_same_v< DisabledType, std::decay_t< T > > > > operator T && () { return context_.Resolve< T&& >(); }

        private:
            Context& context_;
        };

        Container()
        {}

        template <
            typename TypeStorage,
            typename TypeInterface = typename TypeDecay< typename TypeStorage::Type >::type
        > void RegisterBinding()
        {
            CheckInterface< TypeInterface, TypeStorage::Type >();
            type_[typeid(TypeInterface)] = std::make_unique< TypeInstanceFactory< TypeInterface, TypeStorage::Type, TypeStorage > >();
        }

        template <
            typename TypeStorage,
            typename... TypeInterfaces,
            typename = std::enable_if_t< (sizeof...(TypeInterfaces) > 2) >
        > void RegisterBinding()
        {
            auto storage = std::make_shared< TypeStorage >();
            Apply((TypeList< TypeInterfaces... >*)0, [&](auto element)
                {
                    typedef typename decltype(element)::type TypeInterface;

                    CheckInterface< TypeInterface, TypeStorage::Type >();
                    type_[typeid(TypeInterface)] = std::make_unique< TypeInstanceFactory< TypeInterface, TypeStorage::Type, std::shared_ptr< TypeStorage > > >(storage);
                });
        }

        template <
            typename T
            , typename R = std::conditional_t< std::is_rvalue_reference_v< T >, std::remove_reference_t< T >, T >
        > R Resolve()
        {
            Context context(*this);
            return Resolve< T, true >(context);
        }

    private:
        template <
            typename T
            , bool RemoveRvalueReferences
            , typename R = std::conditional_t<
            RemoveRvalueReferences,
            std::conditional_t < std::is_rvalue_reference_v< T >, std::remove_reference_t< T >, T >, T
            >
        > R Resolve(Context& context)
        {
            typedef typename TypeDecay< T >::type Type;
            auto it = type_.find(typeid(Type));
            if (it != type_.end())
            {
                TypeRecursionGuard< Type > guard(it->second->IsCached());
                auto instance = it->second->Resolve(context);
                return TypeInstanceGetter< T >::Get(*instance);                
            }

            throw TypeNotFoundException();
        }

        template < class TypeInterface, class Type > void CheckInterface()
        {
            static_assert(!std::is_reference_v< TypeInterface >);
            static_assert(std::is_convertible_v< typename TypeDecay< Type >::type*, typename TypeDecay< TypeInterface >::type* >);
        }

        std::map< std::type_index, std::unique_ptr< ITypeInstanceFactory > > type_;
    };

    template < typename T > T Context::Resolve()
    {
        return container_.Resolve< T, false >(*this);
    }
}