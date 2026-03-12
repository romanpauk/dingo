//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace dingo {

struct runtime_type;

template <typename T> struct wrapper_traits;

template <> struct wrapper_traits<void> {
    using element_type = void;

    template <typename U> using rebind = U;
    template <typename U> using owned_rebind = void;
    template <typename U> using shared_rebind = void;
    template <typename U> using optional_rebind = void;

    static constexpr bool is_indirect = false;
};

template <typename T> struct wrapper_traits {
    using element_type = T;

    template <typename U> using rebind = U;
    template <typename U> using owned_rebind = void;
    template <typename U> using shared_rebind = void;
    template <typename U> using optional_rebind = std::optional<U>;

    static constexpr bool is_indirect = false;

    static element_type* get_pointer(T& value) { return &value; }

    template <typename... Args> static T construct(Args&&... args) {
        return T{std::forward<Args>(args)...};
    }

    template <typename... Args>
    static void construct_at(void* ptr, Args&&... args) {
        new (ptr) T{std::forward<Args>(args)...};
    }
};

template <typename T> struct wrapper_traits<T*> {
    using element_type = T;

    template <typename U> using rebind = U*;
    template <typename U> using owned_rebind = std::unique_ptr<U>;
    template <typename U> using shared_rebind = std::shared_ptr<U>;
    template <typename U> using optional_rebind = void;

    static constexpr bool is_indirect = true;

    static element_type* get_pointer(T* value) { return value; }

    template <typename U> static rebind<U> adopt(U* value) { return value; }

    template <typename... Args> static T* construct(Args&&... args) {
        return new T{std::forward<Args>(args)...};
    }

    template <typename... Args>
    static void construct_at(void* ptr, Args&&... args) {
        new (ptr) T{std::forward<Args>(args)...};
    }
};

template <typename T, typename Deleter>
struct wrapper_traits<std::unique_ptr<T, Deleter>> {
    using element_type = T;

    template <typename U>
    using rebind = std::unique_ptr<U, std::default_delete<U>>;
    template <typename U> using owned_rebind = rebind<U>;
    template <typename U> using shared_rebind = std::shared_ptr<U>;
    template <typename U> using optional_rebind = void;

    static constexpr bool is_indirect = true;

    static element_type* get_pointer(std::unique_ptr<T, Deleter>& value) {
        return value.get();
    }

    static element_type* release(std::unique_ptr<T, Deleter>& value) {
        return value.release();
    }

    template <typename U> static owned_rebind<U> adopt(U* value) {
        return owned_rebind<U>(value);
    }

    template <typename... Args> static rebind<T> construct(Args&&... args) {
        return rebind<T>{new T{std::forward<Args>(args)...}};
    }

    template <typename... Args>
    static void construct_at(void* ptr, Args&&... args) {
        new (ptr) rebind<T>{new T{std::forward<Args>(args)...}};
    }
};

template <typename T> struct wrapper_traits<std::shared_ptr<T>> {
    using element_type = T;

    template <typename U> using rebind = std::shared_ptr<U>;
    template <typename U> using owned_rebind = void;
    template <typename U> using shared_rebind = rebind<U>;
    template <typename U> using optional_rebind = void;

    static constexpr bool is_indirect = true;

    static element_type* get_pointer(std::shared_ptr<T>& value) {
        return value.get();
    }

    template <typename U> static shared_rebind<U> adopt(U* value) {
        return shared_rebind<U>(value);
    }

    template <typename U, typename V, typename Deleter>
    static shared_rebind<U> adopt(V* value, Deleter deleter) {
        return shared_rebind<U>(value, std::move(deleter));
    }

    template <typename... Args> static rebind<T> construct(Args&&... args) {
        if constexpr (std::is_constructible_v<T, Args...>) {
            return std::make_shared<T>(std::forward<Args>(args)...);
        } else {
            return rebind<T>{new T{std::forward<Args>(args)...}};
        }
    }

    template <typename... Args>
    static void construct_at(void* ptr, Args&&... args) {
        new (ptr) rebind<T>{new T{std::forward<Args>(args)...}};
    }
};

template <typename T> struct wrapper_traits<std::optional<T>> {
    using element_type = T;

    template <typename U>
    using rebind = std::enable_if_t<!std::is_void_v<U> &&
                                        !std::is_reference_v<U> &&
                                        !std::is_abstract_v<U>,
                                    std::optional<U>>;
    template <typename U> using owned_rebind = void;
    template <typename U> using shared_rebind = void;
    template <typename U> using optional_rebind = rebind<U>;

    static constexpr bool is_indirect = false;

    static element_type* get_pointer(std::optional<T>& value) {
        return &value.value();
    }

    template <typename... Args> static rebind<T> construct(Args&&... args) {
        if constexpr (std::is_constructible_v<T, Args...>) {
            return rebind<T>{std::in_place, std::forward<Args>(args)...};
        } else {
            return rebind<T>{std::in_place, T{std::forward<Args>(args)...}};
        }
    }

    template <typename... Args>
    static rebind<T>& construct(rebind<T>& ptr, Args&&... args) {
        if constexpr (std::is_constructible_v<T, Args...>) {
            ptr.emplace(std::forward<Args>(args)...);
        } else {
            ptr.emplace(T{std::forward<Args>(args)...});
        }
        return ptr;
    }

    template <typename... Args>
    static void construct_at(void* ptr, Args&&... args) {
        if constexpr (std::is_constructible_v<T, Args...>) {
            new (ptr) rebind<T>{std::in_place, std::forward<Args>(args)...};
        } else {
            new (ptr) rebind<T>{std::in_place, T{std::forward<Args>(args)...}};
        }
    }
};

