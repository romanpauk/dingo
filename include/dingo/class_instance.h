#pragma once

#include <dingo/type_list.h>
#include <dingo/exceptions.h>

namespace dingo
{
    template < class T > struct is_smart_ptr : std::false_type {}; 
    template < class T > struct is_smart_ptr< std::unique_ptr< T > > : std::true_type {}; 
    template < class T > struct is_smart_ptr< std::unique_ptr< T >* > : std::true_type {}; 
    template < class T > struct is_smart_ptr< std::shared_ptr< T > > : std::true_type {}; 
    template < class T > struct is_smart_ptr< std::shared_ptr< T >* >: std::true_type {}; 

    template < class T > struct pointer_traits { static void* get_address(T& value) { return &value; } };
    template < class T > struct pointer_traits< T* > { static void* get_address(T* ptr) { return ptr; } };
    template < class T > struct pointer_traits< std::shared_ptr< T > > { static void* get_address(std::shared_ptr< T >& ptr) { return ptr.get(); } };
    template < class T > struct pointer_traits< std::unique_ptr< T > > { static void* get_address(std::unique_ptr< T >& ptr) { return ptr.get(); } };

    struct runtime_type {};

    template < class T, class U > struct rebind_type { typedef U type; };
    template < class T, class U > struct rebind_type< T&, U > { typedef U& type; };
    template < class T, class U > struct rebind_type< T&&, U > { typedef U&& type; };
    template < class T, class U > struct rebind_type< T*, U > { typedef U* type; };
    template < class T, class U > struct rebind_type< std::shared_ptr< T >, U > { typedef std::shared_ptr< U > type; };
    template < class T, class U > struct rebind_type< std::shared_ptr< T >&, U > { typedef std::shared_ptr< U >& type; };
    template < class T, class U > struct rebind_type< std::shared_ptr< T >&&, U > { typedef std::shared_ptr< U >&& type; };
    template < class T, class U > struct rebind_type< std::shared_ptr< T >*, U > { typedef std::shared_ptr< U >* type; };

    // TODO: how to rebind those properly?
    template < class T, class Deleter, class U > struct rebind_type< std::unique_ptr< T, Deleter >, U > { typedef std::unique_ptr< U, std::default_delete< U > > type; };
    template < class T, class Deleter, class U > struct rebind_type< std::unique_ptr< T, Deleter >&, U > { typedef std::unique_ptr< U, std::default_delete< U > >& type; };
    template < class T, class Deleter, class U > struct rebind_type< std::unique_ptr< T, Deleter >*, U > { typedef std::unique_ptr< U, std::default_delete< U > >* type; };

    class class_instance_i
    {
    public:
        virtual ~class_instance_i() {};

        virtual void* get_value(const std::type_info&) = 0;
        virtual void* get_lvalue_reference(const std::type_info&) = 0;
        virtual void* get_rvalue_reference(const std::type_info&) = 0;
        virtual void* get_pointer(const std::type_info&) = 0;
    };

    template < class T > struct class_instance_getter
    {
        static T get(class_instance_i& instance) { return *static_cast<T*>(instance.get_value(typeid(typename rebind_type< T, runtime_type >::type))); }
    };

    template < class T > struct class_instance_getter<T&>
    {
        static T& get(class_instance_i& instance) { return *static_cast<std::decay_t< T >*>(instance.get_lvalue_reference(typeid(typename rebind_type< T&, runtime_type >::type))); }
    };

    template < class T > struct class_instance_getter<T&&>
    {
        static T&& get(class_instance_i& instance) { return std::move(*static_cast<std::decay_t< T >*>(instance.get_rvalue_reference(typeid(rebind_type< T&&, runtime_type >::type)))); }
    };

    template < class T > struct class_instance_getter<T*>
    {
        static T* get(class_instance_i& instance) { return static_cast<T*>(instance.get_pointer(typeid(rebind_type< T*, runtime_type >::type))); }
    };

    template < class T > struct class_instance_getter< std::unique_ptr< T > >
    {
        static std::unique_ptr< T > get(class_instance_i& instance) { return std::move(*static_cast<std::unique_ptr< T >*>(instance.get_value(typeid(typename rebind_type< std::unique_ptr< T >, runtime_type >::type)))); }
    };

    template< typename T, typename Type > void* get_type_ptr(const std::type_info& type, Type& instance)
    {
        void* ptr = nullptr;

        if (!for_type((T*)0, type, [&](auto element)
        {
            if (is_smart_ptr< Type >::value)
            {
                if (is_smart_ptr< std::decay_t< decltype(element)::type > >::value)
                {
                    ptr = &instance;

                }
                else
                {
                    ptr = pointer_traits< Type >::get_address(instance);
                }
            }
            else
            {
                ptr = pointer_traits< Type >::get_address(instance);
            }
        }
        ))
        {
            const std::string typeName = type.name();
            const std::string types = typeid(T).name();
            throw type_not_convertible_exception();
        }

        return ptr;
    }

    template< class T, bool > struct class_instance_destructor
    {
        void set_transferred(bool) {}
        void destroy(T&) {}
    };

    template< class T > struct class_instance_destructor< T*, false >
    {
        void set_transferred(bool value) { transferred_ = value; }
        void destroy(T* ptr) { if (!transferred_) delete ptr; }

    private:
        bool transferred_ = false;
    };

    template < typename T, typename TypeStorage > class class_instance
        : public class_instance_i
        , private class_instance_destructor< T, TypeStorage::IsCaching >
    {
        typedef typename TypeStorage::Conversions Conversions;

    public:
        template < typename Ty > class_instance(Ty&& instance)
            : instance_(std::forward< Ty&& >(instance))
        {}

        ~class_instance()
        {
            this->destroy(instance_);
        }

        void* get_value(const std::type_info& type) override { return get_type_ptr< Conversions::ValueTypes >(type, instance_); }
        void* get_lvalue_reference(const std::type_info& type) override { return get_type_ptr< Conversions::LvalueReferenceTypes >(type, instance_); }
        void* get_rvalue_reference(const std::type_info& type) override { return get_type_ptr< Conversions::RvalueReferenceTypes >(type, instance_); }

        void* get_pointer(const std::type_info& type) override
        {
            void* ptr = get_type_ptr< Conversions::PointerTypes >(type, instance_);
            this->set_transferred(true);
            return ptr;
        }

    private:
        T instance_;
    };
}