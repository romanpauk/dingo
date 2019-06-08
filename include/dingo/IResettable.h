#pragma once

namespace dingo
{
    class IResettable
    {
    public:
        virtual ~IResettable() {};
        virtual void Reset() = 0;
    };
}