namespace detail {

template <typename T>
using wrapper_base_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename T>
using wrapper_element_t = typename wrapper_traits<wrapper_base_t<T>>::element_type;

template <typename T, typename U, typename = void>
struct wrapper_rebind {
    using type = void;
};

template <typename T, typename U>
struct wrapper_rebind<
    T, U,
    std::void_t<typename wrapper_traits<wrapper_base_t<T>>::template rebind<U>>> {
    using type = typename wrapper_traits<wrapper_base_t<T>>::template rebind<U>;
};

template <typename T, typename U>
using wrapper_rebind_t = typename wrapper_rebind<T, U>::type;

template <typename T, typename U>
inline constexpr bool has_wrapper_rebind_v =
    !std::is_void_v<wrapper_rebind_t<T, U>>;

template <typename T, typename U, typename = void>
struct wrapper_owned_rebind {
    using type = void;
};

template <typename T, typename U>
struct wrapper_owned_rebind<
    T, U,
    std::void_t<typename wrapper_traits<wrapper_base_t<T>>::template owned_rebind<U>>> {
    using type =
        typename wrapper_traits<wrapper_base_t<T>>::template owned_rebind<U>;
};

template <typename T, typename U>
using wrapper_owned_rebind_t = typename wrapper_owned_rebind<T, U>::type;

template <typename T, typename U>
inline constexpr bool has_wrapper_owned_rebind_v =
    !std::is_void_v<wrapper_owned_rebind_t<T, U>>;

template <typename T, typename U, typename = void>
struct wrapper_shared_rebind {
    using type = void;
};

template <typename T, typename U>
struct wrapper_shared_rebind<
    T, U,
    std::void_t<typename wrapper_traits<wrapper_base_t<T>>::template shared_rebind<U>>> {
    using type =
        typename wrapper_traits<wrapper_base_t<T>>::template shared_rebind<U>;
};

template <typename T, typename U>
using wrapper_shared_rebind_t = typename wrapper_shared_rebind<T, U>::type;

template <typename T, typename U>
inline constexpr bool has_wrapper_shared_rebind_v =
    !std::is_void_v<wrapper_shared_rebind_t<T, U>>;

template <typename T, typename U, typename = void>
struct wrapper_optional_rebind {
    using type = void;
};

template <typename T, typename U>
struct wrapper_optional_rebind<
    T, U,
    std::void_t<
        typename wrapper_traits<wrapper_base_t<T>>::template optional_rebind<U>>> {
    using type =
        typename wrapper_traits<wrapper_base_t<T>>::template optional_rebind<U>;
};

template <typename T, typename U>
using wrapper_optional_rebind_t = typename wrapper_optional_rebind<T, U>::type;

template <typename T, typename U>
inline constexpr bool has_wrapper_optional_rebind_v =
    !std::is_void_v<wrapper_optional_rebind_t<T, U>>;

template <typename T, typename U, typename = void>
struct has_wrapper_adopt : std::false_type {};

template <typename T, typename U>
struct has_wrapper_adopt<
    T, U,
    std::void_t<
        decltype(wrapper_traits<wrapper_base_t<T>>::template adopt<U>(
            std::declval<U*>()))>> : std::true_type {};

template <typename T, typename U>
inline constexpr bool has_wrapper_adopt_v = has_wrapper_adopt<T, U>::value;

template <typename T, typename Target, typename Source, typename Deleter,
          typename = void>
struct has_wrapper_adopt_with_deleter : std::false_type {};

template <typename T, typename Target, typename Source, typename Deleter>
struct has_wrapper_adopt_with_deleter<
    T, Target, Source, Deleter,
    std::void_t<decltype(wrapper_traits<wrapper_base_t<T>>::template adopt<
        Target>(std::declval<Source*>(), std::declval<Deleter>()))>>
    : std::true_type {};

template <typename T, typename Target, typename Source, typename Deleter>
inline constexpr bool has_wrapper_adopt_with_deleter_v =
    has_wrapper_adopt_with_deleter<T, Target, Source, Deleter>::value;

template <typename T, typename = void>
struct has_wrapper_release : std::false_type {};

template <typename T>
struct has_wrapper_release<
    T,
    std::void_t<decltype(wrapper_traits<wrapper_base_t<T>>::release(
        std::declval<wrapper_base_t<T>&>()))>> : std::true_type {};

template <typename T>
inline constexpr bool has_wrapper_release_v = has_wrapper_release<T>::value;

template <typename Source, typename Target, typename = void>
struct can_adopt_released_wrapper : std::false_type {};

template <typename Source, typename Target>
struct can_adopt_released_wrapper<
    Source, Target,
    std::void_t<decltype(wrapper_traits<wrapper_base_t<Target>>::template adopt<
        wrapper_element_t<Target>>(
        wrapper_traits<wrapper_base_t<Source>>::release(
            std::declval<wrapper_base_t<Source>&>())))>> : std::true_type {};

template <typename Source, typename Target>
inline constexpr bool can_adopt_released_wrapper_v =
    can_adopt_released_wrapper<Source, Target>::value;

template <typename T, typename... Args>
T construct_type(Args&&... args) {
    static_assert(!std::is_reference_v<T>,
                  "references cannot be constructed");
    return wrapper_traits<T>::construct(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
void construct_type_at(void* ptr, Args&&... args) {
    static_assert(!std::is_reference_v<T>,
                  "references cannot be constructed");
    wrapper_traits<T>::construct_at(ptr, std::forward<Args>(args)...);
}

} // namespace detail

} // namespace dingo
