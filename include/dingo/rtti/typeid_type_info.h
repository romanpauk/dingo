//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <typeindex>

namespace dingo {
class typeid_type_info {
  public:
    class type_index {
      public:
        type_index(std::type_index value) : value_(value) {}

        bool operator<(const type_index& other) const {
            return value_ < other.value_;
        }
        bool operator==(const type_index& other) const {
            return value_ == other.value_;
        }

        struct hasher {
            size_t operator()(const type_index& index) const {
                return std::hash<std::type_index>()(index.value_);
            }
        };

      private:
        std::type_index value_;
    };

    template <typename T> static type_index get_type_index() {
        return std::type_index(typeid(T));
    }
};

} // namespace dingo