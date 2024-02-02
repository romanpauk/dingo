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
struct external {};

namespace detail {
template <typename Type, typename U> struct conversions<external, Type, U> {
    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct conversions<external, Type&, U> : public conversions<external, Type, U> {
};

template <typename Type, typename U>
struct conversions<external, Type*, U> : public conversions<external, Type, U> {
};

template <typename Type, typename U>
struct conversions<external, std::shared_ptr<Type>, U> {
    using value_types = type_list<std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<U&, std::shared_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::shared_ptr<U>*>;
    using conversion_types = type_list<std::shared_ptr<U>>;
};

template <typename Type, typename U>
struct conversions<external, std::shared_ptr<Type>&, U>
    : public conversions<external, std::shared_ptr<Type>, U> {};

template <typename Type, typename U>
struct conversions<external, std::unique_ptr<Type>, U> {
    using value_types = type_list<>;
    using pointer_types = type_list<U*, std::unique_ptr<U>*>;
    using lvalue_reference_types = type_list<U&, std::unique_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct conversions<external, std::unique_ptr<Type>&, U>
    : public conversions<external, std::unique_ptr<Type>, U> {};

template <typename Type, typename U>
struct conversions<external, std::optional<Type>, U> {
    using value_types = type_list<>;
    using pointer_types = type_list<U*, std::optional<U>*>;
    using lvalue_reference_types = type_list<U&, std::optional<U>&>;
    using rvalue_reference_types = type_list<>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct conversions<external, std::optional<Type>&, U>
    : public conversions<external, std::optional<Type>, U> {};

template <typename Type, typename StoredType>
class storage_instance<external, Type, StoredType, void> {
  public:
    template <typename T>
    storage_instance(T&& instance) : instance_(std::forward<T>(instance)) {}

    Type& get() { return instance_; }

  private:
    Type instance_;
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

template <typename Type, typename StoredType>
class storage_instance<external, std::shared_ptr<Type>,
                       std::shared_ptr<StoredType>, void> {
  public:
    template <typename T>
    storage_instance(T&& instance) : instance_(std::forward<T>(instance)) {}

    std::shared_ptr<StoredType>& get() { return instance_; }

  private:
    std::shared_ptr<StoredType> instance_;
};

template <typename Type, typename StoredType>
class storage_instance<external, std::shared_ptr<Type>&, StoredType, void> {
  public:
    storage_instance(std::shared_ptr<Type>& instance) : instance_(instance) {}

    std::shared_ptr<Type>& get() { return instance_; }

  private:
    std::shared_ptr<Type>& instance_;
};

// TODO: missing reference
template <typename Type, typename StoredType>
class storage_instance<external, std::unique_ptr<Type>,
                       std::unique_ptr<StoredType>, void> {
  public:
    template <typename T>
    storage_instance(T&& instance) : instance_(std::forward<T>(instance)) {}

    std::unique_ptr<StoredType>& get() { return instance_; }

  private:
    std::unique_ptr<StoredType> instance_;
};

template <typename Type, typename StoredType>
class storage_instance<external, std::optional<Type>&, StoredType, void> {
  public:
    template <typename T>
    storage_instance(T&& instance) : instance_(std::forward<T>(instance)) {}

    std::optional<Type>& get() { return instance_; }

  private:
    std::optional<Type> instance_;
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class storage<external, Type, StoredType, Factory, Conversions>
    : public resettable_i {
    storage_instance<external, Type, StoredType, void> instance_;

  public:
    static constexpr bool cacheable = true;

    using conversions = Conversions;
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
