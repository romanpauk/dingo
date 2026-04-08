//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/factory/constructor.h>
#include <dingo/memory/aligned_storage.h>
#include <dingo/storage/detail/resettable.h>
#include <dingo/storage/detail/storage.h>
#include <dingo/storage/storage_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_conversion_traits.h>

#include <memory>
#include <optional>
#include <type_traits>

namespace dingo {
struct shared {};

namespace detail {
struct shared_storage_traits_base {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = true;

    template <typename Leaf, typename Context, typename Storage>
    static auto make_guard(Context& context, const Storage& storage) {
        return recursion_guard_wrapper<Leaf>(context, !storage.is_resolved());
    }

    template <typename Storage>
    static bool preserves_closure(const Storage& storage) {
        // Only unresolved shared storage needs the factory closure to stay on
        // the resolving_context stack while address-based conversions are
        // materialized. Once the instance is resolved, there are no temporary
        // construction artifacts left to preserve.
        return !storage.is_resolved();
    }

    template <typename Context, typename Storage, typename Container>
    static auto materialize_source(Context& context, Storage& storage,
                                   Container& container) {
        return make_resolved_source(storage.resolve(context, container));
    }
};
} // namespace detail

template <typename Type, typename U>
struct storage_traits<
    shared, Type, U,
    std::enable_if_t<!type_traits<Type>::enabled && !std::is_reference_v<Type> &&
                     !std::is_array_v<Type>>> : detail::shared_storage_traits_base {
    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct storage_traits<shared, Type*, U> : detail::shared_storage_traits_base {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename T, typename U>
struct storage_traits<shared, T[], U> : detail::shared_storage_traits_base {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<>;
    using pointer_types =
        type_list<typename detail::wrapper_rebind_leaf<T, U>::type*>;
    using conversion_types = type_list<>;
};

template <typename T, size_t N, typename U>
struct storage_traits<shared, T[N], U> : detail::shared_storage_traits_base {
    using rebound_row_type = typename detail::wrapper_rebind_leaf<T, U>::type;
    using rebound_exact_type = typename detail::wrapper_rebind_leaf<T[N], U>::type;

    using value_types = type_list<>;
    using lvalue_reference_types = type_list<exact_lookup<rebound_exact_type>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types =
        type_list<rebound_row_type*, exact_lookup<rebound_exact_type>*>;
    using conversion_types = type_list<>;
};

template <typename Array, typename Deleter, typename U>
struct storage_traits<
    shared, std::unique_ptr<Array, Deleter>, U,
    std::enable_if_t<std::is_array_v<Array> && (std::extent_v<Array, 0> == 0)>>
    : detail::shared_storage_traits_base {
    using handle_type =
        detail::wrapper_rebind_leaf_t<std::unique_ptr<Array, Deleter>, U>;
    using pointer_types =
        typename detail::smart_array_pointer_types<
            handle_type, std::remove_extent_t<Array>, U>::type;

    using value_types = type_list<>;
    using lvalue_reference_types = type_list<handle_type&>;
    using rvalue_reference_types = type_list<>;
    using conversion_types = type_list<>;
};

template <typename T, typename Deleter, typename U>
struct storage_traits<shared, std::unique_ptr<T, Deleter>, U,
                      std::enable_if_t<!std::is_array_v<T>>>
    : detail::shared_storage_traits_base {
    using handle_type =
        detail::wrapper_rebind_leaf_t<std::unique_ptr<T, Deleter>, U>;
    using types = detail::wrapper_storage_types<handle_type>;

    using value_types = type_list<U>;
    using lvalue_reference_types = typename types::lvalue_reference_types;
    using rvalue_reference_types = type_list<>;
    using pointer_types = typename types::pointer_types;
    using conversion_types = type_list<>;
};

template <typename Array, typename U>
struct storage_traits<
    shared, std::shared_ptr<Array>, U,
    std::enable_if_t<std::is_array_v<Array> && (std::extent_v<Array, 0> == 0)>>
    : detail::shared_storage_traits_base {
    using handle_type = detail::wrapper_rebind_leaf_t<std::shared_ptr<Array>, U>;
    using pointer_types =
        typename detail::smart_array_pointer_types<
            handle_type, std::remove_extent_t<Array>, U>::type;

    using value_types = type_list<handle_type>;
    using lvalue_reference_types = type_list<handle_type&>;
    using rvalue_reference_types = type_list<>;
    using conversion_types = type_list<handle_type>;
};

template <typename T, typename U>
struct storage_traits<shared, std::shared_ptr<T>, U,
                      std::enable_if_t<!std::is_array_v<T>>>
    : detail::shared_storage_traits_base {
    using handle_type = detail::wrapper_rebind_leaf_t<std::shared_ptr<T>, U>;
    using types = detail::wrapper_storage_types<handle_type>;

    using value_types =
        type_list_cat_t<type_list<U>, typename types::copyable_value_types>;
    using lvalue_reference_types = typename types::lvalue_reference_types;
    using rvalue_reference_types = type_list<>;
    using pointer_types = typename types::pointer_types;
    using conversion_types = typename types::copyable_value_types;
};

template <typename T, typename U>
struct storage_traits<shared, std::optional<T>, U>
    : detail::shared_storage_traits_base {
    using value_types = type_list<>;
    using lvalue_reference_types =
        type_list<U&, exact_lookup<std::optional<T>>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, exact_lookup<std::optional<T>>*>;
    using conversion_types = type_list<>;
};

namespace detail {
template <typename Type, typename U> struct conversions<shared, Type, U>
    : storage_traits<shared, Type, U> {};

template <typename Type>
void destroy_array_value(Type& value) {
    if constexpr (std::is_array_v<Type>) {
        for (size_t i = std::extent_v<Type>; i > 0; --i) {
            destroy_array_value(value[i - 1]);
        }
    } else if constexpr (!std::is_trivially_destructible_v<Type>) {
        value.~Type();
    }
}

template <typename Type, typename Factory>
struct storage_instance_base : Factory {
    template <typename... Args>
    storage_instance_base(Args&&... args)
        : Factory(std::forward<Args>(args)...) {}

    Type* get() const { return reinterpret_cast<Type*>(&instance_); }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
    template <typename Context, typename Container>
    void construct(Context& context, Container& container) {
        assert(!initialized_);
        Factory::template construct<Type*>(&instance_, context, container);
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

template <typename Type, typename Factory,
          bool IsTriviallyDestructible = std::is_trivially_destructible_v<Type>>
struct storage_instance_dtor;

template <typename Type, typename Factory>
struct storage_instance_dtor<Type, Factory, true>
    : storage_instance_base<Type, Factory> {
    template <typename... Args>
    storage_instance_dtor(Args&&... args)
        : storage_instance_base<Type, Factory>(std::forward<Args>(args)...) {}

    void reset() { this->initialized_ = false; }
};

template <typename Type, typename Factory>
struct storage_instance_dtor<Type, Factory, false>
    : storage_instance_base<Type, Factory> {
    template <typename... Args>
    storage_instance_dtor(Args&&... args)
        : storage_instance_base<Type, Factory>(std::forward<Args>(args)...) {}

    ~storage_instance_dtor() { reset(); }

    void reset() {
        if (this->initialized_) {
            this->initialized_ = false;
            this->get()->~Type();
        }
    }
};

template <typename Type, typename StoredType, typename Factory,
          typename = void>
class shared_storage_instance_impl : public storage_instance_dtor<Type, Factory> {
  public:
    template <typename... Args>
    shared_storage_instance_impl(Args&&... args)
        : storage_instance_dtor<Type, Factory>(std::forward<Args>(args)...) {}

    static_assert(
        std::is_trivially_destructible_v<Type> ==
        std::is_trivially_destructible_v<storage_instance_dtor<Type, Factory>>);
};

template <typename Type, typename StoredType, typename Factory>
class shared_storage_instance_impl<
    Type, StoredType, Factory, std::enable_if_t<type_traits<Type>::enabled>>
    : Factory {
  public:
    template <typename... Args>
    shared_storage_instance_impl(Args&&... args)
        : Factory(std::forward<Args>(args)...) {}

    template <typename Context, typename Container>
    void construct(Context& context, Container& container) {
        assert(empty());
        new (&instance_) StoredType(
            type_conversion_traits<StoredType, Type>::convert(
                Factory::template construct<Type>(context, container)));
        initialized_ = true;
    }

    ~shared_storage_instance_impl() { reset(); }

    StoredType& get() const { return *get_ptr(); }

    void reset() {
        if (initialized_) {
            get_ptr()->~StoredType();
            initialized_ = false;
        }
    }

    bool empty() const { return !initialized_; }

  private:
    StoredType* get_ptr() const {
        return reinterpret_cast<StoredType*>(&instance_);
    }

    mutable aligned_storage_t<sizeof(StoredType), alignof(StoredType)> instance_;
    bool initialized_ = false;
};

template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared, Type, StoredType, Factory>
    : public shared_storage_instance_impl<Type, StoredType, Factory> {
  public:
    template <typename... Args>
    storage_instance(Args&&... args)
        : shared_storage_instance_impl<Type, StoredType, Factory>(
              std::forward<Args>(args)...) {}
};

template <typename Type, size_t N, typename StoredType, typename Factory>
class storage_instance<shared, Type[N], StoredType, Factory>
    : Factory {
  public:
    template <typename... Args>
    storage_instance(Args&&... args)
        : Factory(std::forward<Args>(args)...) {}

    ~storage_instance() { reset(); }

    template <typename Context, typename Container>
    void construct(Context& context, Container& container) {
        assert(empty());
        Factory::template construct<Type[N]>(&instance_, context, container);
        initialized_ = true;
    }

    Type* get() const {
        return std::addressof((*get_array())[0]);
    }

    void reset() {
        if (!initialized_) {
            return;
        }

        if constexpr (std::is_array_v<Type> ||
                      !std::is_trivially_destructible_v<Type>) {
            for (size_t i = N; i > 0; --i) {
                destroy_array_value((*get_array())[i - 1]);
            }
        }
        initialized_ = false;
    }

    bool empty() const { return !initialized_; }

  private:
    Type (*get_array() const)[N] {
        return reinterpret_cast<Type(*)[N]>(&instance_);
    }

    mutable aligned_storage_t<sizeof(Type[N]), alignof(Type[N])> instance_;
    bool initialized_ = false;
};

template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared, Type*, StoredType*, Factory>
    : Factory {
  public:
    template <typename... Args>
    storage_instance(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    ~storage_instance() { reset(); }

    template <typename Context, typename Container>
    void construct(Context& context, Container& container) {
        assert(empty());
        instance_ = Factory::template construct<Type*>(context, container);
    }

    StoredType* get() const {
        return type_conversion_traits<StoredType*, Type*>::convert(instance_);
    }
    void reset() {
        delete instance_;
        instance_ = nullptr;
    }
    bool empty() const { return instance_ == nullptr; }

  private:
    Type* instance_ = nullptr;
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class storage<shared, Type, StoredType, Factory, Conversions>
    : public resettable {
    // TODO
    // static_assert(std::is_trivially_destructible_v< Type > ==
    // std::is_trivially_destructible_v< storage_instance< Type, shared > >);
    storage_instance<shared, Type, StoredType, Factory> instance_;

  public:
    template <typename... Args>
    storage(Args&&... args) : instance_(std::forward<Args>(args)...) {}

    using conversions = Conversions;
    using type = Type;
    using stored_type = StoredType;
    using tag_type = shared;

    template <typename Context, typename Container>
    decltype(auto) resolve(Context& context, Container& container) {
        if (instance_.empty())
            instance_.construct(context, container);
        return instance_.get();
    }

    bool is_resolved() const { return !instance_.empty(); }
    void reset() override { instance_.reset(); }
};
} // namespace detail
} // namespace dingo
