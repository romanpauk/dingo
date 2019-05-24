#pragma once

namespace dingo
{
    template < typename T > struct TypeConstructor
    {
        template < typename... Args > static T Construct(Args&& ... args)
        {
            return T(std::forward< Args >(args)...);
        }
    };

    template < typename T > struct TypeConstructor< T* >
    {
        template < typename... Args > static T* Construct(Args&& ... args)
        {
            return new T(std::forward< Args >(args)...);
        }

        template < typename... Args > static T* Construct(void* ptr, Args&& ... args)
        {
            return new(ptr) T(std::forward< Args >(args)...);
        }
    };

    template < typename T > struct TypeConstructor< T& >
    {
        template < typename... Args > static T& Construct(Args&& ... args)
        {
            static_assert(true, "references cannot be constructed");
        }
    };

    template < typename T > struct TypeConstructor< std::unique_ptr< T > >
    {
        template < typename... Args > static std::unique_ptr< T > Construct(Args&& ... args)
        {
            return std::make_unique< T >(std::forward< Args >(args)...);
        }
    };

    template < typename T > struct TypeConstructor< std::shared_ptr< T > >
    {
        template < typename... Args > static std::shared_ptr< T > Construct(Args&& ... args)
        {
            return std::make_shared< T >(std::forward< Args >(args)...);
        }
    };

    template < typename T > struct TypeConstructor< std::optional< T > >
    {
        template < typename... Args > static std::optional< T > Construct(Args&& ... args)
        {
            return std::make_optional< T >(std::forward< Args >(args)...);
        }

        template < typename... Args > static std::optional< T >& Construct(std::optional< T >& ptr, Args&& ... args)
        {
            ptr.emplace(std::forward< Args >(args)...);
            return ptr;
        }
    };
}