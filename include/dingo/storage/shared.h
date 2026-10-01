//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/core/factory_traits.h>
#include <dingo/factory/constructor.h>
#include <dingo/memory/aligned_storage.h>
#include <dingo/memory/object_lifetime.h>
#include <dingo/resolution/resolution_operation.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/storage_scope_policy.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/normalized_type.h>

#include <new>

namespace dingo {
struct shared {};

template <> struct storage_scope_policy<shared> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;
  static constexpr bool consumes = false;
  static constexpr materialization_guard guard =
      materialization_guard::while_unresolved;
  static constexpr bool retains_unresolved_source = true;

  static constexpr storage_exposure exposed(storage_form) {
    return storage_exposure::value | storage_exposure::lvalue |
           storage_exposure::pointer;
  }
};

namespace detail {
template <typename Type, typename U>
struct conversions<shared, Type, U> : type_storage_traits<shared, Type, U> {};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
template <typename Type, typename Factory>
struct storage_instance_base : Factory {
  template <typename... Args>
  storage_instance_base(Args &&...args)
      : Factory(std::forward<Args>(args)...) {}

  Type *get() const {
    return std::launder(reinterpret_cast<Type *>(&instance_));
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  template <typename Context, typename Container>
  void construct(construction_scope scope, Context &context,
                 Container &container) {
    assert(!initialized_);
    Factory::template construct<Type *>(&instance_, scope, context, container);
    initialized_ = true;
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  bool empty() const { return !initialized_; }

protected:
  mutable dingo::aligned_storage_t<sizeof(Type), alignof(Type)> instance_;
  bool initialized_ = false;
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

template <typename Type, typename Factory,
          bool IsTriviallyDestructible = std::is_trivially_destructible_v<Type>>
struct storage_instance_dtor;

template <typename Type, typename Factory>
struct storage_instance_dtor<Type, Factory, true>
    : storage_instance_base<Type, Factory> {
  template <typename... Args>
  storage_instance_dtor(Args &&...args)
      : storage_instance_base<Type, Factory>(std::forward<Args>(args)...) {}

  void reset() { this->initialized_ = false; }
};

template <typename Type, typename Factory>
struct storage_instance_dtor<Type, Factory, false>
    : storage_instance_base<Type, Factory> {
  template <typename... Args>
  storage_instance_dtor(Args &&...args)
      : storage_instance_base<Type, Factory>(std::forward<Args>(args)...) {}

  ~storage_instance_dtor() { reset(); }

  void reset() {
    if (this->initialized_) {
      this->initialized_ = false;
      this->get()->~Type();
    }
  }
};

template <typename Type, typename StoredType, typename Factory, typename = void>
class shared_storage_instance_impl
    : public storage_instance_dtor<Type, Factory> {
public:
  template <typename... Args>
  shared_storage_instance_impl(Args &&...args)
      : storage_instance_dtor<Type, Factory>(std::forward<Args>(args)...) {}

  static_assert(
      std::is_trivially_destructible_v<Type> ==
      std::is_trivially_destructible_v<storage_instance_dtor<Type, Factory>>);
};

template <typename Type, typename StoredType, typename Factory>
class shared_storage_instance_impl<Type, StoredType, Factory,
                                   std::enable_if_t<type_traits<Type>::enabled>>
    : Factory {
public:
  template <typename... Args>
  shared_storage_instance_impl(Args &&...args)
      : Factory(std::forward<Args>(args)...) {}

  template <typename Context, typename Container>
  void construct(construction_scope scope, Context &context,
                 Container &container) {
    assert(empty());
    new (&instance_) StoredType(detail::convert_type<StoredType, consume>(
        Factory::template construct<Type>(scope, context, container)));
    initialized_ = true;
  }

  ~shared_storage_instance_impl() { reset(); }

  StoredType &get() const { return *get_ptr(); }

  void reset() {
    if (initialized_) {
      get_ptr()->~StoredType();
      initialized_ = false;
    }
  }

  bool empty() const { return !initialized_; }

private:
  StoredType *get_ptr() const {
    return std::launder(reinterpret_cast<StoredType *>(&instance_));
  }

  mutable aligned_storage_t<sizeof(StoredType), alignof(StoredType)> instance_;
  bool initialized_ = false;
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
class storage_instance<shared, Type[N], StoredType, Factory> : Factory {
public:
  template <typename... Args>
  storage_instance(Args &&...args) : Factory(std::forward<Args>(args)...) {}

  ~storage_instance() { reset(); }

  template <typename Context, typename Container>
  void construct(construction_scope scope, Context &context,
                 Container &container) {
    assert(empty());
    Factory::template construct<Type[N]>(&instance_, scope, context, container);
    initialized_ = true;
  }

  Type *get() const { return std::addressof((*get_array())[0]); }

  void reset() {
    if (!initialized_) {
      return;
    }

    destroy_object_value(*get_array());
    initialized_ = false;
  }

  bool empty() const { return !initialized_; }

private:
  Type (*get_array() const)[N] {
    return std::launder(reinterpret_cast<Type(*)[N]>(&instance_));
  }

  mutable aligned_storage_t<sizeof(Type[N]), alignof(Type[N])> instance_;
  bool initialized_ = false;
};

template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared, Type *, StoredType *, Factory> : Factory {
public:
  template <typename... Args>
  storage_instance(Args &&...args) : Factory(std::forward<Args>(args)...) {}

  ~storage_instance() { reset(); }

  template <typename Context, typename Container>
  void construct(construction_scope scope, Context &context,
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
