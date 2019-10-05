#pragma once

namespace dingo {
    template < typename T > class ScopeGuard
    {
    public:
        ScopeGuard(T&& code)
            : code_(std::move(code))
        {}

        ~ScopeGuard() noexcept(false) { code_(); }

    private:
        T code_;
    };

    template < typename T > ScopeGuard< T > MakeScopeGuard(T&& code) { return ScopeGuard< T >(std::forward< T >(code)); }
}
