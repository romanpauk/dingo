//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/type_traits.h>

#include <gtest/gtest.h>

#include <optional>

#include "support/class.h"

namespace dingo {
template <typename T> class test_unique;
template <typename T> class test_optional;

template <typename T> class test_shared {
  public:
    test_shared() = default;
    explicit test_shared(std::shared_ptr<T> ptr) : ptr_(std::move(ptr)) {}

    template <typename U,
              typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    test_shared(test_unique<U>&& other) : ptr_(std::move(other.ptr_)) {}

    T* get() const { return ptr_.get(); }
    void reset() { ptr_.reset(); }
    explicit operator bool() const { return static_cast<bool>(ptr_); }

  private:
    template <typename> friend class test_shared;
    template <typename> friend class test_unique;
    template <typename, typename, typename> friend struct type_conversion_traits;

    std::shared_ptr<T> ptr_;
};

template <typename T> class test_unique {
  public:
    test_unique() = default;
    explicit test_unique(std::unique_ptr<T> ptr) : ptr_(std::move(ptr)) {}

    template <typename U,
              typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    test_unique(test_unique<U>&& other) : ptr_(std::move(other.ptr_)) {}

    T* get() const { return ptr_.get(); }
    void reset() { ptr_.reset(); }
    explicit operator bool() const { return static_cast<bool>(ptr_); }

  private:
    template <typename> friend class test_unique;
    template <typename> friend class test_shared;

    std::unique_ptr<T> ptr_;
};

template <typename T> class test_optional {
  public:
    test_optional() = default;
    explicit test_optional(T value) : value_(std::move(value)) {}
    test_optional(const test_optional&) = default;
    test_optional(test_optional&&) = default;

    test_optional& operator=(const test_optional& other) {
        if (other.value_) {
            value_.emplace(*other.value_);
        } else {
            value_.reset();
        }
        return *this;
    }

    test_optional& operator=(test_optional&& other) {
        if (other.value_) {
            value_.emplace(std::move(*other.value_));
        } else {
            value_.reset();
        }
        return *this;
    }

    T* get() { return value_ ? std::addressof(*value_) : nullptr; }
    const T* get() const { return value_ ? std::addressof(*value_) : nullptr; }
    void reset() { value_.reset(); }
    bool has_value() const { return value_.has_value(); }

  private:
    template <typename> friend class test_optional;
    template <typename, typename, typename> friend struct type_conversion_traits;

    std::optional<T> value_;
};

template <typename T> struct type_traits<test_shared<T>> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = true;
    static constexpr bool is_value_borrowable = true;

    template <typename>
    static constexpr bool is_handle_rebindable = true;

    using value_type = T;

    template <typename U> using rebind_t = test_shared<U>;

    static T* get(test_shared<T>& wrapper) { return wrapper.get(); }
    static const T* get(const test_shared<T>& wrapper) { return wrapper.get(); }
    static T& borrow(test_shared<T>& wrapper) { return *wrapper.get(); }
    static const T& borrow(const test_shared<T>& wrapper) {
        return *wrapper.get();
    }
    static bool empty(const test_shared<T>& wrapper) { return !wrapper.get(); }
    static void reset(test_shared<T>& wrapper) { wrapper.reset(); }

    template <typename... Args> static test_shared<T> make(Args&&... args) {
        return test_shared<T>(
            std::shared_ptr<T>(new T{std::forward<Args>(args)...}));
    }

    static test_shared<T> from_pointer(T* ptr) {
        return test_shared<T>(std::shared_ptr<T>(ptr));
    }

    template <typename TargetWrapper, typename Factory, typename Context>
    static TargetWrapper& resolve_type(Factory& factory, Context& context,
                                       type_descriptor, type_descriptor) {
        return factory.template resolve<TargetWrapper>(context);
    }
};

template <typename T> struct type_traits<test_unique<T>> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = true;
    static constexpr bool is_value_borrowable = true;
    static constexpr bool is_reference_resolvable = true;

    template <typename>
    static constexpr bool is_handle_rebindable = true;

    using value_type = T;

    template <typename U> using rebind_t = test_unique<U>;

    static T* get(test_unique<T>& wrapper) { return wrapper.get(); }
    static const T* get(const test_unique<T>& wrapper) { return wrapper.get(); }
    static T& borrow(test_unique<T>& wrapper) { return *wrapper.get(); }
    static const T& borrow(const test_unique<T>& wrapper) {
        return *wrapper.get();
    }
    static bool empty(const test_unique<T>& wrapper) { return !wrapper.get(); }
    static void reset(test_unique<T>& wrapper) { wrapper.reset(); }

    template <typename... Args> static test_unique<T> make(Args&&... args) {
        return test_unique<T>(
            std::unique_ptr<T>(new T{std::forward<Args>(args)...}));
    }

    static test_unique<T> from_pointer(T* ptr) {
        return test_unique<T>(std::unique_ptr<T>(ptr));
    }

    template <typename TargetWrapper, typename Factory, typename Context>
    static TargetWrapper& resolve_type(Factory& factory, Context& context,
                                       type_descriptor requested_type,
                                       type_descriptor registered_type) {
        if constexpr (std::is_same_v<TargetWrapper, rebind_t<T>>)
            return factory.resolve(context);
        else
            throw detail::make_type_not_convertible_exception(requested_type,
                                                              registered_type,
                                                              context);
    }
};

