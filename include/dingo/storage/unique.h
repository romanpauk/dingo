//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/decay.h>
#include <dingo/factory/constructor.h>
#include <dingo/storage.h>
#include <dingo/type_list.h>

namespace dingo {
struct unique {};

namespace detail {
template <typename Type, typename U> struct conversions<unique, Type, U> {
    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<U&&>;
    using pointer_types = type_list<>;
};

template <typename Type, typename U> struct conversions<unique, Type*, U> {
    using value_types = type_list<std::unique_ptr<U>, std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types =
        type_list<std::unique_ptr<U>&&, std::shared_ptr<U>&&>;
    using pointer_types = type_list<U*>;
};

template <typename Type, typename U>
struct conversions<unique, std::shared_ptr<Type>, U> {
    using value_types = type_list<std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<std::shared_ptr<U>&&>;
    using pointer_types = type_list<>;
};

template <typename Type, typename U>
struct conversions<unique, std::unique_ptr<Type>, U> {
    using value_types = type_list<std::unique_ptr<U>, std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types =
        type_list<std::unique_ptr<U>&&, std::shared_ptr<U>&&>;
    using pointer_types = type_list<>;
};

template <typename Type, typename Factory, typename Conversions>
class storage<unique, Type, Factory, Conversions> : Factory {
  public:
    template <typename... Args>
    storage(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    static constexpr bool is_caching = false;

    using conversions = Conversions;
    using type = Type;

    template <typename Context, typename Container>
    Type resolve(Context& context, Container& container) {
        return Factory::template construct<Type>(context, container);
    }
};
} // namespace detail
} // namespace dingo