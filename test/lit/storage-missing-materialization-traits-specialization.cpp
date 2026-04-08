// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/storage/type_storage_traits.h>

struct custom_storage_tag {};

int main() {
    return dingo::storage_materialization_traits<custom_storage_tag, int>::
        preserves_closure(0);
}

// CHECK: storage_materialization_traits must be specialized for this storage tag
