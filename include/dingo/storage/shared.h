//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/factory/constructor.h>
#include <dingo/factory/factory_traits.h>
#include <dingo/resolution/resolution_operation.h>
#include <dingo/storage/destructible_slot.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/storage_scope_policy.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/normalized_type.h>

#include <new>

namespace dingo {
struct shared {};

namespace detail {
template <typename Type, typename U>
struct conversions<shared, Type, U> : type_storage_traits<shared, Type, U> {};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
template <typename Type, typename Factory>
struct storage_instance_base : Factory, destructible_slot<Type> {
  template <typename... Args>
  storage_instance_base(Args &&...args)
      : Factory(std::forward<Args>(args)...) {}

  Type *get() const { return this->slot_address(); }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  template <typename Context, typename Container>
  auto construct(construction_scope scope, Context &context,
                 Container &container) {
    assert(!this->constructed());
    Factory::template construct<Type *>(this->slot_address(), scope, context,
                                        container);
    this->mark_constructed();
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  bool empty() const { return !this->constructed(); }

  void reset() { this->destroy(); }
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

template <typename Type, typename StoredType, typename Factory, typename = void>
class shared_storage_instance_impl
    : public storage_instance_base<Type, Factory> {
public:
  template <typename... Args>
  shared_storage_instance_impl(Args &&...args)
      : storage_instance_base<Type, Factory>(std::forward<Args>(args)...) {}

  static_assert(
      std::is_trivially_destructible_v<Type> ==
      std::is_trivially_destructible_v<storage_instance_base<Type, Factory>>);
};

template <typename Type, typename StoredType, typename Factory>
class shared_storage_instance_impl<Type, StoredType, Factory,
                                   std::enable_if_t<type_traits<Type>::enabled>>
    : Factory, destructible_slot<StoredType> {
public:
  template <typename... Args>
  shared_storage_instance_impl(Args &&...args)
      : Factory(std::forward<Args>(args)...) {}

  template <typename Context, typename Container>
  auto construct(construction_scope scope, Context &context,
                 Container &container) {
    assert(this->empty());
    // When the stored type is the registered type there is no conversion to
    // perform, so let the factory's prvalue initialize the slot directly.
    if constexpr (std::is_same_v<Type, StoredType>) {
      new (this->slot_address()) StoredType(
          Factory::template construct<Type>(scope, context, container));
    } else {
      new (this->slot_address())
          StoredType(detail::convert_type<StoredType, consume>(
              Factory::template construct<Type>(scope, context, container)));
    }
    this->mark_constructed();
  }

  StoredType &get() const { return *this->slot_address(); }

  void reset() { this->destroy(); }

  bool empty() const { return !this->constructed(); }
};

template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared, Type, StoredType, Factory>
    : public shared_storage_instance_impl<Type, StoredType, Factory> {
public:
  template <typename... Args>
  storage_instance(Args &&...args)
      : shared_storage_instance_impl<Type, StoredType, Factory>(
            std::forward<Args>(args)...) {}
};

template <typename Type, size_t N, typename StoredType, typename Factory>
class storage_instance<shared, Type[N], StoredType, Factory>
    : Factory, destructible_slot<Type[N]> {
public:
  template <typename... Args>
  storage_instance(Args &&...args) : Factory(std::forward<Args>(args)...) {}

  template <typename Context, typename Container>
  auto construct(construction_scope scope, Context &context,
                 Container &container) {
    assert(empty());
    Factory::template construct<Type[N]>(this->slot_address(), scope, context,
                                         container);
    this->mark_constructed();
  }

  Type *get() const { return std::addressof((*this->slot_address())[0]); }

  void reset() { this->destroy(); }

  bool empty() const { return !this->constructed(); }
};

template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared, Type *, StoredType *, Factory> : Factory {
public:
  template <typename... Args>
  storage_instance(Args &&...args) : Factory(std::forward<Args>(args)...) {}

  ~storage_instance() { reset(); }

  template <typename Context, typename Container>
  auto construct(construction_scope scope, Context &context,
                 Container &container) {
    assert(empty());
    instance_ = Factory::template construct<Type *>(scope, context, container);
  }

  StoredType *get() const {
    return detail::convert_type<StoredType *, borrow>(instance_);
  }
  void reset() {
    delete instance_;
    instance_ = nullptr;
  }
  bool empty() const { return instance_ == nullptr; }

private:
  Type *instance_ = nullptr;
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class storage<shared, Type, StoredType, Factory, Conversions> {
  // TODO
  // static_assert(std::is_trivially_destructible_v< Type > ==
  // std::is_trivially_destructible_v< storage_instance< Type, shared > >);
  storage_instance<shared, Type, StoredType, Factory> instance_;

public:
  template <typename... Args>
  storage(Args &&...args) : instance_(std::forward<Args>(args)...) {}

  using conversions = Conversions;
  using factory_type = Factory;
  using type = Type;
  using stored_type = StoredType;
  using resolved_type =
      decltype(std::declval<
                   storage_instance<shared, Type, StoredType, Factory> &>()
                   .get());
  using tag_type = shared;

  template <typename Context, typename Container>
  decltype(auto) resolve(construction_scope scope, Context &context,
                         Container &container) {
    if (instance_.empty())
      instance_.construct(scope, context, container);
    return instance_.get();
  }

  bool is_resolved() const { return !instance_.empty(); }
  void reset() { instance_.reset(); }
};
} // namespace detail
} // namespace dingo
