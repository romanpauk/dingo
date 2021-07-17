#pragma once

#include <dingo/exceptions.h>

namespace dingo
{
    template < typename T > struct class_recursion_guard
    {
        class_recursion_guard(bool enabled)
            : enabled_(enabled)
        {
            if (enabled_)
            {
                if (!visited_)
                {
                    visited_ = true;
                }
                else
                {
                    throw type_recursion_exception();
                }
            }
        }

        ~class_recursion_guard()
        {
            if (enabled_)
            {
                visited_ = false;
            }
        }

        static thread_local bool visited_;
        bool enabled_;
    };

    template < typename T > thread_local bool class_recursion_guard< T >::visited_ = false;
}