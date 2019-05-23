#pragma once

#include "dingo/TypeDecay.h"
#include "dingo/TypeInstance.h"
#include "dingo/Exceptions.h"
#include "dingo/TypeInstanceFactory.h"

namespace dingo
{
    class Container;

	class Context
	{
	public:
		Context(Container& container)
			: container_(container)
		{}

		template < typename T > T Resolve();

    private:
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
			template < typename T, typename = std::enable_if_t< !std::is_same_v< DisabledType, std::decay_t< T > > > > operator T&& () { return context_.Resolve< T&& >(); }

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
			Apply((std::tuple< TypeInterfaces... >*)0, [&](auto element)
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
				std::unique_ptr< ITypeInstance > cleanup;
				auto instance = it->second->Resolve(context);
				if (instance->Destroyable()) cleanup.reset(instance);
				
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