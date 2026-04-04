//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>

#include <gtest/gtest.h>

#include "support/assert.h"
#include "support/class.h"
#include "support/containers.h"

namespace dingo {
namespace detail {
struct unique_class {
    unique_class(const unique_class&) = delete;
    unique_class(unique_class&&) {}
    unique_class(std::nullptr_t) {}
    unique_class(void*) {}
};

struct shared_class {
    shared_class(const shared_class&) {}
    shared_class(shared_class&&) {}
    shared_class(std::nullptr_t) {}
    shared_class(void*) {}
};

}

using unique_class = std::unique_ptr<int>; //detail::unique_class;
using shared_class = std::shared_ptr<int>; //detail::shared_class;

struct trait_limited_detected {
    trait_limited_detected(int, float) {}
    trait_limited_detected(int, float, double) {}
};

template <> struct constructor_detection_traits<trait_limited_detected> {
    static constexpr size_t max_arity = 2;
};

template< typename U > struct arg_rvalue_reference {
    template<typename T, typename = std::enable_if_t< !std::is_same_v< std::decay_t<T>, U > > >
    operator T&&() const { throw std::runtime_error("operator T&&()"); }
};

template< typename U > struct arg_const_lvalue_reference {
    template<typename T, typename = std::enable_if_t< !std::is_same_v< std::decay_t<T>, U > > >
    operator const T&() const { throw std::runtime_error("operator const T&()"); }
};

template< typename U > struct arg_lvalue_reference {
    template<typename T, typename = std::enable_if_t< !std::is_same_v< std::decay_t<T>, U > > >
    operator T&() const { throw std::runtime_error("operator T&()"); }
};

template< typename U > struct arg_value {
    template<typename T, typename = std::enable_if_t< !std::is_same_v< std::decay_t<T>, U > > >
    operator T() { throw std::runtime_error("operator T()"); }
};

template< typename U > struct arg_pointer {
    template<typename T, typename = std::enable_if_t< !std::is_same_v< std::decay_t<T>, U > > >
    operator T*() { throw std::runtime_error("operator T*()"); }
};

template< typename U > struct arg
    : arg_value<U>
    , arg_const_lvalue_reference<U>
    , arg_lvalue_reference<U>
    , arg_pointer<U>

    // TODO: for some compilers, a presence of rvalue reference is needed
    // so the code is compiled without ambiguity. That does not mean the operator T&&()
    // will actually be called.
#if (defined(_MSC_VER) && DINGO_CXX_STANDARD == 17) || __GNUC__ == 12 || __GNUC__ == 13
    , arg_rvalue_reference<U>
#endif
{};

#if !defined(_MSC_VER) || DINGO_CXX_VERSION > 17
TEST(constructor_detection_test, unique_arg_value) {
    struct T { T(unique_class) {} };

    try {
        T instance((arg<T>()));
    } catch(const std::runtime_error& e) {
        ASSERT_STREQ(e.what(), "operator T()");
    }
}
#endif

TEST(constructor_detection_test, unique_arg_lvalue_reference) {
    struct T { T(unique_class&) {} };

    try {
        T instance((arg<T>()));
    } catch(const std::runtime_error& e) {
        ASSERT_STREQ(e.what(), "operator T&()");
    }
}

TEST(constructor_detection_test, unique_arg_const_lvalue_reference) {
    struct T { T(const unique_class&) {} };

    try {
        T instance((arg<T>()));
    } catch(const std::runtime_error& e) {
        ASSERT_STREQ(e.what(), "operator const T&()");
    }
}

TEST(constructor_detection_test, unique_arg_rvalue_reference) {
    struct T { T(unique_class&&) {} };

    try {
        T instance((arg<T>()));
    } catch(const std::runtime_error& e) {
    #if __GNUC__ == 12 || __GNUC__ == 13
        ASSERT_STREQ(e.what(), "operator T&&()");
    #else
        ASSERT_STREQ(e.what(), "operator T()");
    #endif
    }
}

TEST(constructor_detection_test, unique_arg_pointer) {
    struct T { T(unique_class*) {} };

    try {
        T instance((arg<T>()));
    } catch(const std::runtime_error& e) {
        ASSERT_STREQ(e.what(), "operator T*()");
    }
}

#if !defined(_MSC_VER) || DINGO_CXX_VERSION > 17
TEST(constructor_detection_test, shared_arg_value) {
    struct T { T(shared_class) {} };

    try {
        T instance((arg<T>()));
    } catch(const std::runtime_error& e) {
        ASSERT_STREQ(e.what(), "operator T()");
    }
}
#endif

TEST(constructor_detection_test, shared_arg_lvalue_reference) {
    struct T { T(shared_class&) {} };

    try {
        T instance((arg<T>()));
    } catch(const std::runtime_error& e) {
        ASSERT_STREQ(e.what(), "operator T&()");
    }
}

TEST(constructor_detection_test, shared_arg_const_lvalue_reference) {
    struct T { T(const shared_class&) {} };

    try {
        T instance((arg<T>()));
    } catch(const std::runtime_error& e) {
        ASSERT_STREQ(e.what(), "operator const T&()");
    }
}

TEST(constructor_detection_test, shared_arg_rvalue_reference) {
    struct T { T(shared_class&&) {} };

    try {
        T instance((arg<T>()));
    } catch(const std::runtime_error& e) {
    #if __GNUC__ == 12 || __GNUC__ == 13
        ASSERT_STREQ(e.what(), "operator T&&()");
    #else
        ASSERT_STREQ(e.what(), "operator T()");
    #endif
    }
}

TEST(constructor_detection_test, constructor_arguments_type_list) {
    struct Explicit {
        Explicit(int, float) {}
    };
    struct Detected {
        Detected(int, float) {}
    };

    static_assert(std::is_same_v<typename constructor<Explicit(int, float)>::arguments,
                                 type_list<int, float>>);
    static_assert(constructor_detection<Detected>::arity == 2);
    static_assert(constructor_detection<Detected>::kind ==
                  detail::constructor_kind::concrete);
}

TEST(constructor_detection_test, constructor_detection_traits_limit_public_search) {
    static_assert(constructor_detection<trait_limited_detected>::arity == 2);
    static_assert(constructor_detection<trait_limited_detected>::kind ==
                  detail::constructor_kind::concrete);
    static_assert(detail::constructor_detection<
                      trait_limited_detected,
                      detail::automatic,
                      detail::list_initialization,
                      3>::arity == 3);
}

}
