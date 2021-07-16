#pragma once

#include <windows.h>

namespace dingo
{
    class performance_counter
    {
    public:
        performance_counter()
        {
            elapsed_.LowPart = elapsed_.HighPart = 0;
            QueryPerformanceFrequency(&frequency_);
        }

        void start()
        {
            QueryPerformanceCounter(&begin_);
        }

        void stop()
        {
            LARGE_INTEGER end;
            QueryPerformanceCounter(&end);
            elapsed_.QuadPart += end.QuadPart - begin_.QuadPart;
        }

        unsigned get_elapsed_time(unsigned precision = 1000)
        {
            LARGE_INTEGER elapsed = elapsed_;
            elapsed.QuadPart *= precision;
            elapsed.QuadPart /= frequency_.QuadPart;
            return elapsed.LowPart;
        }

    private:
        LARGE_INTEGER frequency_, begin_, elapsed_;
    };
}
