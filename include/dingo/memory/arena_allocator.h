#pragma once

#include <array>
#include <cassert>

namespace dingo
{
    template < typename T, typename Arena, typename Allocator > class arena_allocator;

    template< size_t N > class arena {
    public:
        arena(): current_(buffer_.data()) {}

        template< size_t Alignment > void* allocate(size_t bytes) {
            unsigned char* ptr = (unsigned char*)(uintptr_t(current_ + Alignment - 1) & ~(Alignment - 1));
            if (ptr + bytes < buffer_.data() + N) {
                void* p = ptr;
                current_ = ptr + bytes;
                return p;
            }
            
            return nullptr;
        }

    private:
        std::array< unsigned char, N > buffer_;
        unsigned char* current_;
    };

    template < typename T, typename Arena, typename Allocator = std::allocator< T > > class arena_allocator
        : public Allocator
    {
    public:
        using value_type = T;

        template< typename U > struct rebind
        {
            using other = arena_allocator< U,
                Arena, typename std::allocator_traits< Allocator >::template rebind_alloc< U >
            >;
        };

        template < typename... Args > arena_allocator(Arena& arena, Args&&... args) noexcept
            : arena_(&arena)
            , Allocator(std::forward< Args >(args)...)
        {}

        template < typename U, typename ArenaU, typename AllocatorU > friend class arena_allocator;

        template < typename U, typename AllocatorU > arena_allocator(const arena_allocator< U, Arena, AllocatorU >& alloc) noexcept
            : arena_(alloc.arena_)
            , Allocator(alloc)
        {}

        arena_allocator< T, Arena, Allocator >& operator = (const arena_allocator< T, Arena, Allocator >& other)
        {
            arena_ = other.arena_;
            return *this;
        }

        value_type* allocate(std::size_t n)
        {
            void *ptr = arena_->template allocate< alignof(value_type) >(sizeof(value_type) * n);
            if (ptr) {
                return reinterpret_cast<value_type*>(ptr);
            } else {
                assert(false);
                throw std::bad_alloc();
            }
        }

        void deallocate(value_type* p, std::size_t n) noexcept {}

    private:
        Arena* arena_;
    };

    template < typename T, typename U, typename Arena, typename Allocator > bool operator == (arena_allocator< T, Arena, Allocator > const& lhs, arena_allocator< U, Arena, Allocator > const& rhs) noexcept
    {
        return lhs.arena_ == rhs.arena_;
    }

    template < typename T, typename U, typename Arena, typename Allocator > bool operator != (arena_allocator< T, Arena, Allocator > const& x, arena_allocator< U, Arena, Allocator > const& y) noexcept
    {
        return !(x == y);
    }
}
