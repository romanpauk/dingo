#pragma once

#include <dingo/config.h>

#include <memory>

namespace dingo {
template <typename Allocator> struct allocator_base : public Allocator {
    template <typename AllocatorT>
    allocator_base(AllocatorT&& alloc)
        : Allocator(std::forward<Allocator>(alloc)) {}

    Allocator& get_allocator() { return *this; }
};

struct allocator_traits {
    template <typename T, typename Allocator>
    static auto rebind(Allocator& alloc) {
        return
            typename std::allocator_traits<Allocator>::template rebind_alloc<T>(
                alloc);
    }

    template <typename Allocator>
    static auto allocate(Allocator& alloc, size_t n) {
        return std::allocator_traits<Allocator>::allocate(alloc, n);
    }

    template <typename Allocator, typename T, typename... Args>
    static void construct(Allocator& alloc, T* ptr, Args&&... args) {
        return std::allocator_traits<Allocator>::construct(
            alloc, ptr, std::forward<Args>(args)...);
    }

    template <typename Allocator, typename T>
    static void destroy(Allocator& alloc, T* ptr) {
        std::allocator_traits<Allocator>::destroy(alloc, ptr);
    }

    template <typename Allocator, typename T>
    static void deallocate(Allocator& alloc, T* ptr, size_t n) {
        std::allocator_traits<Allocator>::deallocate(alloc, ptr, n);
    }
};
} // namespace dingo