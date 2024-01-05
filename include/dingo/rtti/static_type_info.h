//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

namespace dingo {
class static_type_info {
    template <typename T> struct static_type_index {
        static constexpr size_t tag = 0;
    };

  public:
    // A faster variant of std::type_index that can be obtained without calling
    // typeid().
    class type_index {
      public:
        constexpr type_index(size_t value) : value_(value) {}

        constexpr bool operator<(const type_index& other) const {
            return value_ < other.value_;
        }

        constexpr bool operator==(const type_index& other) const {
            return value_ == other.value_;
        }

        struct hasher {
            size_t operator()(const type_index& index) const {
                return std::hash<size_t>()(index.value_);
            }
        };

      private:
        size_t value_;
    };

    template <typename T> static constexpr type_index get_type_index() {
        return reinterpret_cast<size_t>(&static_type_index<T>::tag);
    }
};
} // namespace dingo