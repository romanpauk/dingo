//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/storage/external.h>

#include "incomplete_resolve_shared.h"

namespace dingo {
namespace incomplete_resolve_test {

struct service {
    int value = 7;
};

consumer::consumer(service& dependency)
    : value(&dependency) {}

void register_service(container<>& container) {
    static service instance{};
    container.template register_type<scope<external>, storage<service&>>(
        instance);
}

} // namespace incomplete_resolve_test
} // namespace dingo
