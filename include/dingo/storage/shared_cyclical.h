//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/type_descriptor.h>
#include <dingo/factory/constructor.h>
#include <dingo/memory/aligned_storage.h>
#include <dingo/storage/scope_traits.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/normalized_type.h>

#include <atomic>
#include <memory>
#include <new>
#include <type_traits>
#include <vector>

namespace dingo {
struct shared_cyclical {};

namespace detail {
template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class shared_cyclical_storage;

// Cells of shared_cyclical, which publishes the leaf U and a shared_ptr<U>
// directly, whatever the registered shape.
template <storage_form Form, typename U> struct shared_cyclical_requests {
  using value_types = type_list<>;
  using lvalue_reference_types = type_list<U &>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<U *>;
};

template <typename U>
struct shared_cyclical_requests<storage_form::shared_handle, U> {
  using value_types = type_list<std::shared_ptr<U>>;
  using lvalue_reference_types = type_list<U &, std::shared_ptr<U> &>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<U *, std::shared_ptr<U> *>;
};
} // namespace detail

template <> struct scope_traits<shared_cyclical> {
  static constexpr bool enabled = true;
  static constexpr bool allows_cycles = true;
  static constexpr bool is_stable = false;
  static constexpr bool consumes = false;
  static constexpr materialization_guard guard = materialization_guard::none;
  static constexpr bool retains_unresolved_source = false;

  // Only plain objects, raw pointers and shared_ptr can be stored in a cycle.
  // The instance is allocated before it is constructed, so a dynamic array,
  // which has no size to allocate, cannot be.
  static constexpr storage_exposure exposed(storage_form form) {
    switch (form) {
    case storage_form::plain:
    case storage_form::alternative:
    case storage_form::pointer:
    case storage_form::bounded_array:
      return storage_exposure::lvalue | storage_exposure::pointer;
    case storage_form::shared_handle:
      return storage_exposure::value | storage_exposure::lvalue |
             storage_exposure::pointer;
    case storage_form::array:
    case storage_form::shared_array_handle:
    case storage_form::unique_handle:
    case storage_form::unique_array_handle:
    case storage_form::optional:
    case storage_form::reference:
    case storage_form::unknown:
      break;
    }
    return storage_exposure::none;
  }

  template <storage_form Form, typename Type, typename U>
  using requests = detail::shared_cyclical_requests<Form, U>;

  template <typename Type, typename StoredType, typename Factory,
            typename Conversions>
  using storage =
      detail::shared_cyclical_storage<Type, StoredType, Factory, Conversions>;
};

namespace detail {
template <typename Context, typename = void>
struct has_on_rollback : std::false_type {};

template <typename Context>
struct has_on_rollback<
    Context, std::void_t<decltype(std::declval<Context &>().on_rollback(
                 std::declval<void (*)(void *, void *) noexcept>(),
                 std::declval<void *>()))>> : std::true_type {};

// A shared_cyclical instance publishes its address when it is resolved, before
// the object is constructed, so a cycle can close over it.
template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared_cyclical, Type, StoredType, Factory>
    : Factory, public shared_value<Type> {
public:
  template <typename... Args>
  storage_instance(Args &&...args) : Factory(std::forward<Args>(args)...) {}

  Type *resolve() {
    assert(!resolved_);
    resolved_ = true;
    return get();
  }

  Type *get() const { return this->pointer(); }

  template <typename Context, typename Container>
  void construct(construction_scope scope, Context &context,
                 Container &container) {
    assert(resolved_);
    Factory::template construct<Type *>(this->storage(), scope, context,
                                        container);
    this->constructed();
  }

  bool empty() const { return !resolved_; }

  void reset() {
    resolved_ = false;
    shared_value<Type>::reset();
  }

private:
  bool resolved_ = false;
};

// This path is intentionally std::shared_ptr-specific. We need to publish a
// copyable owning handle before the pointee is constructed so cyclical
// dependencies can observe a stable address during construction.
template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared_cyclical, std::shared_ptr<Type>,
                       std::shared_ptr<StoredType>, Factory> : Factory {
  using storage_type = dingo::aligned_storage_t<sizeof(Type), alignof(Type)>;

  class deleter {
  public:
    void set_constructed() { constructed_ = true; }

    void operator()(Type *ptr) {
      if constexpr (!std::is_trivially_destructible_v<Type>) {
        if (constructed_)
          ptr->~Type();
      }

      delete reinterpret_cast<storage_type *>(ptr);
    }

  private:
    bool constructed_ = false;
  };

public:
  template <typename... Args>
  storage_instance(Args &&...args) : Factory(std::forward<Args>(args)...) {}

  std::shared_ptr<StoredType> &resolve() {
    assert(!instance_);
    instance_.reset(reinterpret_cast<Type *>(new storage_type), deleter());
    return instance_;
  }

  std::shared_ptr<StoredType> &get() { return instance_; }

  template <typename Context, typename Container>
  auto construct(construction_scope scope, Context &context,
                 Container &container) {
    assert(instance_);
    Factory::template construct<Type *>(instance_.get(), scope, context,
                                        container);
    std::get_deleter<deleter>(instance_)->set_constructed();
  }

  bool empty() const { return !instance_; }

  void reset() { instance_.reset(); }

private:
  std::shared_ptr<StoredType> instance_;
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class shared_cyclical_storage {
  struct conversion_entry {
    explicit conversion_entry(type_descriptor key) : descriptor(key) {}
    virtual ~conversion_entry() = default;

    type_descriptor descriptor;
  };

  template <typename T> struct typed_conversion_entry : conversion_entry {
    template <typename Source>
    explicit typed_conversion_entry(Source &&source)
        : conversion_entry(describe_type<T>()),
          value(std::forward<Source>(source)) {}

    T value;
  };

  storage_instance<shared_cyclical, Type, StoredType, Factory> instance_;
  std::vector<std::unique_ptr<conversion_entry>> conversions_;

  class local_rollback_guard {
  public:
    explicit local_rollback_guard(shared_cyclical_storage *storage)
        : storage_(storage) {}
    ~local_rollback_guard() {
      if (active_) {
        storage_->reset();
      }
    }

    void release() { active_ = false; }

  private:
    shared_cyclical_storage *storage_;
    bool active_ = true;
  };

public:
  template <typename... Args>
  shared_cyclical_storage(Args &&...args)
      : instance_(std::forward<Args>(args)...) {}

  using conversions = Conversions;
  using type = Type;
  using stored_type = StoredType;
  using resolved_type =
      decltype(std::declval<storage_instance<shared_cyclical, Type, StoredType,
                                             Factory> &>()
                   .get());
  using tag_type = shared_cyclical;

  template <typename Context, typename Container>
  decltype(auto) resolve(construction_scope scope, Context &context,
                         Container &container) {
    if (instance_.empty()) {
      // shared_cyclical publishes its owning handle before the pointee
      // is fully constructed so the graph can close over a stable
      // address. Rebound shared_ptr interface handles therefore belong
      // to the storage too and must roll back with the instance if the
      // first resolve throws.
      //
      // A failed construction is undone here; a context that keeps a
      // transaction also undoes the publication when it rolls back later.
      // The static context does not, so it relies on the local guard alone.
      if constexpr (has_on_rollback<Context>::value) {
        context.on_rollback(
            +[](void *self, void *) noexcept {
              static_cast<shared_cyclical_storage *>(self)->reset();
            },
            this);
      }
      local_rollback_guard rollback(this);
      instance_.resolve();
      instance_.construct(scope, context, container);
      rollback.release();
    }
    return instance_.get();
  }

  template <typename T, typename Context, typename Source>
  T &resolve_conversion(Context &, Source &&source) {
    for (auto &entry : conversions_) {
      if (entry->descriptor == describe_type<T>()) {
        return static_cast<typed_conversion_entry<T> &>(*entry).value;
      }
    }

    conversions_.push_back(std::make_unique<typed_conversion_entry<T>>(
        std::forward<Source>(source)));
    return static_cast<typed_conversion_entry<T> &>(*conversions_.back()).value;
  }

  bool is_resolved() const { return !instance_.empty(); }

  void reset() noexcept {
    // Graph objects can keep references to rebound shared_ptr interface
    // handles stored in `conversions_`. Destroy the object graph first so
    // those references stay valid for any destructor work.
    instance_.reset();
    conversions_.clear();
  }
};
} // namespace detail
} // namespace dingo
