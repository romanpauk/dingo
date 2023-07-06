#pragma once

#include <dingo/exceptions.h>

namespace dingo
{
    template < typename T > struct class_recursion_guard {
        class_recursion_guard() {
            if (!visited_) {
                visited_ = true;
            } else {
                throw type_recursion_exception();
            }
        }

        ~class_recursion_guard() {
            visited_ = false;
        }

        static bool visited_;
    };

    template < typename T > bool class_recursion_guard< T >::visited_ = false;
}