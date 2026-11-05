//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/factory/constructor.h>
#include <dingo/memory/aligned_storage.h>
#include <dingo/memory/object_lifetime.h>
#include <dingo/type/normalized_type.h>

#include <new>
#include <type_traits>

namespace dingo {
// Describes a storage scope. A scope is a tag type and one specialization of
// scope_traits; see scope_traits.h for its members.
template <typename StorageTag> struct scope_traits {
  static constexpr bool enabled = false;
};

// Whether a scope hands its instances over for every request.
template <typename StorageTag>
inline constexpr bool scope_consumes_v = [] {
  if constexpr (scope_traits<StorageTag>::enabled) {
    return scope_traits<StorageTag>::consumes;
  } else {
    return false;
  }
}();

namespace detail {
template <typename StorageTag, typename = void>
struct scope_allows_cycles : std::false_type {};

template <typename StorageTag>
struct scope_allows_cycles<
    StorageTag, std::void_t<decltype(scope_traits<StorageTag>::allows_cycles)>>
    : std::bool_constant<scope_traits<StorageTag>::allows_cycles> {};
} // namespace detail

// Whether the bindings of a scope may take part in a dependency cycle; a scope
// declares allows_cycles to permit it.
template <typename StorageTag>
inline constexpr bool scope_allows_cycles_v =
    detail::scope_allows_cycles<StorageTag>::value;

namespace detail {
// The requests a scope publishes by default, from the scope_traits of the
// scope.
template <typename StorageTag, typename Type, typename U> struct conversions;

// The storage of a scope for a registered type.
template <typename StorageTag, typename Type, typename StoredType,
          typename Factory, typename Conversions>
using storage =
    typename scope_traits<StorageTag>::template storage<Type, StoredType,
                                                        Factory, Conversions>;

template <typename StorageTag, typename Type, typename StoredType,
          typename Factory>
class storage_instance;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

// The value of a storage instance, constructed in place once. A value that
// needs destruction is destroyed with the instance; a trivially destructible
// one keeps the instance trivially destructible.
template <typename Value, bool = std::is_trivially_destructible_v<Value>>
class shared_value {
public:
  bool empty() const { return !initialized_; }

  void reset() {
    if (initialized_) {
      initialized_ = false;
      destroy_object_value(*pointer());
    }
  }

protected:
  void *storage() const { return &buffer_; }
  void constructed() { initialized_ = true; }

  Value *pointer() const {
    return std::launder(reinterpret_cast<Value *>(&buffer_));
  }

private:
  mutable aligned_storage_t<sizeof(Value), alignof(Value)> buffer_;
  bool initialized_ = false;
};

template <typename Value>
class shared_value<Value, false> : public shared_value<Value, true> {
public:
  shared_value() = default;
  ~shared_value() { this->reset(); }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace detail
} // namespace dingo