template <typename T> struct type_traits<test_optional<T>> {
    static constexpr bool enabled = true;
    static constexpr bool is_pointer_like = false;
    static constexpr bool is_value_borrowable = true;

    template <typename>
    static constexpr bool is_handle_rebindable = false;

    using value_type = T;

    template <typename U> using rebind_t = test_optional<U>;

    static T* get(test_optional<T>& wrapper) { return wrapper.get(); }
    static const T* get(const test_optional<T>& wrapper) {
        return wrapper.get();
    }
    static T& borrow(test_optional<T>& wrapper) { return *wrapper.get(); }
    static const T& borrow(const test_optional<T>& wrapper) {
        return *wrapper.get();
    }
    static bool empty(const test_optional<T>& wrapper) {
        return !wrapper.has_value();
    }
    static void reset(test_optional<T>& wrapper) { wrapper.reset(); }

    template <typename... Args> static test_optional<T> make(Args&&... args) {
        if constexpr (std::is_constructible_v<T, Args...>)
            return test_optional<T>(T(std::forward<Args>(args)...));
        else
            return test_optional<T>(T{std::forward<Args>(args)...});
    }

    template <typename TargetWrapper, typename Factory, typename Context>
    static TargetWrapper& resolve_type(Factory& factory, Context& context,
                                       type_descriptor requested_type,
                                       type_descriptor registered_type) {
        if constexpr (std::is_same_v<TargetWrapper, rebind_t<T>>)
            return factory.resolve(context);
        else
            throw detail::make_type_not_convertible_exception(requested_type,
                                                              registered_type,
                                                              context);
    }
};

template <typename T, typename U>
struct storage_traits<unique, test_shared<T>, U> {
    static constexpr bool enabled = true;

    using value_types = type_list<test_shared<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<test_shared<U>&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<test_shared<U>>;
};

template <typename T, typename U>
struct storage_traits<shared, test_shared<T>, U> {
    static constexpr bool enabled = true;

    using value_types = type_list<U, test_shared<U>>;
    using lvalue_reference_types = type_list<U&, test_shared<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, test_shared<U>*>;
    using conversion_types = type_list<test_shared<U>>;
};

template <typename T, typename U>
struct storage_traits<external, test_shared<T>, U> {
    static constexpr bool enabled = true;

    using value_types = type_list<test_shared<U>>;
    using lvalue_reference_types = type_list<U&, test_shared<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, test_shared<U>*>;
    using conversion_types = type_list<test_shared<U>>;
};

template <typename T, typename U>
struct storage_traits<unique, test_unique<T>, U> {
    static constexpr bool enabled = true;

    using value_types = type_list<test_unique<U>, test_shared<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types =
        type_list<test_unique<U>&&, test_shared<U>&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<test_unique<U>, test_shared<U>>;
};

template <typename T, typename U>
struct storage_traits<shared, test_unique<T>, U> {
    static constexpr bool enabled = true;

    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&, test_unique<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, test_unique<U>*>;
    using conversion_types = type_list<>;
};

template <typename T, typename U>
struct storage_traits<external, test_unique<T>, U> {
    static constexpr bool enabled = true;

    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&, test_unique<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, test_unique<U>*>;
    using conversion_types = type_list<>;
};

template <typename T, typename U>
struct storage_traits<unique, test_optional<T>, U> {
    static constexpr bool enabled = true;

    using value_types = type_list<test_optional<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<test_optional<U>&&>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<test_optional<U>>;
};

template <typename T, typename U>
struct storage_traits<shared, test_optional<T>, U> {
    static constexpr bool enabled = true;

    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&, exact_lookup<test_optional<T>>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, exact_lookup<test_optional<T>>*>;
    using conversion_types = type_list<>;
};

template <typename T, typename U>
struct storage_traits<external, test_optional<T>, U> {
    static constexpr bool enabled = true;

    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&, exact_lookup<test_optional<T>>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, exact_lookup<test_optional<T>>*>;
    using conversion_types = type_list<>;
};

template <typename T, typename U>
struct type_conversion_traits<test_shared<U>, test_shared<T>> {
    static test_shared<U> convert(const test_shared<T>& source) {
        return test_shared<U>(std::static_pointer_cast<U>(source.ptr_));
    }

    static test_shared<U> convert(test_shared<T>&& source) {
        return test_shared<U>(std::static_pointer_cast<U>(std::move(source.ptr_)));
    }
};

