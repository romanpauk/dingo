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
#include <dingo/memory/aligned_storage.h>
#include <dingo/memory/object_lifetime.h>
#include <dingo/resolution/resolution_operation.h>
#include <dingo/storage/scope_traits.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/shape.h>

#include <new>

namespace dingo {
struct shared {};

namespace detail {
template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class shared_storage;
} // namespace detail

template <> struct scope_traits<shared> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;
  static constexpr bool consumes = false;
  static constexpr materialization_guard guard =
      materialization_guard::while_unresolved;
  static constexpr bool retains_unresolved_source = true;

  // The instance is constructed, so it is not a reference.
  static constexpr storage_exposure exposed(storage_form form) {
    return form == storage_form::reference
               ? storage_exposure::none
               : storage_exposure::value | storage_exposure::lvalue |
                     storage_exposure::pointer;
  }

  template <typename Type, typename StoredType, typename Factory,
            typename Conversions>
  using storage =
      detail::shared_storage<Type, StoredType, Factory, Conversions>;
};

namespace detail {

template <typename Type, typename StoredType>
using shared_value_t =
    std::conditional_t<type_traits<Type>::enabled, StoredType, Type>;

// A shared instance of the registered type, or of the stored type a wrapper
// converts to.
template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared, Type, StoredType, Factory>
    : Factory, public shared_value<shared_value_t<Type, StoredType>> {
  static constexpr bool converts = type_traits<Type>::enabled;

public:
  template <typename... Args>
  storage_instance(Args &&...args) : Factory(std::forward<Args>(args)...) {}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  template <typename Context, typename Container, typename Scope>
  void construct(Scope scope, Context &context, Container &container) {
    assert(this->empty());
    if constexpr (converts && std::is_same_v<Type, StoredType>) {
      // What the factory makes is constructed in place, as the leaf of an
      // optional that can be neither copied nor moved has to be.
      new (this->storage()) StoredType(
          Factory::template construct<Type>(scope, context, container));
    } else if constexpr (converts) {
      new (this->storage())
          StoredType(detail::convert_type<StoredType, consume>(
              Factory::template construct<Type>(scope, context, container)));
    } else if constexpr (std::is_array_v<Type>) {
      Factory::template construct<Type>(this->storage(), scope, context,
                                        container);
    } else {
      Factory::template construct<Type *>(this->storage(), scope, context,
                                          container);
    }
    this->constructed();
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  decltype(auto) get() const {
    if constexpr (converts) {
      return *this->pointer();
    } else {
      return this->pointer();
    }
  }
};

template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared, Type *, StoredType *, Factory> : Factory {
public:
  template <typename... Args>
  storage_instance(Args &&...args) : Factory(std::forward<Args>(args)...) {}

  ~storage_instance() { reset(); }

  template <typename Context, typename Container, typename Scope>
  auto construct(Scope scope, Context &context, Container &container) {
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
class shared_storage {
  // TODO
  // static_assert(std::is_trivially_destructible_v< Type > ==
  // std::is_trivially_destructible_v< storage_instance< Type, shared > >);
  storage_instance<shared, Type, StoredType, Factory> instance_;

public:
  template <typename... Args>
  shared_storage(Args &&...args) : instance_(std::forward<Args>(args)...) {}

  using conversions = Conversions;
  using factory_type = Factory;
  using type = Type;
  using stored_type = StoredType;
  using resolved_type =
      decltype(std::declval<
                   storage_instance<shared, Type, StoredType, Factory> &>()
                   .get());
  using tag_type = shared;

  template <typename Context, typename Container, typename Scope>
  decltype(auto) resolve(Scope scope, Context &context, Container &container) {
    // The instance is kept, so what it is constructed from has to live as
    // long as the container.
    (void)scope;
    if (instance_.empty())
      instance_.construct(persistent_scope, context, container);
    return instance_.get();
  }

  bool is_resolved() const { return !instance_.empty(); }
  void reset() { instance_.reset(); }
};
} // namespace detail
} // namespace dingo
