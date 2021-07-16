#pragma once

namespace dingo
{
    class resolving_context;
 
    class constructible_i
    {
    public:
        virtual ~constructible_i() {};

        virtual void construct(resolving_context& context, int phase) = 0;
        virtual bool has_address(uintptr_t) = 0;
    };

}
