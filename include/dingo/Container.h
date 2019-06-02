#pragma once

#include "dingo/TypeDecay.h"
#include "dingo/TypeInstance.h"
#include "dingo/Exceptions.h"
#include "dingo/TypeInstanceFactory.h"
#include "dingo/ArenaAllocator.h"

#include <set>
#include <unordered_map>
#include <queue>

namespace dingo
{
    class Container;

    class Context
    {
    public:
        Context(Container& container)
            : container_(container)
            , arena_(buffer_)
            , allocator_(arena_)
            , types_(allocator_)
            , instances_(allocator_)
        {}

        template < typename T > T Resolve();

        struct TypeRecursionGuard
        {
            TypeRecursionGuard(Context& context, const std::type_info& type, bool typeInitialized)
                : context_(context)
                , type_(type)
                , typeInitialized_(typeInitialized)
            {
                if(typeInitialized_) return;

                if(!context.types_.insert(&type_).second)
                {
                    throw TypeRecursionException();
                }
            }
            
            ~TypeRecursionGuard()
            {
                if(typeInitialized_) return;

                context_.types_.erase(&type_);
            }

            Context& context_;
            const std::type_info& type_;
            bool typeInitialized_;
        };

        template < typename T > ArenaAllocator< T > GetAllocator() { return allocator_; }
          
        void AddInstance(ITypeInstance* instance)
        {
            instances_.push_back(instance);
        }

        ~Context()
        {
            while (!instances_.empty())
            {
                instances_.back()->~ITypeInstance();
                instances_.pop_back();
            }
        }
        
    private:
        std::array< unsigned char, 1024 > buffer_;
        Arena arena_;
        ArenaAllocator< void > allocator_;

        std::set< const std::type_info*, std::less< const std::type_info* >, ArenaAllocator< const std::type_info* > > types_;
        std::list< ITypeInstance*, ArenaAllocator< ITypeInstance* > > instances_;

        Container& container_;
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
                Context::TypeRecursionGuard guard(context, typeid(Type), it->second->IsCached());
                auto instance = it->second->Resolve(context);
                // std::unique_ptr< ITypeInstance > cleanup;
                // if (instance->Destroyable()) cleanup.reset(instance);
                return TypeInstanceGetter< T >::Get(*instance);                
            }

            throw TypeNotFoundException();
        }

        template < class TypeInterface, class Type > void CheckInterface()
        {
            static_assert(!std::is_reference_v< TypeInterface >);
            static_assert(std::is_convertible_v< typename TypeDecay< Type >::type*, typename TypeDecay< TypeInterface >::type* >);
        }

        std::unordered_map< std::type_index, std::unique_ptr< ITypeInstanceFactory > > type_;
    };

    template < typename T > T Context::Resolve()
    {
        return container_.Resolve< T, false >(*this);
    }
}