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
};

template <typename Type, typename U> struct conversions<shared, Type*, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
};

template <typename Type, typename U> struct conversions<shared, std::shared_ptr<Type>, U> {
    using value_types = type_list<U, std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<U&, std::shared_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::shared_ptr<U>*>;
};

template <typename Type, typename U> struct conversions<shared, std::unique_ptr<Type>, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&, std::unique_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::unique_ptr<U>*>;
};

template <typename Type, typename Factory> struct storage_instance_base : Factory {
    template <typename... Args> storage_instance_base(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    Type* get() const { return reinterpret_cast<Type*>(&instance_); }

    template <typename Context> void construct(Context& context) {
        assert(!initialized_);
        Factory::template construct<Type*>(&instance_, context);
        initialized_ = true;
    }

    bool empty() const { return !initialized_; }

  protected:
    mutable std::aligned_storage_t<sizeof(Type), alignof(Type)> instance_;
    bool initialized_ = false;
};

template <typename Type, typename Factory, bool IsTriviallyDestructible = std::is_trivially_destructible_v<Type>>
struct storage_instance_dtor;

template <typename Type, typename Factory>
struct storage_instance_dtor<Type, Factory, true> : storage_instance_base<Type, Factory> {
    template <typename... Args>
    storage_instance_dtor(Args&&... args) : storage_instance_base<Type, Factory>(std::forward<Args>(args)...) {}

    void reset() { this->initialized_ = false; }
};

template <typename Type, typename Factory>
struct storage_instance_dtor<Type, Factory, false> : storage_instance_base<Type, Factory> {
    template <typename... Args>
    storage_instance_dtor(Args&&... args) : storage_instance_base<Type, Factory>(std::forward<Args>(args)...) {}

    ~storage_instance_dtor() { reset(); }

    void reset() {
        if (this->initialized_) {
            this->initialized_ = false;
            this->get()->~Type();
        }
    }
};

template <typename Type, typename Factory>
class storage_instance<shared, Type, Factory> : public storage_instance_dtor<Type, Factory> {
  public:
    template <typename... Args>
    storage_instance(Args&&... args) : storage_instance_dtor<Type, Factory>(std::forward<Args>(args)...) {}

    static_assert(std::is_trivially_destructible_v<Type> ==
                  std::is_trivially_destructible_v<storage_instance_dtor<Type, Factory>>);
};

template <typename Type, typename Factory> class storage_instance<shared, std::unique_ptr<Type>, Factory> : Factory {
  public:
    template <typename... Args> storage_instance(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    template <typename Context> void construct(Context& context) {
        assert(!instance_);
        instance_ = Factory::template construct<std::unique_ptr<Type>>(context);
    }

    std::unique_ptr<Type>& get() const { return instance_; }
    void reset() { instance_.reset(); }
    bool empty() const { return instance_.get() == nullptr; }

  private:
    mutable std::unique_ptr<Type> instance_;
};

template <typename Type, typename Factory> class storage_instance<shared, std::shared_ptr<Type>, Factory> : Factory {
  public:
    template <typename... Args> storage_instance(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    template <typename Context> void construct(Context& context) {
        assert(!instance_);
        instance_ = Factory::template construct<std::shared_ptr<Type>>(context);
    }

    // TODO: this can't be reference so we can construct shared_ptr<Base> from
    // shared_ptr<Type> later
    std::shared_ptr<Type> get() const { return instance_; }
    void reset() { instance_.reset(); }
    bool empty() const { return instance_.get() == nullptr; }

  private:
    mutable std::shared_ptr<Type> instance_;
};

template <typename Type, typename Factory>
class storage_instance<shared, Type*, Factory> : public storage_instance<shared, std::unique_ptr<Type>, Factory> {
  public:
    template <typename... Args>
    storage_instance(Args&&... args)
        : storage_instance<shared, std::unique_ptr<Type>, Factory>(std::forward<Args>(args)...) {}

    template <typename Context> void construct(Context& context) {
        storage_instance<shared, std::unique_ptr<Type>, Factory>::construct(context);
    }

    Type* get() const { return storage_instance<shared, std::unique_ptr<Type>, Factory>::get().get(); }
};

template <typename Type, typename Factory, typename Container, typename Conversions>
class storage<shared, Type, Factory, Container, Conversions> : public resettable_i {
    // TODO
    // static_assert(std::is_trivially_destructible_v< Type > ==
    // std::is_trivially_destructible_v< storage_instance< Type, shared > >);
    storage_instance<shared, Type, Factory> instance_;

  public:
    template <typename... Args> storage(Args&&... args) : instance_(std::forward<Args>(args)...) {}

    static constexpr bool is_caching = true;

    using conversions = Conversions;
    using type = Type;

    template <typename Context> auto resolve(Context& context) -> decltype(instance_.get()) {
        if (instance_.empty())
            instance_.construct(context);
        return instance_.get();
    }

    bool is_resolved() const { return !instance_.empty(); }
    void reset() override { instance_.reset(); }
};
} // namespace detail
} // namespace dingo