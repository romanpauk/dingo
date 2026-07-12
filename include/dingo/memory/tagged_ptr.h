//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace dingo {

template <typename T, std::size_t TagBits = 1> class tagged_ptr {
  static_assert(TagBits < std::numeric_limits<std::uintptr_t>::digits);

public:
  using element_type = T;
  using tag_type = std::uintptr_t;

  static constexpr tag_type tag_mask = (tag_type{1} << TagBits) - 1;

  tagged_ptr() noexcept = default;

  explicit tagged_ptr(T *pointer, tag_type tag = 0) noexcept
      : encoded_(encode(pointer, tag)) {}

  T *get() const noexcept {
    return reinterpret_cast<T *>(encoded_ & ~tag_mask);
  }

  tag_type tag() const noexcept { return encoded_ & tag_mask; }

  void set_pointer(T *pointer) noexcept { encoded_ = encode(pointer, tag()); }

  void set_tag(tag_type tag) noexcept { encoded_ = encode(get(), tag); }

  explicit operator bool() const noexcept { return get() != nullptr; }
  T &operator*() const noexcept { return *get(); }
  T *operator->() const noexcept { return get(); }

private:
  static tag_type encode(T *pointer, tag_type tag) noexcept {
    const auto address = reinterpret_cast<tag_type>(pointer);
    assert((address & tag_mask) == 0);
    assert((tag & ~tag_mask) == 0);
    return address | tag;
  }

  tag_type encoded_ = 0;
};

} // namespace dingo
