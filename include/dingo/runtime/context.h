//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/context_base.h>

namespace dingo {

class runtime_context : public detail::context_state {
  public:
    template <typename T, typename Container>
    T resolve(Container& container) {
        if constexpr (is_keyed_v<T>) {
            using request_type = keyed_type_t<T>;
            using key_type = keyed_key_t<T>;
            return T(container.template resolve<request_type, false>(
                *this, key<key_type>{}));
        } else {
            return container.template resolve<T, false>(*this);
        }
    }

    template <typename T, typename DetectionTag, typename Container>
    T construct_temporary(Container& container) {
        using temporary_type = normalized_type_t<T>;

        auto& active_closure = *closures_.back();
        arena_allocator<void> alloc(active_closure.arena_storage());
        auto allocator = allocator_traits::rebind<temporary_type>(alloc);
        auto instance = allocator_traits::allocate(allocator, 1);
        detail::default_constructor_detection<temporary_type, DetectionTag>()
            .template construct<temporary_type>(instance, *this, container);
        if constexpr (!std::is_trivially_destructible_v<temporary_type>) {
            register_destructor(instance);
        }

        if constexpr (std::is_lvalue_reference_v<T>) {
            return *instance;
        } else {
            return std::move(*instance);
        }
    }
};

} // namespace dingo
