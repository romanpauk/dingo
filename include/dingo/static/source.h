//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/binding_selection.h>
#include <dingo/runtime/context.h>
#include <dingo/static/registry.h>

namespace dingo::detail {

template <typename Host, typename StaticRegistry, typename Request,
          typename Key>
struct static_binding_source {
    using selection = static_binding_t<
        typename StaticRegistry::template bindings<Request, Key>>;
    static constexpr bool can_resolve =
        selection::status == binding_selection_status::found;

    Host& host;

    constexpr binding_selection_status status() const {
        return selection::status;
    }

    template <typename ResolveRequest>
    decltype(auto) resolve(runtime_context& context) {
        return host.template resolve_static_source<Request, Key>(context);
    }
};

} // namespace dingo::detail
