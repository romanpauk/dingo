//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/aligned_storage.h>
#include <dingo/decay.h>
#include <dingo/factory/constructor.h>
#include <dingo/storage.h>
#include <dingo/type_conversion_traits.h>
#include <dingo/type_storage_traits.h>

namespace dingo {
struct shared {};

namespace detail {
template <typename Type, typename U> struct conversions<shared, Type, U>
    : type_storage_traits<shared, Type, U> {};

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
        detail::construct_factory<Type*>(
            &instance_, context, container, static_cast<Factory&>(*this));
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
    Type, StoredType, Factory, std::enable_if_t<has_type_traits_v<Type>>>
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
                detail::construct_factory<Type>(
                    context, container, static_cast<Factory&>(*this))));
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
        instance_ = detail::construct_factory<Type*>(
            context, container, static_cast<Factory&>(*this));
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
    : public resettable_i {
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
