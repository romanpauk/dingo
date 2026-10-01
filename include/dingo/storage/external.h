//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/factory/constructor.h>
#include <dingo/resolution/resolution_operation.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/storage_scope_policy.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/normalized_type.h>

namespace dingo {
struct external {};

template <> struct storage_scope_policy<external> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;
  static constexpr bool consumes = false;
  static constexpr materialization_guard guard = materialization_guard::none;
  static constexpr bool retains_unresolved_source = false;

  static constexpr storage_exposure exposed(storage_form) {
    return storage_exposure::value | storage_exposure::lvalue |
           storage_exposure::pointer;
  }
};

namespace detail {
template <typename Type, typename U>
struct conversions<external, Type, U> : type_storage_traits<external, Type, U> {
};

template <typename Type, typename U>
struct conversions<external, Type &, U>
    : public type_storage_traits<external, Type &, U> {};

template <typename Type, typename U>
struct conversions<external, Type *, U>
    : public type_storage_traits<external, Type *, U> {};

template <typename Type, typename StoredType, typename = void>
class external_storage_instance_impl {
public:
  template <typename T>
  external_storage_instance_impl(T &&instance)
      : instance_(std::forward<T>(instance)) {}

  Type &get() { return instance_; }

private:
  Type instance_;
};

template <typename Type, typename StoredType>
class external_storage_instance_impl<
    Type, StoredType, std::enable_if_t<type_traits<Type>::enabled>> {
public:
  template <typename T>
  external_storage_instance_impl(T &&instance)
      : instance_(detail::convert_type<StoredType, consume>(
            std::forward<T>(instance))) {}

  StoredType &get() { return instance_; }

private:
  StoredType instance_;
};

template <typename Type, typename StoredType>
class storage_instance<external, Type, StoredType, void>
    : public external_storage_instance_impl<Type, StoredType> {
public:
  template <typename T>
  storage_instance(T &&instance)
      : external_storage_instance_impl<Type, StoredType>(
            std::forward<T>(instance)) {}
};

template <typename Type, size_t N, typename StoredType>
class storage_instance<external, Type[N], StoredType, void> {
public:
  storage_instance(Type (&instance)[N]) : instance_(instance) {}

  Type *get() { return instance_; }

private:
  Type (&instance_)[N];
};

template <typename Type, typename StoredType>
class storage_instance<external, Type &, StoredType, void> {
public:
  storage_instance(Type &instance) : instance_(instance) {}

  Type &get() { return instance_; }

private:
  Type &instance_;
};

template <typename Type, typename StoredType>
class storage_instance<external, Type *, StoredType, void> {
public:
  storage_instance(Type *instance) : instance_(instance) {}

  Type *get() { return instance_; }

private:
  Type *instance_;
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class storage<external, Type, StoredType, Factory, Conversions> {
  storage_instance<external, Type, StoredType, void> instance_;

public:
  using conversions = Conversions;
  using type = Type;
  using stored_type = StoredType;
  using resolved_type =
      decltype(std::declval<
                   storage_instance<external, Type, StoredType, void> &>()
                   .get());
  using tag_type = external;

  template <typename T>
  storage(T &&instance) : instance_(std::forward<T>(instance)) {}

  template <typename Context, typename Container>
  decltype(auto) resolve(construction_scope, Context &, Container &) {
    return instance_.get();
  }
  constexpr bool is_resolved() const { return true; }

  void reset() {}
};
} // namespace detail
} // namespace dingo
