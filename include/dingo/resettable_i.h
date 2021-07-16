#pragma once

namespace dingo
{
    class resettable_i
    {
    public:
        virtual ~resettable_i() {};
        virtual void reset() = 0;
    };
}