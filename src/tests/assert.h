#pragma once

namespace dingo
{
    template< typename Type, typename NonConvertibleTypes > void AssertTypeNotConvertible(Container& container)
    {
        for_each((NonConvertibleTypes*)0, [&](auto element)
        {
            BOOST_CHECK_THROW(container.Resolve< decltype(element)::type >(), dingo::TypeNotConvertibleException);
        });
    }

    template < class T > void AssertClass(T&& cls)
    {
        BOOST_TEST(cls.GetName() == "Class");
    }
}