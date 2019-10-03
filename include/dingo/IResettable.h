#pragma once

namespace dingo
{
    class IResettable
    {
    public:
        virtual ~IResettable() {};
        virtual void Reset() = 0;
    };

    // TODO: move to separate file
    class Context;
    class IConstructible
    {
    public:
        virtual ~IConstructible() {};

        virtual void Construct(Context& context, int phase) = 0;
        virtual bool HasAddress(void*) = 0;
    };
}