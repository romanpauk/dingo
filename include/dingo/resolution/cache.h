//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cassert>
#include <memory>
#include <type_traits>

namespace dingo::detail::cache {
template <typename T> inline constexpr unsigned char type_key = 0;

template <typename T>
inline constexpr bool supports_v =
    std::is_lvalue_reference_v<T> || std::is_pointer_v<T>;

template <typename T> const void *key() noexcept {
  return std::addressof(type_key<T>);
}

struct sink {
  void *state = nullptr;
  void (*publish)(void *, void *) = nullptr;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  void operator()(void *address) const {
    if (publish != nullptr) {
      publish(state, address);
    }
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
};

struct entry {
  const void *key;
  void *address;
};

template <typename Storage, typename = void>
struct is_stable_storage : std::false_type {};

template <typename Storage>
struct is_stable_storage<Storage,
                         std::void_t<decltype(Storage::conversions::is_stable)>>
    : std::bool_constant<Storage::conversions::is_stable> {};

template <typename Storage>
inline constexpr bool is_stable_storage_v = is_stable_storage<Storage>::value;

template <bool Enabled> class state;

template <> class state<true> {
protected:
  entry *get() noexcept { return entry_; }
  void set(entry *value) noexcept { entry_ = value; }

private:
  entry *entry_ = nullptr;
};

template <> class state<false> {
protected:
  entry *get() noexcept { return nullptr; }
  void set(entry *value) noexcept {
    assert(value == nullptr);
    (void)value;
  }
};
} // namespace dingo::detail::cache
