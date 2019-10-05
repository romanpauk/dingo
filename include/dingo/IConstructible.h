#pragma once

namespace dingo
{
    class Context;
 
   class IConstructible
    {
    public:
        virtual ~IConstructible() {};

        virtual void Construct(Context& context, int phase) = 0;
        virtual bool HasAddress(uintptr_t) = 0;
    };

}
