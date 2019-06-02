#pragma once

namespace dingo {

    template < typename T > class ArenaAllocator;

    class Arena
    {
        template < typename T > class ArenaAllocator;

    public:
        template < size_t Size > Arena(std::array< unsigned char, Size >& arena)
            : base_(arena.data())
            , current_(arena.data())
            , end_(arena.data() + arena.size())
        {}

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
            const auto align = std::alignment_of_v< value_type >;
            unsigned char* ptr = (unsigned char*)(uintptr_t(arena_.current_ + align - 1) & ~(align - 1));

            if (ptr + n * sizeof(value_type) < arena_.end_)
            {
                value_type* p = reinterpret_cast< value_type* >(ptr);
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
            auto ptr = reinterpret_cast< unsigned char* >(p);
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

    template <class T, class U> bool operator==(ArenaAllocator<T> const&, ArenaAllocator<U> const&) noexcept
    {
        return true;
    }

    template <class T, class U> bool operator!=(ArenaAllocator<T> const& x, ArenaAllocator<U> const& y) noexcept
    {
        return !(x == y);
}
}
