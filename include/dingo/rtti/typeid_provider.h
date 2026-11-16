//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/rtti/rtti.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <typeinfo>

namespace dingo {

template <> class rtti<typeid_provider> {
  // typeid of the wrapper keeps the cv and reference qualifiers of T.
  template <typename T> struct wrapper {};

  // The hash of the name of a type, as the compiler spells it.
  static constexpr std::size_t name_hash(std::string_view name) {
    std::uint64_t hash = 14695981039346656037ull;
    for (char c : name) {
      hash = (hash ^ static_cast<unsigned char>(c)) * 1099511628211ull;
    }
    return static_cast<std::size_t>(hash);
  }

  // The type_info of a type and its hash, as constants.
  struct type_record {
    const std::type_info *info;
    std::size_t hash;
  };

  template <typename T>
  static constexpr type_record record{
      &typeid(wrapper<T>), name_hash(detail::wrapped_type_name<T>())};

public:
  // The record of a type. Types are ordered by their hash, and by their
  // names only where the hashes are equal: the same type is equal and not
  // ordered before itself without its name compared, and one type, which
  // may have a type_info in each module, has one name and one hash.
  class type_index {
  public:
    explicit type_index(const type_record &record) noexcept
        : record_(&record) {}

    std::size_t hash_code() const noexcept { return record_->hash; }

    friend bool operator==(const type_index &left,
                           const type_index &right) noexcept {
      return left.record_ == right.record_ ||
             (left.record_->hash == right.record_->hash &&
              *left.record_->info == *right.record_->info);
    }
    friend bool operator!=(const type_index &left,
                           const type_index &right) noexcept {
      return !(left == right);
    }
    friend bool operator<(const type_index &left,
                          const type_index &right) noexcept {
      if (left.record_ == right.record_) {
        return false;
      }
      if (left.record_->hash != right.record_->hash) {
        return left.record_->hash < right.record_->hash;
      }
      return left.record_->info->before(*right.record_->info);
    }

  private:
    const type_record *record_;
  };

  template <typename T> static type_index get_type_index() {
    return type_index(record<T>);
  }
};

} // namespace dingo

namespace std {
template <> struct hash<dingo::rtti<dingo::typeid_provider>::type_index> {
  std::size_t operator()(const dingo::rtti<dingo::typeid_provider>::type_index
                             &index) const noexcept {
    return index.hash_code();
  }
};
} // namespace std
