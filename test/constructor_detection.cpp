//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"

namespace dingo {
template <typename T> struct constructor_detection_test : public testing::Test {};

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

template <typename T> struct movable_handle {
    using element_type = T;

    movable_handle() = default;
    explicit movable_handle(std::unique_ptr<T> handle_ptr)
        : ptr(std::move(handle_ptr)) {}
    movable_handle(movable_handle&&) = default;
    movable_handle& operator=(movable_handle&&) = default;

    movable_handle(const movable_handle&) = delete;
    movable_handle& operator=(const movable_handle&) = delete;

    std::unique_ptr<T> ptr;
};

template <typename T> struct reference_handle {
    using element_type = T;

    reference_handle() = default;
    explicit reference_handle(std::unique_ptr<T> handle_ptr)
        : ptr(std::move(handle_ptr)) {}
    reference_handle(reference_handle&&) = default;
    reference_handle& operator=(reference_handle&&) = default;

    reference_handle(const reference_handle&) = delete;
    reference_handle& operator=(const reference_handle&) = delete;

    std::unique_ptr<T> ptr;
};

template <typename T, typename Tag> struct tagged_reference_handle {
    using element_type = T;

    tagged_reference_handle() = default;
    explicit tagged_reference_handle(std::unique_ptr<T> handle_ptr)
        : ptr(std::move(handle_ptr)) {}
    tagged_reference_handle(tagged_reference_handle&&) = default;
    tagged_reference_handle& operator=(tagged_reference_handle&&) = default;

    tagged_reference_handle(const tagged_reference_handle&) = delete;
    tagged_reference_handle& operator=(const tagged_reference_handle&) = delete;

    std::unique_ptr<T> ptr;
};

struct fixed_reference_handle {
    fixed_reference_handle() = default;
    explicit fixed_reference_handle(std::unique_ptr<int> handle_ptr)
        : ptr(std::move(handle_ptr)) {}
    fixed_reference_handle(fixed_reference_handle&&) = default;
    fixed_reference_handle& operator=(fixed_reference_handle&&) = default;

    fixed_reference_handle(const fixed_reference_handle&) = delete;
    fixed_reference_handle& operator=(const fixed_reference_handle&) = delete;

    std::unique_ptr<int> ptr;
};

template <typename T, int TagValue> struct indexed_reference_handle {
    using element_type = T;

    indexed_reference_handle() = default;
    explicit indexed_reference_handle(std::unique_ptr<T> handle_ptr)
        : ptr(std::move(handle_ptr)) {}
    indexed_reference_handle(indexed_reference_handle&&) = default;
    indexed_reference_handle& operator=(indexed_reference_handle&&) = default;

    indexed_reference_handle(const indexed_reference_handle&) = delete;
    indexed_reference_handle& operator=(const indexed_reference_handle&) = delete;

    std::unique_ptr<T> ptr;
};
}

using unique_class = std::unique_ptr<int>; //detail::unique_class;
using shared_class = std::shared_ptr<int>; //detail::shared_class;

template <typename T> struct type_traits<detail::movable_handle<T>> {
    static constexpr bool enabled = true;
};

template <typename T> struct type_traits<detail::reference_handle<T>> {
    static constexpr bool enabled = true;
    static constexpr bool is_reference_resolvable = true;
};

template <typename T, typename Tag>
struct type_traits<detail::tagged_reference_handle<T, Tag>> {
    static constexpr bool enabled = true;
    static constexpr bool is_reference_resolvable = true;
};

template <>
struct type_traits<detail::fixed_reference_handle> {
    static constexpr bool enabled = true;
    static constexpr bool is_reference_resolvable = true;
};

template <typename T, int TagValue>
struct type_traits<detail::indexed_reference_handle<T, TagValue>> {
    static constexpr bool enabled = true;
    static constexpr bool is_reference_resolvable = true;
};

struct reference_value_test_container {};

struct reference_value_test_context {
    template <typename T> T resolve(reference_value_test_container&) {
        return T(std::make_unique<int>(7));
    }
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

TEST(constructor_detection_test, constructor_arguments_tuple) {
    struct Explicit {
        Explicit(int, float) {}
    };
    struct Detected {
        Detected(int, float) {}
    };

    static_assert(std::is_same_v<typename constructor<Explicit(int, float)>::arguments,
                                 std::tuple<int, float>>);
    static_assert(constructor_detection<Detected>::arity == 2);
}

TEST(constructor_detection_test, reference_detection_allows_trait_opted_in_handle_value) {
    struct T {
        T(detail::reference_handle<int>) {}
    };

    static_assert(constructor_detection<T, detail::reference>::valid);
    static_assert(constructor_detection<T, detail::reference>::arity == 1);
}

TEST(constructor_detection_test, reference_detection_allows_trait_opted_in_handle_const_lvalue_reference) {
    struct T {
        T(const detail::reference_handle<int>&) {}
    };

    static_assert(std::is_constructible_v<
                  T, detail::constructor_argument<T, detail::reference>>);
}

TEST(constructor_detection_test, reference_detection_allows_trait_opted_in_handle_lvalue_reference) {
    struct T {
        T(detail::reference_handle<int>&) {}
    };

    static_assert(std::is_constructible_v<
                  T, detail::constructor_argument<T, detail::reference>>);
}

TEST(constructor_detection_test, reference_detection_allows_trait_opted_in_handle_pointer) {
    struct T {
        T(detail::reference_handle<int>*) {}
    };

    static_assert(std::is_constructible_v<
                  T, detail::constructor_argument<T, detail::reference>>);
}

TEST(constructor_detection_test, reference_detection_rejects_handle_value_without_opt_in) {
    struct T {
        T(detail::movable_handle<int>) {}
    };

    static_assert(!std::is_constructible_v<
                  T, detail::constructor_argument<T, detail::reference>>);
}

TEST(constructor_detection_test, reference_detection_allows_multi_type_wrapper_value) {
    struct tag {};
    struct T {
        T(detail::tagged_reference_handle<int, tag>) {}
    };

    static_assert(constructor_detection<T, detail::reference>::valid);
    static_assert(constructor_detection<T, detail::reference>::arity == 1);
}

TEST(constructor_detection_test, reference_detection_allows_non_template_wrapper_value) {
    struct T {
        T(detail::fixed_reference_handle) {}
    };

    static_assert(constructor_detection<T, detail::reference>::valid);
    static_assert(constructor_detection<T, detail::reference>::arity == 1);
}

TEST(constructor_detection_test, reference_detection_allows_non_type_template_wrapper_value) {
    struct T {
        T(detail::indexed_reference_handle<int, 7>) {}
    };

    static_assert(constructor_detection<T, detail::reference>::valid);
    static_assert(constructor_detection<T, detail::reference>::arity == 1);
}

TEST(constructor_detection_test, reference_exact_argument_resolves_exact_wrapper_runtime) {
    reference_value_test_context context;
    reference_value_test_container container;
    detail::constructor_argument_impl<void, reference_value_test_context,
                                      reference_value_test_container,
                                      detail::reference_exact>
        argument(context, container);

    auto convert = [](detail::reference_handle<int> handle) {
        return handle;
    };

    auto handle = convert(argument);
    ASSERT_NE(handle.ptr, nullptr);
    ASSERT_EQ(*handle.ptr, 7);
}

}
