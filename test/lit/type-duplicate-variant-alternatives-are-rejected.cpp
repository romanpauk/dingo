// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/type/type_traits.h>

using alternatives =
    typename dingo::detail::alternative_type_alternatives<
        std::variant<int, int>>::type;

int main() {
    return sizeof(alternatives);
}

// CHECK: alternative_type_traits<T>::alternatives must not contain duplicate types
