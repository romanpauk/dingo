//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <cassert>
#include <memory>
#include <utility>

namespace dingo {
template <typename Allocator> struct allocator_base : public Allocator {
  template <typename AllocatorT>
  allocator_base(AllocatorT &&alloc)
      : Allocator(std::forward<AllocatorT>(alloc)) {}

  Allocator &get_allocator() { return *this; }
};

struct allocator_traits {
  template <typename T, typename Allocator>
  static auto rebind(Allocator &alloc) {
    return typename std::allocator_traits<Allocator>::template rebind_alloc<T>(
        alloc);
  }

  template <typename Allocator>
  static auto allocate(Allocator &alloc, size_t n) {
    return std::allocator_traits<Allocator>::allocate(alloc, n);
  }

  template <typename Allocator, typename T, typename... Args>
  static void construct(Allocator &alloc, T *ptr, Args &&...args) {
    return std::allocator_traits<Allocator>::construct(
        alloc, ptr, std::forward<Args>(args)...);
  }

  template <typename Allocator, typename T>
  static void destroy(Allocator &alloc, T *ptr) {
    std::allocator_traits<Allocator>::destroy(alloc, ptr);
  }

  template <typename Allocator, typename T>
  static void deallocate(Allocator &alloc, T *ptr, size_t n) {
    std::allocator_traits<Allocator>::deallocate(alloc, ptr, n);
  }
};

namespace detail {
template <typename T, typename Allocator>
struct allocator_ptr
    : allocator_base<
          typename std::allocator_traits<Allocator>::template rebind_alloc<T>> {
  using allocator_type =
      typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
  using base = allocator_base<allocator_type>;

  explicit allocator_ptr(Allocator &al) : base(allocator_type(al)) {}

  template <typename... Args>
  allocator_ptr(Allocator &al, Args &&...args) : allocator_ptr(al) {
    emplace(std::forward<Args>(args)...);
  }

  ~allocator_ptr() { reset(); }

  allocator_ptr(const allocator_ptr &) = delete;
  allocator_ptr &operator=(const allocator_ptr &) = delete;

  allocator_ptr(allocator_ptr &&other) noexcept
      : base(std::move(other.get_allocator())), ptr_(other.ptr_) {
    other.ptr_ = nullptr;
  }

  allocator_ptr &operator=(allocator_ptr &&) = delete;

  explicit operator bool() const { return ptr_ != nullptr; }

  T &operator*() {
    assert(ptr_);
    return *ptr_;
  }

  template <typename... Args> void emplace(Args &&...args) {
    reset();
    auto &alloc = this->get_allocator();
    ptr_ = allocator_traits::allocate(alloc, 1);
    try {
      allocator_traits::construct(alloc, ptr_, std::forward<Args>(args)...);
    } catch (...) {
      allocator_traits::deallocate(alloc, ptr_, 1);
      ptr_ = nullptr;
      throw;
    }
  }

  void reset() {
    if (ptr_) {
      auto &alloc = this->get_allocator();
      allocator_traits::destroy(alloc, ptr_);
      allocator_traits::deallocate(alloc, ptr_, 1);
      ptr_ = nullptr;
    }
  }

private:
  T *ptr_ = nullptr;
};
} // namespace detail
} // namespace dingo
