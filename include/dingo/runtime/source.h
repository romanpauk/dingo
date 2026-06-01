//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/binding_selection.h>
#include <dingo/runtime/context.h>
#include <dingo/type/request_traits.h>

#include <utility>

namespace dingo::detail {

template <typename Host, typename T, bool RemoveRvalueReferences, typename Key,
          typename Request>
struct runtime_binding_source {
    static constexpr bool can_resolve = true;

    Host& host;

    binding_selection_status status() const {
        return host.template runtime_source_status<Request, Key>();
    }

    template <typename ResolveRequest>
    decltype(auto) resolve(runtime_context& context) {
        return host.template resolve_runtime_source<T, RemoveRvalueReferences,
                                                    Key>(context);
    }
};

template <typename Host, typename T, bool CheckCache, typename IdType>
struct runtime_selected_binding_source {
    Host& host;
    IdType& id;

    decltype(auto) select() {
        return host.template runtime_source_select<T>(
            std::forward<IdType>(id));
    }

    template <typename Request, typename Selection>
    decltype(auto) resolve(runtime_context& context, Selection selection) {
        return host.template runtime_source_resolve<T, CheckCache>(
            selection, context, std::forward<IdType>(id));
    }
};

template <typename Host, typename T, bool RemoveRvalueReferences,
          bool MayAutoConstruct, typename IdType>
struct runtime_missing_binding_source {
    Host& host;
    IdType& id;

    template <typename Request>
    request_interface_t<Request> resolve(runtime_context& context) {
        return host.template runtime_source_missing<
            T, RemoveRvalueReferences, MayAutoConstruct, IdType,
            request_interface_t<Request>>(context, std::forward<IdType>(id));
    }
};

} // namespace dingo::detail
