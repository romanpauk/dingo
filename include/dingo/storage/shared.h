//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/decay.h>
#include <dingo/factory/constructor.h>
#include <dingo/storage.h>
#include <dingo/type_list.h>

namespace dingo {
struct shared {};

namespace detail {
template <typename Type, typename U> struct conversions<shared, Type, U> {
    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U> struct conversions<shared, Type*, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct conversions<shared, std::shared_ptr<Type>, U> {
    using value_types = type_list<U, std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<U&, std::shared_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::shared_ptr<U>*>;
    using conversion_types = type_list<std::shared_ptr<U>>;
};

template <typename Type, typename U>
struct conversions<shared, std::unique_ptr<Type>, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&, std::unique_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::unique_ptr<U>*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct conversions<shared, std::optional<Type>, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&, std::optional<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::optional<U>*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename Factory>
struct storage_instance_base : Factory {
    template <typename... Args>
    storage_instance_base(Args&&... args)
        : Factory(std::forward<Args>(args)...) {}

    Type* get() const { return reinterpret_cast<Type*>(&instance_); }

    template <typename Context, typename Container>
    void construct(Context& context, Container& container) {
        assert(!initialized_);
        Factory::template construct<Type*>(&instance_, context, container);
        initialized_ = true;
    }

    bool empty() const { return !initialized_; }

  protected:
    mutable std::aligned_storage_t<sizeof(Type), alignof(Type)> instance_;
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

template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared, Type, StoredType, Factory>
    : public storage_instance_dtor<Type, Factory> {
  public:
    template <typename... Args>
    storage_instance(Args&&... args)
        : storage_instance_dtor<Type, Factory>(std::forward<Args>(args)...) {}

    static_assert(
        std::is_trivially_destructible_v<Type> ==
        std::is_trivially_destructible_v<storage_instance_dtor<Type, Factory>>);
};

template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared, std::unique_ptr<Type>,
                       std::unique_ptr<StoredType>, Factory> : Factory {
  public:
    template <typename... Args>
    storage_instance(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    template <typename Context, typename Container>
    void construct(Context& context, Container& container) {
        assert(!instance_);
        instance_ = Factory::template construct<std::unique_ptr<Type>>(
            context, container);
    }

    std::unique_ptr<StoredType>& get() const { return instance_; }
    void reset() { instance_.reset(); }
    bool empty() const { return instance_.get() == nullptr; }

  private:
    mutable std::unique_ptr<StoredType> instance_;
};

template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared, std::shared_ptr<Type>,
                       std::shared_ptr<StoredType>, Factory> : Factory {
  public:
    template <typename... Args>
    storage_instance(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    template <typename Context, typename Container>
    void construct(Context& context, Container& container) {
        assert(!instance_);
        instance_ = Factory::template construct<std::shared_ptr<Type>>(
            context, container);
    }

    std::shared_ptr<StoredType>& get() const { return instance_; }
    void reset() { instance_.reset(); }
    bool empty() const { return instance_.get() == nullptr; }

  private:
    mutable std::shared_ptr<StoredType> instance_;
};

// TODO: the container should act as a vector of pointers, so this should not
// delete
template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared, Type*, StoredType*, Factory>
    : public storage_instance<shared, std::unique_ptr<Type>,
                              std::unique_ptr<StoredType>, Factory> {
  public:
    template <typename... Args>
    storage_instance(Args&&... args)
        : storage_instance<shared, std::unique_ptr<Type>,
                           std::unique_ptr<StoredType>, Factory>(
              std::forward<Args>(args)...) {}

    template <typename Context, typename Container>
    void construct(Context& context, Container& container) {
        storage_instance<shared, std::unique_ptr<Type>,
                         std::unique_ptr<StoredType>,
                         Factory>::construct(context, container);
    }

    StoredType* get() const {
        return storage_instance<shared, std::unique_ptr<Type>,
                                std::unique_ptr<StoredType>, Factory>::get()
            .get();
    }
};

template <typename Type, typename StoredType, typename Factory>
class storage_instance<shared, std::optional<Type>, StoredType, Factory>
    : Factory {
  public:
    template <typename... Args>
    storage_instance(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    template <typename Context, typename Container>
    void construct(Context& context, Container& container) {
        instance_.emplace(
            Factory::template construct<Type>(context, container));
    }

    std::optional<Type>& get() const { return instance_; }

    void reset() { instance_.reset(); }
    bool empty() const { return !instance_.has_value(); }

  private:
    mutable std::optional<Type> instance_;
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