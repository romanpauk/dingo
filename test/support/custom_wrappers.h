//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/storage/storage_scope_policy.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_traits.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace dingo {

template <typename T> class test_shared;
template <typename T> class test_unique;
template <typename T> class test_optional;

namespace test_detail {
template <typename T, template <typename...> class Template>
struct is_instance : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_instance<Template<Args...>, Template> : std::true_type {};
} // namespace test_detail

template <typename T> class test_shared {
public:
  test_shared() = default;
  explicit test_shared(std::shared_ptr<T> ptr) : ptr_(std::move(ptr)) {}

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  test_shared(test_unique<U> &&other) : ptr_(std::move(other.ptr_)) {}

  T *get() const { return ptr_.get(); }
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
            typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  test_unique(test_unique<U> &&other) : ptr_(std::move(other.ptr_)) {}

  T *get() const { return ptr_.get(); }
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
  test_optional(const test_optional &) = default;
  test_optional(test_optional &&) = default;

  test_optional &operator=(const test_optional &other) {
    if (other.value_) {
      value_.emplace(*other.value_);
    } else {
      value_.reset();
    }
    return *this;
  }

  test_optional &operator=(test_optional &&other) {
    if (other.value_) {
      value_.emplace(std::move(*other.value_));
    } else {
      value_.reset();
    }
    return *this;
  }

  T *get() { return value_ ? std::addressof(*value_) : nullptr; }
  const T *get() const { return value_ ? std::addressof(*value_) : nullptr; }
  void reset() { value_.reset(); }
  bool has_value() const { return value_.has_value(); }

private:
  template <typename> friend class test_optional;

  std::optional<T> value_;
};

// The test wrappers are described as the standard wrappers are: by their
// semantics and their operations. Their storage request lists follow from the
// semantics.
template <typename T>
struct type_traits<test_shared<T>> : semantics::shared_owner {
  using value_type = T;
  template <typename U> using rebind_t = test_shared<U>;

  static T *get(const test_shared<T> &wrapper) { return wrapper.get(); }
  static T &borrow(const test_shared<T> &wrapper) { return *wrapper.get(); }
  static bool empty(const test_shared<T> &wrapper) { return !wrapper.get(); }

  template <typename... Args> static test_shared<T> make(Args &&...args) {
    return test_shared<T>(
        std::shared_ptr<T>(new T{std::forward<Args>(args)...}));
  }
};

template <typename T>
struct type_traits<test_unique<T>> : semantics::sole_owner {
  using value_type = T;
  template <typename U> using rebind_t = test_unique<U>;
  template <typename U> using share_t = test_shared<U>;

  static T *get(const test_unique<T> &wrapper) { return wrapper.get(); }
  static T &borrow(const test_unique<T> &wrapper) { return *wrapper.get(); }
  static bool empty(const test_unique<T> &wrapper) { return !wrapper.get(); }

  template <typename... Args> static test_unique<T> make(Args &&...args) {
    return test_unique<T>(
        std::unique_ptr<T>(new T{std::forward<Args>(args)...}));
  }
};

template <typename T>
struct type_traits<test_optional<T>> : semantics::optional_value {
  using value_type = T;
  template <typename U> using rebind_t = test_optional<U>;

  static T *get(test_optional<T> &wrapper) { return wrapper.get(); }
  static const T *get(const test_optional<T> &wrapper) { return wrapper.get(); }
  static T &borrow(test_optional<T> &wrapper) { return *wrapper.get(); }
  static const T &borrow(const test_optional<T> &wrapper) {
    return *wrapper.get();
  }
  static bool empty(const test_optional<T> &wrapper) {
    return !wrapper.has_value();
  }

  template <typename Value> static test_optional<T> wrap(Value &&value) {
    return test_optional<T>(T(std::forward<Value>(value)));
  }

  static test_optional<T> make_empty() { return {}; }

  template <typename... Args> static test_optional<T> make(Args &&...args) {
    if constexpr (std::is_constructible_v<T, Args...>) {
      return test_optional<T>(T(std::forward<Args>(args)...));
    } else {
      return test_optional<T>(T{std::forward<Args>(args)...});
    }
  }
};

template <typename T, typename U>
struct type_conversion_traits<
    test_shared<U>, test_shared<T>,
    std::enable_if_t<std::is_convertible_v<T *, U *>>> {
  template <typename Source>
  using required_access =
      std::conditional_t<std::is_rvalue_reference_v<Source>, consume, borrow>;

  static test_shared<U> convert(const test_shared<T> &source) {
    return test_shared<U>(std::static_pointer_cast<U>(source.ptr_));
  }

  static test_shared<U> convert(test_shared<T> &&source) {
    return test_shared<U>(std::static_pointer_cast<U>(std::move(source.ptr_)));
  }
};

} // namespace dingo
