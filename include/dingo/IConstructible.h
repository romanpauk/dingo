#pragma once

namespace dingo
{
    class resolving_context;
 
   class IConstructible
    {
    public:
        virtual ~IConstructible() {};

        virtual void Construct(resolving_context& context, int phase) = 0;
        virtual bool HasAddress(uintptr_t) = 0;
    };

}
