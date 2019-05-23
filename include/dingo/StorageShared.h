#pragma once

#include "dingo/TypeDecay.h"
#include "dingo/Type.h"
#include "dingo/Storage.h"

namespace dingo 
{
	class Container;

	template < typename Type, typename U > struct Conversions< Shared, Type, U >
	{
		typedef std::tuple<> ValueTypes;
		typedef std::tuple< U& > LvalueReferenceTypes;
		typedef std::tuple<> RvalueReferenceTypes;
		typedef std::tuple< U* > PointerTypes;
	};

	template < typename Type, typename U > struct Conversions< Shared, Type*, U >
	{
		typedef std::tuple<> ValueTypes;
		typedef std::tuple< U& > LvalueReferenceTypes;
		typedef std::tuple<> RvalueReferenceTypes;
		typedef std::tuple< U* > PointerTypes;
	};

	template < typename Type, typename U > struct Conversions< Shared, std::shared_ptr< Type >, U >
	{
		typedef std::tuple< std::shared_ptr< U > > ValueTypes;
		typedef std::tuple< U&, std::shared_ptr< U >& > LvalueReferenceTypes;
		typedef std::tuple<> RvalueReferenceTypes;
		typedef std::tuple< U*, std::shared_ptr< U >* > PointerTypes;
	};

	template < typename Type, typename U > struct Conversions< Shared, std::unique_ptr< Type >, U >
	{
		typedef std::tuple<> ValueTypes;
		typedef std::tuple< U*, std::unique_ptr< U >* > PointerTypes;
		typedef std::tuple< U&, std::unique_ptr< U >& > LvalueReferenceTypes;
		typedef std::tuple<> RvalueReferenceTypes;
	};
		
    template < typename Type, typename Conversions > class Storage< Shared, Type, Conversions >
    {
	public:
		static const bool Destroyable = false;

		typedef Conversions Conversions;
		typedef Type Type;

		Storage()
            : initialized_(false)
        {}

        ~Storage()
        {
            if (initialized_)
            {
                reinterpret_cast<Type*>(&instance_)->~Type();
            }
        }

        Type* Resolve(Context& context)
        {
			// TODO: thread-safe
            if (!initialized_)
            {
                TypeFactory< typename TypeDecay< Type >::type >::template Construct< Type*, Container::ConstructorArgument< Type > >(context, &instance_);
				initialized_ = true;
            }

			return reinterpret_cast<Type*>(&instance_);
        }

    private:
        std::aligned_storage_t< sizeof(Type) > instance_;
        bool initialized_;
    };

	template < typename Type, typename Conversions > class Storage< Shared, Type*, Conversions >
		: public Storage< Shared, std::unique_ptr< Type >, Conversions >
	{
	public:
		//*
		Type* Resolve(Context& context)
		{
			return Storage< Shared, std::unique_ptr< Type >, Conversions >::Resolve(context).get();
		}
		//*/

		/*
		static const bool Destroyable = false;

		typedef Conversions Conversions;
		typedef Type Type;

		Storage()
			: instance_()
		{}

		~Storage()
		{
			if (instance_)
			{
				delete instance_;
			}
		}

		Type* Resolve(Context& context)
		{
			// TODO: thread-safe
			if (!instance_)
			{
				instance_ = TypeFactory< typename TypeDecay< Type >::type >::template Construct< Type*, Container::ConstructorArgument< Type > >(context);
			}

			return instance_;
		}

	private:
		Type* instance_;
		*/
	};

	template < typename Type, typename Conversions > class Storage< Shared, std::shared_ptr< Type >, Conversions >
	{
	public:
		static const bool Destroyable = false;

		typedef Conversions Conversions;
		typedef Type Type;

		std::shared_ptr< Type >& Resolve(Context& context)
		{
			// TODO: thread-safe
			if (!instance_)
			{
				instance_ = TypeFactory< Type >::template Construct< std::shared_ptr< Type >, Container::ConstructorArgument< Type > >(context);
			}

			return instance_;
		}

	private:
		std::shared_ptr< Type > instance_;
	};

	template < typename Type, typename Conversions > class Storage< Shared, std::unique_ptr< Type >, Conversions >
    {
	public:
		static const bool Destroyable = false;

		typedef Conversions Conversions;
		typedef Type Type;

		std::unique_ptr< Type >& Resolve(Context& context)
        {
			// TODO: thread-safe
            if (!instance_)
            {
				instance_ = TypeFactory< Type >::template Construct< std::unique_ptr< Type >, Container::ConstructorArgument< Type > >(context);
			}

			return instance_;
		}

    private:
        std::unique_ptr< Type > instance_;
    };
}