//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/binding_selection.h>

namespace dingo::detail {

enum class binding_resolution_policy {
    prefer_primary,
    ambiguous_on_conflict,
};

enum class binding_result {
    primary,
    secondary,
    missing,
    ambiguous,
};

constexpr binding_result resolve_binding(binding_selection_status primary,
                                         binding_selection_status secondary,
                                         binding_resolution_policy policy) {
    if (policy == binding_resolution_policy::prefer_primary) {
        if (primary == binding_selection_status::ambiguous) {
            return binding_result::ambiguous;
        }

        if (primary == binding_selection_status::found) {
            return binding_result::primary;
        }

        if (secondary == binding_selection_status::ambiguous) {
            return binding_result::ambiguous;
        }

        if (secondary == binding_selection_status::found) {
            return binding_result::secondary;
        }

        return binding_result::missing;
    }

    if (primary == binding_selection_status::ambiguous ||
        secondary == binding_selection_status::ambiguous ||
        (primary == binding_selection_status::found &&
         secondary == binding_selection_status::found)) {
        return binding_result::ambiguous;
    }

    if (primary == binding_selection_status::found) {
        return binding_result::primary;
    }

    if (secondary == binding_selection_status::found) {
        return binding_result::secondary;
    }

    return binding_result::missing;
}

constexpr binding_selection_status binding_status(binding_result resolution) {
    switch (resolution) {
    case binding_result::primary:
    case binding_result::secondary:
        return binding_selection_status::found;
    case binding_result::ambiguous:
        return binding_selection_status::ambiguous;
    case binding_result::missing:
    default:
        return binding_selection_status::not_found;
    }
}

template <binding_selection_status SecondaryStatus>
constexpr binding_selection_status
resolve_binding_status(binding_selection_status primary,
                       binding_resolution_policy policy) {
    return binding_status(resolve_binding(primary, SecondaryStatus, policy));
}

} // namespace dingo::detail
