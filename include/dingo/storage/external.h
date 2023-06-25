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
        typedef type_list<> ValueTypes;
        typedef type_list< U& > LvalueReferenceTypes;
        typedef type_list<> RvalueReferenceTypes;
        typedef type_list< U* > PointerTypes;
    };

    template < typename Type, typename U > struct conversions< external, Type&, U >
        : public conversions< external, Type, U >
    {};

    template < typename Type, typename U > struct conversions< external, Type*, U >
        : public conversions< external, Type, U >
    {};

    template < typename Type, typename U > struct conversions< external, std::shared_ptr< Type >, U >
    {
        typedef type_list< std::shared_ptr< U > > ValueTypes;
        typedef type_list< U&, std::shared_ptr< U >& > LvalueReferenceTypes;
        typedef type_list<> RvalueReferenceTypes;
        typedef type_list< U*, std::shared_ptr< U >* > PointerTypes;
    };

    template < typename Type, typename U > struct conversions< external, std::shared_ptr< Type >&, U >
        : public conversions< external, std::shared_ptr< Type >, U >
    {};
    
    template < typename Type, typename U > struct conversions< external, std::unique_ptr< Type >, U >
    {
        typedef type_list<> ValueTypes;
        typedef type_list< U*, std::unique_ptr< U >* > PointerTypes;
        typedef type_list< U&, std::unique_ptr< U >& > LvalueReferenceTypes;
        typedef type_list<> RvalueReferenceTypes;
    };

    template < typename Type, typename U > struct conversions< external, std::unique_ptr< Type >&, U >
        : public conversions< external, std::unique_ptr< Type >, U >
    {};

    template < typename Container, typename TypeT, typename ConversionsT > class storage< Container, external, TypeT, ConversionsT >
        : public resettable_i
    {
    public:
        static const bool IsCaching = true;

        using Conversions = ConversionsT;
        using Type = TypeT;

        storage(Type&& instance)
            : instance_(std::move(instance))
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
        Type instance_;
    };

    template < typename Container, typename TypeT, typename ConversionsT > class storage< Container, external, TypeT&, ConversionsT >
        : public resettable_i
    {
    public:
        static const bool IsCaching = true;

        using Conversions = ConversionsT;
        using Type = TypeT;

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

    template < typename Container, typename TypeT, typename ConversionsT > class storage< Container, external, TypeT*, ConversionsT >
        : public resettable_i
    {
    public:
        static const bool IsCaching = true;

        using Conversions = ConversionsT;
        using Type = TypeT;

        storage(Type* instance)
            : instance_(instance)
        {}

        ~storage()
        {}

        template < typename ContainerT > Type* resolve(resolving_context< ContainerT >& context)
        {
            return instance_;
        }

        bool is_resolved() const { return true; }

        void reset() override {}

    private:
        Type* instance_;
    };

    template < typename Container, typename TypeT, typename ConversionsT > class storage< Container, external, std::shared_ptr< TypeT >, ConversionsT >
        : public resettable_i
    {
    public:
        static const bool IsCaching = true;

        using Conversions = ConversionsT;
        using Type = TypeT;

        storage(std::shared_ptr< Type > instance)
            : instance_(instance)
        {}

        ~storage()
        {}

        template < typename ContainerT > std::shared_ptr< Type > resolve(resolving_context< ContainerT >& context)
        {
            return instance_;
        }

        bool is_resolved() const { return true; }

        void reset() override {}

    private:
        std::shared_ptr< Type > instance_;
    };

    template < typename Container, typename TypeT, typename ConversionsT > class storage< Container, external, std::shared_ptr< TypeT >&, ConversionsT >
        : public resettable_i
    {
    public:
        static const bool IsCaching = true;

        using Conversions = ConversionsT;
        using Type = TypeT;

        storage(std::shared_ptr< Type >& instance)
            : instance_(instance)
        {}

        ~storage()
        {}

        template < typename ContainerT > std::shared_ptr< Type > resolve(resolving_context< ContainerT >& context)
        {
            return instance_;
        }

        bool is_resolved() const { return true; }

        void reset() override {}

    private:
        std::shared_ptr< Type >& instance_;
    };
    
    template < typename Container, typename TypeT, typename ConversionsT > class storage< Container, external, std::unique_ptr< TypeT >, ConversionsT >
        : public resettable_i
    {
    public:
        static const bool IsCaching = true;

        using Conversions = ConversionsT;
        using Type = TypeT;

        storage(std::unique_ptr< Type >&& instance)
            : instance_(std::move(instance))
        {}

        ~storage()
        {}

        template < typename ContainerT > std::unique_ptr< Type >& resolve(resolving_context< ContainerT >& context)
        {
            return instance_;
        }

        bool is_resolved() const { return true; }

        void reset() override {}

    private:
        std::unique_ptr< Type > instance_;
    };

    template < typename Container, typename TypeT, typename ConversionsT > class storage< Container, external, std::unique_ptr< TypeT >&, ConversionsT >
        : public resettable_i
    {
    public:
        static const bool IsCaching = true;

        using Conversions = ConversionsT;
        using Type = TypeT;

        storage(std::unique_ptr< Type >& instance)
            : instance_(instance)
        {}

        ~storage()
        {}

        template < typename ContainerT > std::unique_ptr< Type >& resolve(resolving_context< ContainerT >& context)
        {
            return instance_;
        }

        bool is_resolved() const { return true; }

        void reset() override {}

    private:
        std::unique_ptr< Type >& instance_;
    };
}