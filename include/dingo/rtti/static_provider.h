//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/rtti/rtti.h>

#include <cstdint>
#include <cstring>

namespace dingo {

template<> class rtti<static_provider> {
  public:
    struct type_descriptor {
        uint64_t hash;
        const char* name;
        mutable const char* equal_name;
    };

    /*
        Notes

        This should give correct behavior for cross-module usage, with the caveat
        of operator < () for same types always using strcmp. The trick that was used
        in operator == is not applicable, as operator < has to produce stable ordering.

        On Linux, with -rdynamic, there are no cross-module issues (could be DINGO_RDYNAMIC).
            Without rdynamic, with __attribute__ ((visibility ("default"))) there should not
            be any issues.
        
        The results of strcmp() for operator < can be cached on descriptors.
        As operator == is fine, question is if the opertor < shouldn't be removed instead.
    */

    class type_index {
        friend struct std::hash<type_index>;
      public:
        constexpr type_index(const type_descriptor* desc)
            : desc_(desc)
        {}

        bool operator<(const type_index& other) const {
            // If descs have the same address, the types are not <.
            if (desc_ == other.desc_)
                return false;

            // If hashes do not equal, names can't have the same contents.
            // As name addresses has to differ, we can use them to determine <.
            if (desc_->hash != other.desc_->hash)
                return desc_->name < other.desc_->name;

            // If hashes equal and descs addresses not, this is either
            // a across-module comparision of equal types or a hash collision.

            // TODO: what to do here, is there a similar trick possible as in operator == ?

            return strcmp(desc_->name, other.desc_->name) < 0;
        }

        bool operator==(const type_index& other) const {
            // If descs have the same address, types are the same
            if (desc_ == other.desc_)
                return true;

            // If hashes do not equal, the types can't be the same
            if (desc_->hash != other.desc_->hash)
                return false;

            // If hashes equal and descs addresses not, this is either
            // a across-module comparision of equal types or a hash collision.

            // If normalized names are the same, types are equal.
            if (desc_->equal_name == other.desc_->equal_name)
                return true;

            // Normalize the addresses if names are the same so they
            // take the fastpath next time. As we always resolve using lower address,
            // this will eventually converge for all modules, unless there really is a collision.
            if (strcmp(desc_->name, other.desc_->name) == 0) {
                other.desc_->equal_name = desc_->equal_name =
                    std::min(desc_->equal_name, other.desc_->equal_name);
                return true;
            }
         
            // Names are not the same
            return false;
        }

      private:
        const type_descriptor* desc_;
    };

    static constexpr uint64_t fnv1a(const char* value) {
        // http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-reference-source
        uint64_t hash = 0xcbf29ce484222325ull;
        while (char ch = *value++) {
            hash ^= ch;
            hash *= 0x100000001b3ull;
        }
        return hash;
    }

    template<typename T> static constexpr const char* get_type_name() {
    #if defined(__GNUG__) || defined(__clang__)
        return __PRETTY_FUNCTION__;
    #elif defined(_MSC_VER)
        return __FUNCSIG__;
    #else
        static_assert(false, "unsupported compiler");
    #endif
    }

    // TODO: constexpr in C++23
    template <typename T> static type_index get_type_index() {
        constexpr const char* name = get_type_name<T>();
        constexpr uint64_t hash = fnv1a(name);
        static type_descriptor desc { hash, name, name };
        return &desc;
    }
};
} // namespace dingo

namespace std {
    template<> struct hash<typename dingo::rtti<dingo::static_provider>::type_index> {
        size_t operator()(const typename dingo::rtti<dingo::static_provider>::type_index& value) const {
            return value.desc_->hash;
        }
    };
}
