//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>

namespace dingo {
namespace incomplete_resolve_test {

struct service;

struct consumer {
    explicit consumer(service& value);

    service* value;
};

void register_service(container<>& container);

} // namespace incomplete_resolve_test
} // namespace dingo
