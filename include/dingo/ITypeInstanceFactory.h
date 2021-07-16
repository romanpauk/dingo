#pragma once

namespace dingo
{
    class resolving_context;
    class ITypeInstance;

    class ITypeInstanceFactory
    {
    public:
        virtual ~ITypeInstanceFactory() {}

        virtual ITypeInstance* Resolve(resolving_context& context) = 0;
        virtual bool IsCaching() = 0;
        virtual bool IsResolved() = 0;
    };
}
