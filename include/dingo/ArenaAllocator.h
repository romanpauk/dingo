#pragma once

namespace dingo
{
    template < typename T > class ArenaAllocator;

    class Arena
    {
        template < typename T > friend class ArenaAllocator;

    public:
        template < size_t Size > Arena(std::array< unsigned char, Size >& arena)
            : base_(arena.data())
            , current_(arena.data())
            , end_(arena.data() + arena.size())
        {}

        bool operator == (const Arena& arena) { return base_ == arena.base_; }

    private:
        unsigned char* base_;
        unsigned char* current_;
        unsigned char* end_;
    };

    template < typename T >
    class ArenaAllocator
    {
    public:
        using value_type    = T;

        ArenaAllocator(Arena& arena) noexcept
            : arena_(arena)
        {}

        template <class U> friend class ArenaAllocator;

        template <class U> ArenaAllocator(ArenaAllocator<U> const& alloc) noexcept
            : arena_(alloc.arena_)
        {}

        value_type* allocate(std::size_t n)
        {
            unsigned char* ptr = (unsigned char*)(uintptr_t(arena_.current_ + alignof(value_type) - 1) & ~(alignof(value_type) - 1));

            if (ptr + n * sizeof(value_type) < arena_.end_)
            {
                value_type* p = reinterpret_cast<value_type*>(ptr);
                ptr += n * sizeof(value_type);
                arena_.current_ = ptr;
                return p;
            }
            else
            {
            #ifdef _DEBUG
                throw TypeAllocateFailedException();
            #endif
                return static_cast<value_type*>(::operator new (n * sizeof(value_type)));
            }
        }

        void deallocate(value_type* p, std::size_t) noexcept
        {
            auto ptr = reinterpret_cast<unsigned char*>(p);
            if (ptr >= arena_.base_ && ptr < arena_.end_)
            {
                // This is from arena
            }
            else
            {
                delete p;
            }
        }

    private:
        Arena& arena_;

    };

    template <class T, class U> bool operator==(ArenaAllocator<T> const& lhs, ArenaAllocator<U> const& rhs) noexcept
    {
        return lhs.arena_ == rhs.arena_;
    }

    template <class T, class U> bool operator!=(ArenaAllocator<T> const& x, ArenaAllocator<U> const& y) noexcept
    {
        return !(x == y);
    }
}
