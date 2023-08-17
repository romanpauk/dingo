#pragma once

#include <dingo/decay.h>
#include <dingo/class_factory.h>
#include <dingo/type_list.h>
#include <dingo/storage.h>

namespace dingo
{
    struct external {};

    template < typename Type, typename U > struct conversions< external, Type, U >
    {
        using value_types = type_list<>;
        using lvalue_reference_types = type_list< U& >;
        using rvalue_reference_types = type_list<>;
        using pointer_types = type_list< U* >;
    };

    template < typename Type, typename U > struct conversions< external, Type&, U >
        : public conversions< external, Type, U >
    {};

    template < typename Type, typename U > struct conversions< external, Type*, U >
        : public conversions< external, Type, U >
    {};

    template < typename Type, typename U > struct conversions< external, std::shared_ptr< Type >, U >
    {
        using value_types = type_list< std::shared_ptr< U > >;
        using lvalue_reference_types = type_list< U&, std::shared_ptr< U >& >;
        using rvalue_reference_types = type_list<>;
        using pointer_types = type_list< U*, std::shared_ptr< U >* >;
    };

    template < typename Type, typename U > struct conversions< external, std::shared_ptr< Type >&, U >
        : public conversions< external, std::shared_ptr< Type >, U >
    {};
    
    template < typename Type, typename U > struct conversions< external, std::unique_ptr< Type >, U >
    {
        using value_types = type_list<>;
        using pointer_types = type_list< U*, std::unique_ptr< U >* >;
        using lvalue_reference_types = type_list< U&, std::unique_ptr< U >& >;
        using rvalue_reference_types = type_list<>;
    };

    template < typename Type, typename U > struct conversions< external, std::unique_ptr< Type >&, U >
        : public conversions< external, std::unique_ptr< Type >, U >
    {};

    // TODO: external storage needs a lot of refactoring
    
    template < typename Container, typename Type, typename Conversions > class storage< Container, external, Type, Conversions >
        : public resettable_i
    {
    public:
        static constexpr bool is_caching = true;

        using conversions = Conversions;
        using type = Type;

        storage(Type&& instance)
            : instance_(std::move(instance))
        {}

        ~storage()
        {}

        Type& resolve(resolving_context< Container >& context)
        {
            return instance_;
        }

        bool is_resolved() const { return true; }

        void reset() override {}

    private:
        Type instance_;
    };

    template < typename Container, typename Type, typename Conversions > class storage< Container, external, Type&, Conversions >
        : public resettable_i
    {
    public:
        static constexpr bool is_caching = true;

        using conversions = Conversions;
        using type = Type;

        storage(Type& instance)
            : instance_(instance)
        {}

        ~storage()
        {}

        template < typename ContainerT > Type& resolve(resolving_context< ContainerT >& context)
        {
            return instance_;
        }

        bool is_resolved() const { return true; }

        void reset() override {}

    private:
        Type& instance_;
    };

    template < typename Container, typename Type, typename Conversions > class storage< Container, external, Type*, Conversions >
        : public resettable_i
    {
    public:
        static constexpr bool is_caching = true;

        using conversions = Conversions;
        using type = Type;

        storage(Type* instance)
            : instance_(instance)
        {}

        ~storage()
        {}

        Type* resolve(resolving_context< Container >& context)
        {
            return instance_;
        }

        bool is_resolved() const { return true; }

        void reset() override {}

    private:
        Type* instance_;
    };

    template < typename Container, typename Type, typename Conversions > class storage< Container, external, std::shared_ptr< Type >, Conversions >
        : public resettable_i
    {
    public:
        static constexpr bool is_caching = true;

        using conversions = Conversions;
        using type = Type;

        storage(std::shared_ptr< Type > instance)
            : instance_(instance)
        {}

        ~storage()
        {}

        std::shared_ptr< Type > resolve(resolving_context< Container >& context)
        {
            return instance_;
        }

        bool is_resolved() const { return true; }

        void reset() override {}

    private:
        std::shared_ptr< Type > instance_;
    };

    template < typename Container, typename Type, typename Conversions > class storage< Container, external, std::shared_ptr< Type >&, Conversions >
        : public resettable_i
    {
    public:
        static constexpr bool is_caching = true;

        using conversions = Conversions;
        using type = Type;

        storage(std::shared_ptr< Type >& instance)
            : instance_(instance)
        {}

        ~storage()
        {}

        std::shared_ptr< Type > resolve(resolving_context< Container >& context)
        {
            return instance_;
        }

        bool is_resolved() const { return true; }

        void reset() override {}

    private:
        std::shared_ptr< Type >& instance_;
    };
    
    template < typename Container, typename Type, typename Conversions > class storage< Container, external, std::unique_ptr< Type >, Conversions >
        : public resettable_i
    {
    public:
        static constexpr bool is_caching = true;

        using conversions = Conversions;
        using type = Type;

        storage(std::unique_ptr< Type >&& instance)
            : instance_(std::move(instance))
        {}

        ~storage()
        {}

        std::unique_ptr< Type >& resolve(resolving_context< Container >& context)
        {
            return instance_;
        }

        bool is_resolved() const { return true; }

        void reset() override {}

    private:
        std::unique_ptr< Type > instance_;
    };

    template < typename Container, typename Type, typename Conversions > class storage< Container, external, std::unique_ptr< Type >&, Conversions >
        : public resettable_i
    {
    public:
        static constexpr bool is_caching = true;

        using conversions = Conversions;
        using type = Type;

        storage(std::unique_ptr< Type >& instance)
            : instance_(instance)
        {}

        ~storage()
        {}

        std::unique_ptr< Type >& resolve(resolving_context< Container >& context)
        {
            return instance_;
        }

        bool is_resolved() const { return true; }

        void reset() override {}

    private:
        std::unique_ptr< Type >& instance_;
    };
}