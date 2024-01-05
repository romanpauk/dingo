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
    template <typename T> struct type_index_tag {
        static constexpr size_t tag{};
    };

  public:
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
            constexpr size_t operator()(const type_index& index) const {
                size_t h = index.value_;
                // MurmurHash3Mixer
                h ^= h >> 33;
                h *= 0xff51afd7ed558ccdL;
                h ^= h >> 33;
                h *= 0xc4ceb9fe1a85ec53L;
                h ^= h >> 33;
                return h;
            }
        };

      private:
        size_t value_;
    };

    template <typename T> static constexpr type_index get_type_index() {
        return reinterpret_cast<size_t>(&type_index_tag<T>::tag);
    }
};
} // namespace dingo