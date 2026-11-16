//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/rtti/rtti.h>

#include <cstddef>
#include <functional>
#include <typeinfo>

namespace dingo {

template <> class rtti<typeid_provider> {
  // typeid of the wrapper keeps the cv and reference qualifiers of T.
  template <typename T> struct wrapper {};

public:
  // The type_info of a type. The same type_info is equal to itself and not
  // ordered before itself without its name compared; another one is
  // compared by its name, as one type may have a type_info in each module.
  class type_index {
  public:
    explicit type_index(const std::type_info &info) noexcept : info_(&info) {}

    std::size_t hash_code() const noexcept { return info_->hash_code(); }

    friend bool operator==(const type_index &left,
                           const type_index &right) noexcept {
      return left.info_ == right.info_ || *left.info_ == *right.info_;
    }
    friend bool operator!=(const type_index &left,
                           const type_index &right) noexcept {
      return !(left == right);
    }
    friend bool operator<(const type_index &left,
                          const type_index &right) noexcept {
      return left.info_ != right.info_ && left.info_->before(*right.info_);
    }

  private:
    const std::type_info *info_;
  };

  template <typename T> static type_index get_type_index() {
    return type_index(typeid(wrapper<T>));
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
