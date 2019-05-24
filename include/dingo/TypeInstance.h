#pragma once

#include "dingo/Tuple.h"
#include "dingo/Exceptions.h"

namespace dingo
{
    template < class T > struct IsSmartPtr { static const bool value = false; };
    template < class T > struct IsSmartPtr< std::unique_ptr< T > > { static const bool value = true; };
    template < class T > struct IsSmartPtr< std::unique_ptr< T >* > { static const bool value = true; };
    template < class T > struct IsSmartPtr< std::shared_ptr< T > > { static const bool value = true; };
    template < class T > struct IsSmartPtr< std::shared_ptr< T >* > { static const bool value = true; };

    template < class T > struct PointerTraits { static void* ToAddress(T& value) { return &value; } };
    template < class T > struct PointerTraits< T* > { static void* ToAddress(T* ptr) { return ptr; } };
    template < class T > struct PointerTraits< std::shared_ptr< T > > { static void* ToAddress(std::shared_ptr< T >& ptr) { return ptr.get(); } };
    template < class T > struct PointerTraits< std::unique_ptr< T > > { static void* ToAddress(std::unique_ptr< T >& ptr) { return ptr.get(); } };

    struct RuntimeType {};

    template < class T, class U > struct RebindType { typedef U type; };
    template < class T, class U > struct RebindType< T&, U > { typedef U& type; };
    template < class T, class U > struct RebindType< T&&, U > { typedef U&& type; };
    template < class T, class U > struct RebindType< T*, U > { typedef U* type; };
    template < class T, class U > struct RebindType< std::shared_ptr< T >, U > { typedef std::shared_ptr< U > type; };
    template < class T, class U > struct RebindType< std::shared_ptr< T >&, U > { typedef std::shared_ptr< U >& type; };
    template < class T, class U > struct RebindType< std::shared_ptr< T >&&, U > { typedef std::shared_ptr< U >&& type; };
    template < class T, class U > struct RebindType< std::shared_ptr< T >*, U > { typedef std::shared_ptr< U >* type; };

    // TODO: how to rebind those properly?
    template < class T, class Deleter, class U > struct RebindType< std::unique_ptr< T, Deleter >, U > { typedef std::unique_ptr< U, std::default_delete< U > > type; };
    template < class T, class Deleter, class U > struct RebindType< std::unique_ptr< T, Deleter >&, U > { typedef std::unique_ptr< U, std::default_delete< U > >& type; };
    template < class T, class Deleter, class U > struct RebindType< std::unique_ptr< T, Deleter >*, U > { typedef std::unique_ptr< U, std::default_delete< U > >* type; };

    class ITypeInstance
    {
    public:
        virtual ~ITypeInstance() {};

        virtual void* GetValueType(const std::type_info&) = 0;
        virtual void* GetLvalueReferenceType(const std::type_info&) = 0;
        virtual void* GetRvalueReferenceType(const std::type_info&) = 0;
        virtual void* GetPointerType(const std::type_info&) = 0;

        virtual bool Destroyable() = 0;
    };

    template < class T > struct TypeInstanceGetter
    {
        static T Get(ITypeInstance& instance) { return *static_cast<T*>(instance.GetValueType(typeid(typename RebindType< T, RuntimeType >::type))); }
    };

    template < class T > struct TypeInstanceGetter<T&>
    {
        static T& Get(ITypeInstance& instance) { return *static_cast<std::decay_t< T >*>(instance.GetLvalueReferenceType(typeid(typename RebindType< T&, RuntimeType >::type))); }
    };

    template < class T > struct TypeInstanceGetter<T&&>
    {
        static T&& Get(ITypeInstance& instance) { return std::move(*static_cast<std::decay_t< T >*>(instance.GetRvalueReferenceType(typeid(RebindType< T&&, RuntimeType >::type)))); }
    };

    template < class T > struct TypeInstanceGetter<T*>
    {
        static T* Get(ITypeInstance& instance) { return static_cast<T*>(instance.GetPointerType(typeid(RebindType< T*, RuntimeType >::type))); }
    };

    template < class T > struct TypeInstanceGetter< std::unique_ptr< T > >
    {
        static std::unique_ptr< T > Get(ITypeInstance& instance) { return std::move(*static_cast<std::unique_ptr< T >*>(instance.GetValueType(typeid(typename RebindType< std::unique_ptr< T >, RuntimeType >::type)))); }
    };

    template< typename T, typename Type > void* GetPtr(const std::type_info& type, Type& instance)
    {
        void* ptr = nullptr;

        if (!ApplyType((T*)0, type, [&](auto element)
            {
                if (IsSmartPtr< Type >::value)
                {
                    if (IsSmartPtr< std::decay_t< decltype(element)::type > >::value)
                    {
                        ptr = &instance;

                    }
                    else
                    {
                        ptr = PointerTraits< Type >::ToAddress(instance);
                    }
                }
                else
                {
                    ptr = PointerTraits< Type >::ToAddress(instance);
                }
            }))
        {
            const std::string typeName = type.name();
            const std::string types = typeid(T).name();
            throw TypeNotConvertibleException();
        }

            return ptr;
    }

    template< class T, bool > struct TypeInstanceDestructor
    {
        void SetTransferred(bool) {}
        void Destroy(T&) {}
    };

    template< class T > struct TypeInstanceDestructor< T*, true >
    {
        bool Transferred = false;
        void SetTransferred(bool value) { Transferred = value; }
        void Destroy(T* ptr) { if (!Transferred) delete ptr; }
    };

    template < typename T, typename TypeStorage > class TypeInstance
        : public ITypeInstance
        , private TypeInstanceDestructor< T, TypeStorage::Destroyable >
    {
        typedef typename TypeStorage::Conversions Conversions;

    public:
        template < typename Ty > TypeInstance(Ty&& instance)
            : instance_(std::forward< Ty&& >(instance))
        {}

        ~TypeInstance()
        {
            this->Destroy(instance_);
        }

        void* GetValueType(const std::type_info& type) override { return GetPtr< Conversions::ValueTypes >(type, instance_); }
        void* GetLvalueReferenceType(const std::type_info& type) override { return GetPtr< Conversions::LvalueReferenceTypes >(type, instance_); }
        void* GetRvalueReferenceType(const std::type_info& type) override { return GetPtr< Conversions::RvalueReferenceTypes >(type, instance_); }

        void* GetPointerType(const std::type_info& type) override
        {
            // TODO: thread-safe...
            void* ptr = GetPtr< Conversions::PointerTypes >(type, instance_);
            this->SetTransferred(true);
            return ptr;
        }

        bool Destroyable() override { return TypeStorage::Destroyable; }

    private:
        T instance_;
    };
}