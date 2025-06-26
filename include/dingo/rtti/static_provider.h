//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/rtti/rtti.h>

namespace dingo {

template<> class rtti<static_provider> {
    template <typename T> struct type_index_tag {
        // TODO: This will not work across modules.
        // Should probably detect it first, handle it next.
        static constexpr size_t tag{};
    };

  public:
    class type_index {
        friend struct std::hash<type_index>;
      public:
        constexpr type_index(size_t value) : value_(value) {}

        constexpr bool operator<(const type_index& other) const {
            return value_ < other.value_;
        }

        constexpr bool operator==(const type_index& other) const {
            return value_ == other.value_;
        }

      private:
        size_t value_;
    };

    template <typename T> static constexpr type_index get_type_index() {
        return reinterpret_cast<size_t>(&type_index_tag<T>::tag);
    }
};
} // namespace dingo

namespace std {
    template<> struct hash<typename dingo::rtti<dingo::static_provider>::type_index> {
        size_t operator()(const typename dingo::rtti<dingo::static_provider>::type_index& value) const {
            return hash<size_t>()(value.value_);
        }
    };
}
