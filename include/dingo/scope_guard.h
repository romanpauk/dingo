#pragma once

namespace dingo {
    template < typename T > class scope_guard
    {
    public:
        scope_guard(T&& code)
            : code_(std::move(code))
        {}

        ~scope_guard() noexcept(false) { code_(); }

    private:
        T code_;
    };

    template < typename T > scope_guard< T > make_scope_guard(T&& code) { return scope_guard< T >(std::forward< T >(code)); }
}
