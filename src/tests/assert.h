#pragma once

namespace dingo
{
    template< typename Type, typename NonConvertibleTypes > void AssertTypeNotConvertible(container& container)
    {
        for_each((NonConvertibleTypes*)0, [&](auto element)
        {
            BOOST_CHECK_THROW(container.resolve< decltype(element)::type >(), dingo::TypeNotConvertibleException);
        });
    }

    template < class T > void AssertClass(T&& cls)
    {
        BOOST_TEST(cls.GetName() == "Class");
    }
}