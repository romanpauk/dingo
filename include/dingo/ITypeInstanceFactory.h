#pragma once

namespace dingo
{
    class Context;
    class ITypeInstance;

    class ITypeInstanceFactory
    {
    public:
        virtual ~ITypeInstanceFactory() {}

        virtual ITypeInstance* Resolve(Context& context) = 0;
        virtual bool IsCaching() = 0;
        virtual bool IsResolved() = 0;
    };
}