template <typename T, typename U>
struct type_conversion_traits<test_optional<U>, test_optional<T>> {
    static test_optional<U> convert(const test_optional<T>& source) {
        test_optional<U> target;
        if (source.value_)
            target.value_.emplace(*source.value_);
        return target;
    }

    static test_optional<U> convert(test_optional<T>&& source) {
        test_optional<U> target;
        if (source.value_)
            target.value_.emplace(std::move(*source.value_));
        return target;
    }
};

static_assert(is_interface_storage_rebindable_v<test_shared<Class>, IClass>);
static_assert(is_interface_storage_rebindable_v<test_unique<Class>, IClass>);

TEST(type_traits_test, shared_storage_uses_custom_shared_wrapper) {
    container<> container;
    container.template register_type<scope<shared>, storage<test_shared<Class>>,
                                     interfaces<IClass>>();

    auto& shared = container.template resolve<test_shared<IClass>&>();
    ASSERT_NE(shared.get(), nullptr);
    EXPECT_EQ(shared.get(), container.template resolve<IClass*>());

    auto copy = container.template resolve<test_shared<IClass>>();
    ASSERT_NE(copy.get(), nullptr);
    EXPECT_EQ(copy.get(), shared.get());
}

TEST(type_traits_test, external_storage_uses_custom_shared_wrapper) {
    container<> container;
    test_shared<Class> shared =
        type_traits<test_shared<Class>>::make();

    container
        .template register_type<scope<external>, storage<test_shared<Class>&>,
                                interfaces<IClass>>(shared);

    auto resolved = container.template resolve<test_shared<IClass>>();
    ASSERT_NE(resolved.get(), nullptr);
    EXPECT_EQ(resolved.get(), shared.get());
    EXPECT_EQ(shared.get(), container.template resolve<IClass*>());
}

TEST(type_traits_test, external_value_storage_uses_type_conversion_traits) {
    container<> container;
    auto shared = type_traits<test_shared<Class>>::make();
    auto expected = shared.get();

    container
        .template register_type<scope<external>, storage<test_shared<Class>>,
                                interfaces<IClass>>(std::move(shared));

    auto resolved = container.template resolve<test_shared<IClass>>();
    ASSERT_NE(resolved.get(), nullptr);
    EXPECT_EQ(resolved.get(), expected);
    EXPECT_EQ(expected, container.template resolve<IClass*>());
}

TEST(type_traits_test, unique_storage_uses_custom_unique_wrapper) {
    container<> container;
    container.template register_type<scope<unique>, storage<test_unique<Class>>,
                                     interfaces<IClass>>();

    auto unique = container.template resolve<test_unique<IClass>>();
    ASSERT_NE(unique.get(), nullptr);

    auto shared = container.template resolve<test_shared<IClass>>();
    ASSERT_NE(shared.get(), nullptr);
}

TEST(type_traits_test, shared_storage_uses_custom_optional_wrapper) {
    container<> container;
    container
        .template register_type<scope<shared>, storage<test_optional<Class>>,
                                interfaces<Class>>();

    auto& optional = container.template resolve<test_optional<Class>&>();
    ASSERT_NE(optional.get(), nullptr);
    EXPECT_EQ(optional.get()->GetTag(), 0u);
    EXPECT_EQ(container.template resolve<Class&>().GetTag(), 0u);
}

TEST(type_traits_test, external_storage_uses_custom_optional_wrapper) {
    container<> container;
    auto optional = type_traits<test_optional<Class>>::make();

    container
        .template register_type<scope<external>, storage<test_optional<Class>&>,
                                interfaces<Class>>(optional);

    auto& resolved = container.template resolve<test_optional<Class>&>();
    EXPECT_EQ(resolved.get(), optional.get());
    ASSERT_NE(resolved.get(), nullptr);
    EXPECT_EQ(resolved.get()->GetTag(), 0u);
    EXPECT_EQ(container.template resolve<Class&>().GetTag(), 0u);
}

TEST(type_traits_test, external_value_storage_uses_custom_optional_wrapper) {
    container<> container;
    auto optional = type_traits<test_optional<Class>>::make();

    container
        .template register_type<scope<external>, storage<test_optional<Class>>,
                                interfaces<Class>>(std::move(optional));

    auto& resolved = container.template resolve<test_optional<Class>&>();
    ASSERT_NE(resolved.get(), nullptr);
    EXPECT_EQ(resolved.get()->GetTag(), 0u);
    EXPECT_EQ(container.template resolve<Class&>().GetTag(), 0u);
}

TEST(type_traits_test, unique_storage_uses_custom_optional_wrapper) {
    container<> container;
    container
        .template register_type<scope<unique>, storage<test_optional<Class>>,
                                interfaces<Class>>();

    auto optional = container.template resolve<test_optional<Class>>();
    ASSERT_NE(optional.get(), nullptr);
    EXPECT_EQ(optional.get()->GetTag(), 0u);
}
} // namespace dingo
