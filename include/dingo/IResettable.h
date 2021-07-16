#pragma once

namespace dingo
{
    class IResettable
    {
    public:
        virtual ~IResettable() {};
        virtual void reset() = 0;
    };
}