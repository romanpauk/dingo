//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/factory/constructor.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/normalized_type.h>

namespace dingo {
struct unique {};

namespace detail {
template <typename Type, typename U> struct conversions<unique, Type, U>
    : type_storage_traits<unique, Type, U> {};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class storage<unique, Type, StoredType, Factory, Conversions> : Factory {
  public:
    template <typename... Args>
    storage(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    static constexpr bool cacheable = false;

    using conversions = Conversions;
    using type = Type;
    using stored_type = StoredType;
    using tag_type = unique;

    template <typename Context, typename Container>
    auto resolve(Context& context, Container& container) {
        return Factory::template construct<Type>(context, container);
    }

    template <typename Context, typename Container>
    void resolve(void* ptr, Context& context, Container& container) {
        Factory::template construct<Type>(ptr, context, container);
    }
};

template <typename Type, size_t N, typename StoredType, typename Factory,
          typename Conversions>
class storage<unique, Type[N], StoredType, Factory, Conversions> : Factory {
  public:
    template <typename... Args>
    storage(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    static constexpr bool cacheable = false;

    using conversions = Conversions;
    using type = Type[N];
    using stored_type = StoredType;
    using tag_type = unique;

    template <typename Context, typename Container>
    auto resolve(Context& context, Container& container) {
        return Factory::template construct<Type[N]>(context, container);
    }

    template <typename Context, typename Container>
    void resolve(void* ptr, Context& context, Container& container) {
        Factory::template construct<Type[N]>(ptr, context, container);
    }
};
} // namespace detail
} // namespace dingo
