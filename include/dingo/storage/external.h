//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/factory/constructor.h>
#include <dingo/storage/resettable.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_conversion_traits.h>

namespace dingo {
struct external {};

namespace detail {
template <typename Type, typename U> struct conversions<external, Type, U>
    : type_storage_traits<external, Type, U> {};

template <typename Type, typename U> struct conversions<external, Type&, U>
    : public type_storage_traits<external, Type&, U> {};

template <typename Type, typename U> struct conversions<external, Type*, U>
    : public type_storage_traits<external, Type*, U> {};

template <typename Type, typename StoredType, typename = void>
class external_storage_instance_impl {
  public:
    template <typename T>
    external_storage_instance_impl(T&& instance)
        : instance_(std::forward<T>(instance)) {}

    Type& get() { return instance_; }

  private:
    Type instance_;
};

template <typename Type, typename StoredType>
class external_storage_instance_impl<
    Type, StoredType, std::enable_if_t<type_traits<Type>::enabled>> {
  public:
    template <typename T>
    external_storage_instance_impl(T&& instance)
        : instance_(type_conversion_traits<StoredType, Type>::convert(
              std::forward<T>(instance))) {}

    StoredType& get() { return instance_; }

  private:
    StoredType instance_;
};

template <typename Type, typename StoredType>
class storage_instance<external, Type, StoredType, void>
    : public external_storage_instance_impl<Type, StoredType> {
  public:
    template <typename T>
    storage_instance(T&& instance)
        : external_storage_instance_impl<Type, StoredType>(
              std::forward<T>(instance)) {}
};

template <typename Type, size_t N, typename StoredType>
class storage_instance<external, Type[N], StoredType, void> {
  public:
    storage_instance(Type (&instance)[N]) : instance_(instance) {}

    Type* get() { return instance_; }

  private:
    Type (&instance_)[N];
};

template <typename Type, typename StoredType>
class storage_instance<external, Type&, StoredType, void> {
  public:
    storage_instance(Type& instance) : instance_(instance) {}

    Type& get() { return instance_; }

  private:
    Type& instance_;
};

template <typename Type, typename StoredType>
class storage_instance<external, Type*, StoredType, void> {
  public:
    storage_instance(Type* instance) : instance_(instance) {}

    Type* get() { return instance_; }

  private:
    Type* instance_;
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class storage<external, Type, StoredType, Factory, Conversions>
    : public resettable {
    storage_instance<external, Type, StoredType, void> instance_;

  public:
    static constexpr bool cacheable = true;

    using conversions = Conversions;
    using factory_type = Factory;
    using type = Type;
    using stored_type = StoredType;
    using tag_type = external;

    template <typename T>
    storage(T&& instance) : instance_(std::forward<T>(instance)) {}

    template <typename Context, typename Container>
    auto resolve(Context&, Container&) -> decltype(instance_.get()) {
        return instance_.get();
    }
    constexpr bool is_resolved() const { return true; }

    void reset() override {}
};
} // namespace detail
} // namespace dingo
