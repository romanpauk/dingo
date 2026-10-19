//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The instrumented leaves, interfaces, and user wrappers of the route probes.

#include <map>
#include <memory>
#include <type_traits>
#include <utility>

namespace probe {
struct state {
  unsigned long def = 0, copy = 0, move = 0, dtor = 0, seq = 0;
  std::map<const void *, unsigned long> live;
};
inline state &st() {
  static state &s = *new state;
  return s;
}
inline void created(const void *p) { st().live[p] = ++st().seq; }
// An address that reaches the same instance as p, such as an interface
// subobject.
inline void alias(const void *a, const void *p) { st().live[a] = st().live[p]; }
inline void destroyed(const void *p) {
  st().live.erase(p);
  ++st().dtor;
}
inline void unalias(const void *a) { st().live.erase(a); }
} // namespace probe

struct other_leaf {
  other_leaf() {}
};

struct leaf {
  leaf() {
    probe::created(this);
    ++probe::st().def;
  }
  leaf(const leaf &) {
    probe::created(this);
    ++probe::st().copy;
  }
  leaf(leaf &&) noexcept {
    probe::created(this);
    ++probe::st().move;
  }
  leaf &operator=(const leaf &) = default;
  leaf &operator=(leaf &&) = default;
  ~leaf() { probe::destroyed(this); }
};

struct move_only_leaf {
  move_only_leaf() {
    probe::created(this);
    ++probe::st().def;
  }
  move_only_leaf(const move_only_leaf &) = delete;
  move_only_leaf(move_only_leaf &&) noexcept {
    probe::created(this);
    ++probe::st().move;
  }
  move_only_leaf &operator=(const move_only_leaf &) = delete;
  move_only_leaf &operator=(move_only_leaf &&) = default;
  ~move_only_leaf() { probe::destroyed(this); }
};

// Two polymorphic interfaces and the instrumented implementation of both.
struct iface {
  virtual ~iface() = default;
  virtual int first() const = 0;
};

struct other_iface {
  virtual ~other_iface() = default;
  virtual int second() const = 0;
};

struct impl : iface, other_iface {
  impl() {
    probe::created(this);
    alias();
    ++probe::st().def;
  }
  impl(const impl &) : iface(), other_iface() {
    probe::created(this);
    alias();
    ++probe::st().copy;
  }
  impl(impl &&) noexcept : iface(), other_iface() {
    probe::created(this);
    alias();
    ++probe::st().move;
  }
  impl &operator=(const impl &) = default;
  impl &operator=(impl &&) = default;
  ~impl() override {
    probe::unalias(static_cast<other_iface *>(this));
    probe::destroyed(this);
  }
  int first() const override { return 1; }
  int second() const override { return 2; }

private:
  void alias() {
    probe::alias(static_cast<iface *>(this), this);
    probe::alias(static_cast<other_iface *>(this), this);
  }
};

// User wrappers, made known to dingo the way test/support/custom_wrappers.h
// does: a sole-owner handle, a sharing handle, and an optional-like value.
template <typename T> class user_unique;
template <typename T> class user_shared;
template <typename T> class user_optional;

template <typename T> class user_shared {
public:
  user_shared() = default;
  explicit user_shared(std::shared_ptr<T> ptr) : ptr_(std::move(ptr)) {}

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  user_shared(user_unique<U> &&other) : ptr_(std::move(other).native()) {}

  T *get() const { return ptr_.get(); }
  void reset() { ptr_.reset(); }
  explicit operator bool() const { return static_cast<bool>(ptr_); }
  const std::shared_ptr<T> &native() const & { return ptr_; }
  std::shared_ptr<T> &&native() && { return std::move(ptr_); }

private:
  std::shared_ptr<T> ptr_;
};

template <typename T> class user_unique {
public:
  user_unique() = default;
  explicit user_unique(std::unique_ptr<T> ptr) : ptr_(std::move(ptr)) {}

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  user_unique(user_unique<U> &&other) : ptr_(std::move(other).native()) {}

  T *get() const { return ptr_.get(); }
  void reset() { ptr_.reset(); }
  explicit operator bool() const { return static_cast<bool>(ptr_); }
  std::unique_ptr<T> &&native() && { return std::move(ptr_); }

private:
  std::unique_ptr<T> ptr_;
};

template <typename T> class user_optional {
public:
  user_optional() = default;
  explicit user_optional(T value) : value_(std::move(value)) {}
  template <typename... Args>
  explicit user_optional(std::in_place_t, Args &&...args)
      : value_(std::in_place, std::forward<Args>(args)...) {}
  user_optional(const user_optional &) = default;
  user_optional(user_optional &&) = default;

  user_optional &operator=(const user_optional &other) {
    if (other.value_) {
      value_.emplace(*other.value_);
    } else {
      value_.reset();
    }
    return *this;
  }

  user_optional &operator=(user_optional &&other) {
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
  std::optional<T> value_;
};
