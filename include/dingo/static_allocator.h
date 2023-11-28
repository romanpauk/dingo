//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <cassert>

namespace dingo {

template <typename T, typename Tag> class static_allocator {
  public:
    using value_type = T;

    static_allocator() noexcept {}
    template <typename U>
    static_allocator(const static_allocator<U, Tag>&) noexcept {}

    value_type* allocate(std::size_t n) {
        (void)n;
        assert(n == 1);
        if (used_)
            return nullptr;
        used_ = true;
        return reinterpret_cast<value_type*>(&storage_);
    }

    void deallocate(value_type* p, std::size_t n) noexcept {
        (void)p;
        (void)n;
        assert(used_);
        assert(n == 1);
        assert(p == reinterpret_cast<value_type*>(&storage_));
        used_ = false;
    }

  private:
    static std::aligned_storage_t<sizeof(T), alignof(T)> storage_;
    static bool used_;
};

template <typename T, typename Tag> bool static_allocator<T, Tag>::used_;
template <typename T, typename Tag>
std::aligned_storage_t<sizeof(T), alignof(T)>
    static_allocator<T, Tag>::storage_;

template <typename Allocator>
struct is_static_allocator : std::bool_constant<false> {};

template <typename T, typename Tag>
struct is_static_allocator<static_allocator<T, Tag>>
    : std::bool_constant<true> {};

template <typename Allocator>
static constexpr bool is_static_allocator_v =
    is_static_allocator<Allocator>::value;

} // namespace dingo