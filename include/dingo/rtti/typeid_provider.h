//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>
#include <dingo/rtti/rtti.h>

#include <typeindex>

namespace dingo {

template<> class rtti<typeid_provider> {
    template <typename T> struct wrapper {};

  public:
    class type_index {
        friend struct std::hash<type_index>;
      public:
        type_index(std::type_index value) : value_(value) {}

        bool operator<(const type_index& other) const {
            return value_ < other.value_;
        }
        bool operator==(const type_index& other) const {
            return value_ == other.value_;
        }

      private:       
        std::type_index value_;
    };

    template <typename T> static type_index get_type_index() {
        return std::type_index(typeid(wrapper<T>));
    }
};

} // namespace dingo

namespace std {
    template<> struct hash<typename dingo::rtti<dingo::typeid_provider>::type_index> {
        size_t operator()(const typename dingo::rtti<dingo::typeid_provider>::type_index& value) const {
            return hash<std::type_index>()(value.value_);
        }
    };
}
