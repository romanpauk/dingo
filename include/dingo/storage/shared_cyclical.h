//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/factory/constructor.h>
#include <dingo/memory/aligned_storage.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/storage_scope_policy.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_descriptor.h>

#include <atomic>
#include <memory>
#include <new>
#include <type_traits>
#include <vector>

namespace dingo {
struct shared_cyclical {};

template <> struct storage_scope_policy<shared_cyclical> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = false;
  static constexpr bool consumes = false;
  static constexpr materialization_guard guard = materialization_guard::none;
  static constexpr bool retains_unresolved_source = false;

  // Only plain objects, raw pointers and shared_ptr can be stored in a cycle.
  static constexpr storage_exposure exposed(storage_form form) {
    switch (form) {
    case storage_form::plain:
    case storage_form::alternative:
    case storage_form::pointer:
    case storage_form::array:
    case storage_form::bounded_array:
      return storage_exposure::lvalue | storage_exposure::pointer;
    case storage_form::shared_handle:
    case storage_form::shared_array_handle:
      return storage_exposure::value | storage_exposure::lvalue |
             storage_exposure::pointer;
    case storage_form::unique_handle:
    case storage_form::unique_array_handle:
    case storage_form::optional:
    case storage_form::unknown:
      break;
    }
    return storage_exposure::none;
  }
};

namespace detail {
// Cells of shared_cyclical, which publishes the leaf U and a shared_ptr<U>
// directly, whatever the registered shape: every cell deviates from the generic
// rule, so the scope lists them itself.
template <storage_form Form, typename U> struct shared_cyclical_cell {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = false;

  using value_types = type_list<>;
  using lvalue_reference_types = type_list<U &>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<U *>;
};

template <typename U>
struct shared_cyclical_cell<storage_form::shared_handle, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = false;

  using value_types = type_list<std::shared_ptr<U>>;
  using lvalue_reference_types = type_list<U &, std::shared_ptr<U> &>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<U *, std::shared_ptr<U> *>;
};

template <typename U>
struct shared_cyclical_cell<storage_form::shared_array_handle, U>
    : shared_cyclical_cell<storage_form::shared_handle, U> {};

template <storage_form Form, typename Type, typename U>
struct storage_cell<shared_cyclical, Form, Type, U>
    : shared_cyclical_cell<Form, U> {};
} // namespace detail

template <typename Base, typename Derived> struct is_virtual_base {
#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winaccessible-base"
#endif
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4250)
#endif
  struct Test : Derived, virtual Base {};
#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
  // If this equals, it means Base is already a virtual base of Derived
  static constexpr bool value = sizeof(Test) == sizeof(Derived);
};

template <typename Base, typename Derived>
static constexpr bool is_virtual_base_v = is_virtual_base<Base, Derived>::value;

namespace detail {
template <typename Type, typename U>
struct conversions<shared_cyclical, Type, U>
    : type_storage_traits<shared_cyclical, Type, U> {};

template <typename Context, typename = void>
struct has_on_rollback : std::false_type {};

template <typename Context>
struct has_on_rollback<
    Context, std::void_t<decltype(std::declval<Context &>().on_rollback(
                 std::declval<void (*)(void *, void *) noexcept>(),
                 std::declval<void *>()))>> : std::true_type {};

// Disallow virtual bases as interfaces as in cyclical storage, we can't
// properly calculate the cast when the object is not constructed
template <typename Type, typename StoredType, typename Factory,
          typename Conversions, typename Derived, typename Base>
struct storage_interface_requirements<
    storage<shared_cyclical, Type, StoredType, Factory, Conversions>, Derived,
    Base> : std::bool_constant<!is_virtual_base_v<Base, Derived>> {};

template <typename Type, typename Factory,
          bool IsTriviallyDestructible = std::is_trivially_destructible_v<Type>>
class cyclical_storage_instance_impl;

template <typename Type, typename Factory>
class cyclical_storage_instance_impl<Type, Factory, false> : Factory {
public:
  template <typename... Args>
  cyclical_storage_instance_impl(Args &&...args)
      : Factory(std::forward<Args>(args)...) {}

  ~cyclical_storage_instance_impl() { reset(); }

  template <typename Context> Type *resolve(Context &) {
    assert(!resolved_);
    resolved_ = true;
    return get();
  }

  Type *get() { return std::launder(reinterpret_cast<Type *>(&instance_)); }

  template <typename Context, typename Container>
  void construct(construction_scope scope, Context &context,
                 Container &container) {
    assert(resolved_);
    Factory::template construct<Type *>(&instance_, scope, context, container);
    constructed_ = true;
  }

  bool empty() const { return !resolved_; }

  void reset() {
    resolved_ = false;
    if (constructed_) {
      constructed_ = false;
      std::launder(reinterpret_cast<Type *>(&instance_))->~Type();
    }
  }

private:
  dingo::aligned_storage_t<sizeof(Type), alignof(Type)> instance_;
  bool resolved_ = false;
  bool constructed_ = false;
};

template <typename Type, typename Factory>
class cyclical_storage_instance_impl<Type, Factory, true> : Factory {
public:
  template <typename... Args>
  cyclical_storage_instance_impl(Args &&...args)
      : Factory(std::forward<Args>(args)...) {}

  template <typename Context> Type *resolve(Context &) {
    assert(!resolved_);
    resolved_ = true;
    return get();
  }

  Type *get() { return std::launder(reinterpret_cast<Type *>(&instance_)); }

  template <typename Context, typename Container>
  void construct(construction_scope scope, Context &context,
                 Container &container) {
    assert(resolved_);
    Factory::template construct<Type *>(&instance_, scope, context, container);
  }

  bool empty() const { return !resolved_; }

  void reset() { resolved_ = false; }

private:
  dingo::aligned_storage_t<sizeof(Type), alignof(Type)> instance_;
  bool resolved_ = false;
};

template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared_cyclical, Type, StoredType, Factory>
    : public cyclical_storage_instance_impl<Type, Factory> {
public:
  template <typename... Args>
  storage_instance(Args &&...args)
      : cyclical_storage_instance_impl<Type, Factory>(
            std::forward<Args>(args)...) {}
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

  template <typename Context> std::shared_ptr<StoredType> &resolve(Context &) {
    assert(!instance_);
    instance_.reset(reinterpret_cast<Type *>(new storage_type), deleter());
    return instance_;
  }

  std::shared_ptr<StoredType> &get() { return instance_; }

  template <typename Context, typename Container>
  void construct(construction_scope scope, Context &context,
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
class storage<shared_cyclical, Type, StoredType, Factory, Conversions> {
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
    explicit local_rollback_guard(storage *storage) : storage_(storage) {}
    ~local_rollback_guard() {
      if (active_) {
        storage_->reset();
      }
    }

    void release() { active_ = false; }

  private:
    storage *storage_;
    bool active_ = true;
  };

public:
  template <typename... Args>
  storage(Args &&...args) : instance_(std::forward<Args>(args)...) {}

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
      if constexpr (has_on_rollback<Context>::value) {
        context.on_rollback(
            +[](void *self, void *) noexcept {
              static_cast<storage *>(self)->reset();
            },
            this);
        instance_.resolve(context);
        instance_.construct(scope, context, container);
      } else {
        local_rollback_guard rollback(this);
        instance_.resolve(context);
        instance_.construct(scope, context, container);
        rollback.release();
      }
      return instance_.get();
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
