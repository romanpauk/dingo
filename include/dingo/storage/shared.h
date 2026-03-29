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
#include <dingo/storage/resettable.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_conversion_traits.h>

namespace dingo {
struct shared {};

namespace detail {
template <typename Type, typename U> struct conversions<shared, Type, U>
    : type_storage_traits<shared, Type, U> {};

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

    static constexpr bool cacheable = true;

    using conversions = Conversions;
    using type = Type;
    using stored_type = StoredType;
    using tag_type = shared;

    template <typename Context, typename Container>
    auto resolve(Context& context, Container& container)
        -> decltype(instance_.get()) {
        if (instance_.empty())
            instance_.construct(context, container);
        return instance_.get();
    }

    bool is_resolved() const { return !instance_.empty(); }
    void reset() override { instance_.reset(); }
};
} // namespace detail
} // namespace dingo
