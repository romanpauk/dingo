//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/aligned_storage.h>
#include <dingo/detail/storage_conversion_traits.h>
#include <dingo/detail/wrapper_storage.h>
#include <dingo/decay.h>
#include <dingo/factory/constructor.h>
#include <dingo/storage.h>

namespace dingo {
struct shared_cyclical {};

template <typename Base, typename Derived> struct is_virtual_base_of {
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
static constexpr bool is_virtual_base_of_v =
    is_virtual_base_of<Base, Derived>::value;

namespace detail {
template <typename Type, typename Interface, typename = void>
struct supports_shared_cyclical_wrapper_conversion : std::false_type {};

template <typename Type, typename Interface>
struct supports_shared_cyclical_wrapper_conversion<
    Type, Interface, std::enable_if_t<has_wrapper_rebind_v<Type, Interface>>>
    : std::bool_constant<
          is_wrapper_registration_v<Type, Interface> &&
          supports_shared_cyclical_wrapper_types_v<
              Type, storage_rebind_t<Type, Interface>>> {};

template <typename Type, typename Interface>
inline constexpr bool supports_shared_cyclical_wrapper_conversion_v =
    supports_shared_cyclical_wrapper_conversion<Type, Interface>::value;

template <typename Type, typename U>
struct conversions<shared_cyclical, Type, U,
                   std::enable_if_t<is_plain_value_registration_v<Type, U>>> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct conversions<
    shared_cyclical, Type, U,
    std::enable_if_t<is_wrapper_registration_v<Type, U> &&
                     !supports_shared_cyclical_wrapper_conversion_v<Type, U>>> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct conversions<shared_cyclical, Type*, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct conversions<
    shared_cyclical, Type, U,
    std::enable_if_t<supports_shared_cyclical_wrapper_conversion_v<Type, U>>> {
    using value_types =
        type_list_if_t<has_copyable_wrapper_rebind_v<Type, U>,
                       storage_rebind_t<Type, U>>;
    using lvalue_reference_types =
        type_list_cat_t<type_list<U&>,
                        type_list_if_t<has_wrapper_rebind_v<Type, U>,
                                       storage_rebind_t<Type, U>&>>;
    using rvalue_reference_types = type_list<>;
    using pointer_types =
        type_list_cat_t<type_list<U*>,
                        type_list_if_t<has_wrapper_rebind_v<Type, U>,
                                       storage_rebind_t<Type, U>*>>;
    using conversion_types =
        type_list_if_t<has_copyable_wrapper_rebind_v<Type, U>,
                       storage_rebind_t<Type, U>>;
};

// Disallow virtual bases as interfaces as in cyclical storage, we can't
// properly calculate the cast when the object is not constructed
template <typename Type, typename StoredType, typename Factory,
          typename Conversions, typename Derived, typename Base>
struct storage_interface_requirements<
    storage<shared_cyclical, Type, StoredType, Factory, Conversions>, Derived,
    Base> : std::bool_constant<!is_virtual_base_of_v<Base, Derived>> {};

template <typename Type, typename Factory,
          bool IsTriviallyDestructible = std::is_trivially_destructible_v<Type>>
class storage_instance_shared_impl;

template <typename Type, typename Factory>
class storage_instance_shared_impl<Type, Factory, false> : Factory {
  public:
    template <typename... Args>
    storage_instance_shared_impl(Args&&... args)
        : Factory(std::forward<Args>(args)...) {}

    ~storage_instance_shared_impl() { reset(); }

    template <typename Context> Type* resolve(Context&) {
        assert(!resolved_);
        resolved_ = true;
        return get();
    }

    Type* get() { return reinterpret_cast<Type*>(&instance_); }

    template <typename Context, typename Container>
    void construct(Context& context, Container& container) {
        Factory::template construct<Type*>(&instance_, context, container);
        constructed_ = true;
    }

    bool empty() const { return !resolved_; }

    void reset() {
        resolved_ = false;
        if (constructed_) {
            constructed_ = false;
            reinterpret_cast<Type*>(&instance_)->~Type();
        }
    }

  private:
    dingo::aligned_storage_t<sizeof(Type), alignof(Type)> instance_;
    bool resolved_ = false;
    bool constructed_ = false;
};

template <typename Type, typename Factory>
class storage_instance_shared_impl<Type, Factory, true> : Factory {
  public:
    template <typename... Args>
    storage_instance_shared_impl(Args&&... args)
        : Factory(std::forward<Args>(args)...) {}

    template <typename Context> Type* resolve(Context&) {
        assert(!resolved_);
        resolved_ = true;
        return get();
    }

    Type* get() { return reinterpret_cast<Type*>(&instance_); }

    template <typename Context, typename Container>
    void construct(Context& context, Container& container) {
        Factory::template construct<Type*>(&instance_, context, container);
    }

    bool empty() const { return !resolved_; }

    void reset() { resolved_ = false; }

  private:
    dingo::aligned_storage_t<sizeof(Type), alignof(Type)> instance_;
    bool resolved_ = false;
};

template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared_cyclical, Type, StoredType, Factory>
    : public std::conditional_t<
          supports_shared_cyclical_wrapper_storage_v<Type, StoredType>,
          shared_cyclical_wrapper_storage_instance<Type, StoredType, Factory>,
          storage_instance_shared_impl<Type, Factory>> {
    using instance_type = std::conditional_t<
        supports_shared_cyclical_wrapper_storage_v<Type, StoredType>,
        shared_cyclical_wrapper_storage_instance<Type, StoredType, Factory>,
        storage_instance_shared_impl<Type, Factory>>;

  public:
    template <typename... Args>
    storage_instance(Args&&... args)
        : instance_type(std::forward<Args>(args)...) {}
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class storage<shared_cyclical, Type, StoredType, Factory, Conversions>
    : public resettable_i {
    storage_instance<shared_cyclical, Type, StoredType, Factory> instance_;

    template <typename T> struct rollback {
        rollback(T* instance) : instance_(instance) {}
        ~rollback() {
            if (std::uncaught_exceptions())
                instance_->reset();
        }

      private:
        T* instance_;
    };

  public:
    template <typename... Args>
    storage(Args&&... args) : instance_(std::forward<Args>(args)...) {}

    // TODO: there is a problem with cache rollback for cyclical storage
    static constexpr bool cacheable = false;

    using conversions = Conversions;
    using type = Type;
    using stored_type = StoredType;
    using tag_type = shared_cyclical;

    template <typename Context, typename Container>
    auto resolve(Context& context, Container& container)
        -> decltype(instance_.get()) {
        if (instance_.empty()) {
            context.template construct<rollback<decltype(instance_)>>(
                &instance_);

            auto&& instance = instance_.resolve(context);
            instance_.construct(context, container);
            return instance;
        }
        return instance_.get();
    }

    bool is_resolved() const { return !instance_.empty(); }

    void reset() override { instance_.reset(); }
};
} // namespace detail
} // namespace dingo
