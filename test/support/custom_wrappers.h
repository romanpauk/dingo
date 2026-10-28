//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_traits.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace dingo {

template <typename T> class test_shared;
template <typename T> class test_unique;
template <typename T> class test_optional;
template <typename T> class test_variant;

namespace test_detail {
template <typename T, template <typename...> class Template>
struct is_instance : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_instance<Template<Args...>, Template> : std::true_type {};
} // namespace test_detail

// A minimal variant-like wrapper. It exists only to prove that a user
// alternative family can use alternative_type_traits without a type_traits
// specialization.
template <typename T> class test_variant {
public:
  test_variant() : value_(std::in_place_index<0>) {}

  test_variant(const test_variant &) = default;
  test_variant(test_variant &&) noexcept = default;
  test_variant &operator=(const test_variant &) = default;
  test_variant &operator=(test_variant &&) noexcept = default;

  template <typename Selected, typename Value>
  static test_variant wrap(Value &&value) {
    test_variant result;
    result.value_.template emplace<Selected>(std::forward<Value>(value));
    return result;
  }

  template <typename Selected> Selected *get() {
    return std::get_if<Selected>(&value_);
  }

  template <typename Selected> const Selected *get() const {
    return std::get_if<Selected>(&value_);
  }

  T *first_ptr() { return std::get_if<0>(&value_); }
  const T *first_ptr() const { return std::get_if<0>(&value_); }

private:
  std::variant<T, long> value_;
};

template <typename T> struct alternative_type_traits<test_variant<T>> {
  static constexpr bool enabled = true;

  using alternatives = type_list<T, long>;

  template <typename Source>
  using selected_type = typename alternative_type_traits<
      std::variant<T, long>>::template selected_type<Source>;

  template <typename Selected, typename Value>
  static test_variant<T> wrap(Value &&value) {
    return test_variant<T>::template wrap<Selected>(std::forward<Value>(value));
  }

  template <typename Selected> static Selected *get(test_variant<T> &value) {
    return value.template get<Selected>();
  }

  template <typename Selected>
  static const Selected *get(const test_variant<T> &value) {
    return value.template get<Selected>();
  }
};

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

template <typename T> struct type_traits<test_shared<T>> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = true;
  static constexpr bool is_owning_handle = true;
  static constexpr bool is_owning = true;
  static constexpr bool is_sharing = true;
  static constexpr bool is_nullable = true;
  static constexpr bool is_borrowable = true;
  static constexpr bool is_array = false;
  static constexpr bool is_variant = false;
  static constexpr bool can_copy_out = true;
  static constexpr bool can_take = true;

  template <typename> static constexpr bool is_handle_rebindable = true;

  using value_type = T;

  template <typename U> using rebind_t = test_shared<U>;

  static T *get(test_shared<T> &wrapper) { return wrapper.get(); }
  static const T *get(const test_shared<T> &wrapper) { return wrapper.get(); }
  static T &borrow(test_shared<T> &wrapper) { return *wrapper.get(); }
  static const T &borrow(const test_shared<T> &wrapper) {
    return *wrapper.get();
  }
  static bool empty(const test_shared<T> &wrapper) { return !wrapper.get(); }
  static void reset(test_shared<T> &wrapper) { wrapper.reset(); }

  template <typename... Args> static test_shared<T> make(Args &&...args) {
    return test_shared<T>(
        std::shared_ptr<T>(new T{std::forward<Args>(args)...}));
  }

  static test_shared<T> from_pointer(T *ptr) {
    return test_shared<T>(std::shared_ptr<T>(ptr));
  }
};

template <typename T> struct type_traits<test_unique<T>> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = true;
  static constexpr bool is_owning_handle = true;
  static constexpr bool is_owning = true;
  static constexpr bool is_sharing = false;
  static constexpr bool is_nullable = true;
  static constexpr bool is_borrowable = true;
  static constexpr bool is_array = false;
  static constexpr bool is_variant = false;
  static constexpr bool can_copy_out = true;
  static constexpr bool can_take = true;

  template <typename> static constexpr bool is_handle_rebindable = true;

  using value_type = T;

  template <typename U> using rebind_t = test_unique<U>;

  static T *get(test_unique<T> &wrapper) { return wrapper.get(); }
  static const T *get(const test_unique<T> &wrapper) { return wrapper.get(); }
  static T &borrow(test_unique<T> &wrapper) { return *wrapper.get(); }
  static const T &borrow(const test_unique<T> &wrapper) {
    return *wrapper.get();
  }
  static bool empty(const test_unique<T> &wrapper) { return !wrapper.get(); }
  static void reset(test_unique<T> &wrapper) { wrapper.reset(); }

  template <typename... Args> static test_unique<T> make(Args &&...args) {
    return test_unique<T>(
        std::unique_ptr<T>(new T{std::forward<Args>(args)...}));
  }

  static test_unique<T> from_pointer(T *ptr) {
    return test_unique<T>(std::unique_ptr<T>(ptr));
  }
};

template <typename T> struct type_traits<test_optional<T>> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = false;
  static constexpr bool is_value_borrowable = true;
  static constexpr bool is_owning_handle = false;
  static constexpr bool is_owning = true;
  static constexpr bool is_sharing = false;
  static constexpr bool is_nullable = true;
  static constexpr bool is_borrowable = true;
  static constexpr bool is_array = false;
  static constexpr bool is_variant = false;
  static constexpr bool can_copy_out = true;
  static constexpr bool can_take = false;

  template <typename> static constexpr bool is_handle_rebindable = false;

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
  static void reset(test_optional<T> &wrapper) { wrapper.reset(); }

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
struct storage_traits<unique, test_shared<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = false;

  using value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using rvalue_reference_types = type_list<test_shared<U> &&>;
  using pointer_types = type_list<>;
};

template <typename T, typename U>
struct storage_traits<shared, test_shared<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using value_types = type_list<U, test_shared<U>>;
  using lvalue_reference_types = type_list<U &, test_shared<U> &>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<U *, test_shared<U> *>;
};

template <typename T, typename U>
struct storage_traits<external, test_shared<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using value_types = type_list<test_shared<U>>;
  using lvalue_reference_types = type_list<U &, test_shared<U> &>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<U *, test_shared<U> *>;
};

template <typename T, typename U>
struct storage_traits<unique, test_unique<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = false;

  using value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using rvalue_reference_types =
      type_list<test_unique<U> &&, test_shared<U> &&>;
  using pointer_types = type_list<>;
};

template <typename T, typename U>
struct storage_traits<shared, test_unique<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using value_types = type_list<U>;
  using lvalue_reference_types = type_list<U &, test_unique<U> &>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<U *, test_unique<U> *>;
};

template <typename T, typename U>
struct storage_traits<external, test_unique<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using value_types = type_list<>;
  using lvalue_reference_types = type_list<U &, test_unique<U> &>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<U *, test_unique<U> *>;
};

template <typename T, typename U>
struct storage_traits<unique, test_optional<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = false;

  using value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using rvalue_reference_types = type_list<test_optional<U> &&>;
  using pointer_types = type_list<>;
};

template <typename T, typename U>
struct storage_traits<shared, test_optional<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using value_types = type_list<U>;
  using lvalue_reference_types =
      type_list<U &, exact_lookup<test_optional<T>> &>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<U *, exact_lookup<test_optional<T>> *>;
};

template <typename T, typename U>
struct storage_traits<external, test_optional<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using value_types = type_list<U>;
  using lvalue_reference_types =
      type_list<U &, exact_lookup<test_optional<T>> &>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<U *, exact_lookup<test_optional<T>> *>;
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
