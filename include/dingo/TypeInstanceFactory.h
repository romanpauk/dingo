#pragma once

namespace dingo 
{
	class Context;

	template< typename TypeInterface, typename Storage, bool Destroyable > struct TypeInstanceCache
	{
		ITypeInstance* Resolve(Context& context, Storage& storage)
		{
			return new TypeInstance< TypeInterface, Storage >(storage.Resolve(context));
		}
	};

	template< typename TypeInterface, typename Storage > struct TypeInstanceCache< TypeInterface, Storage, false >
	{
		ITypeInstance* Resolve(Context& context, Storage& storage)
		{
			// TODO: thread-safe
			if (!instance_)
			{
				instance_.emplace(storage.Resolve(context));
			}

			return &*instance_;
		}

	private:
		std::optional< TypeInstance< TypeInterface, Storage > > instance_;
	};

	struct ITypeInstanceFactory
	{
		virtual ~ITypeInstanceFactory() {}
		virtual ITypeInstance* Resolve(Context& context) = 0;
	};

	template < 
		typename TypeInterface, 
		typename Type, 
		typename Storage 
	> 
	class TypeInstanceFactory
		: public ITypeInstanceFactory
	{
		Storage storage_;
		typedef decltype(storage_.Resolve(std::declval< Context& >())) ResolveType;
		typedef typename RebindType < ResolveType, TypeInterface >::type InterfaceType;
		TypeInstanceCache< InterfaceType, Storage, Storage::Destroyable > instance_;

	public:
		TypeInstanceFactory()
		{}

		ITypeInstance* Resolve(Context& context) override
		{
			return instance_.Resolve(context, storage_);
		}
	};

	template <
		typename TypeInterface,
		typename Type,
		typename Storage 
	>
		class TypeInstanceFactory< TypeInterface, Type, std::shared_ptr< Storage > >
		: public ITypeInstanceFactory
	{
		std::shared_ptr< Storage > storage_;
		typedef decltype(storage_->Resolve(std::declval< Context& >())) ResolveType;
		typedef typename RebindType < ResolveType, TypeInterface >::type InterfaceType;
		TypeInstanceCache< InterfaceType, Storage, Storage::Destroyable > instance_;

	public:
		TypeInstanceFactory(std::shared_ptr< Storage > storage)
			: storage_(storage)
		{}

		ITypeInstance* Resolve(Context& context) override
		{
			return instance_.Resolve(context, *storage_);
		}
	};
}