#pragma once

#include <dingo/config.h>

#include <dingo/constructible_i.h>
#include <dingo/decay.h>
#include <dingo/factory/constructor.h>
#include <dingo/storage.h>

#include <atomic>
#include <memory>

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
template <typename Type, typename U>
struct conversions<shared_cyclical, Type, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
};

template <typename Type, typename U>
struct conversions<shared_cyclical, Type*, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
};

template <typename Type, typename U>
struct conversions<shared_cyclical, std::shared_ptr<Type>, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&, std::shared_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::shared_ptr<U>*>;
};

// Disallow virtual bases as interfaces as in cyclical storage, we can't
// properly calculate the cast when the object is not constructed
template <typename Type, typename Factory, typename Conversions,
          typename Derived, typename Base>
struct storage_interface_requirements<
    storage<shared_cyclical, Type, Factory, Conversions>, Derived, Base>
    : std::bool_constant<!is_virtual_base_of_v<Base, Derived>> {};

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
    std::aligned_storage_t<sizeof(Type), alignof(Type)> instance_;
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
    std::aligned_storage_t<sizeof(Type), alignof(Type)> instance_;
    bool resolved_ = false;
};

template <typename Type, typename Factory>
class storage_instance<shared_cyclical, Type, Factory>
    : public storage_instance_shared_impl<Type, Factory> {
  public:
    template <typename... Args>
    storage_instance(Args&&... args)
        : storage_instance_shared_impl<Type, Factory>(
              std::forward<Args>(args)...) {}
};

template <typename Type, typename Factory>
class storage_instance<shared_cyclical, std::shared_ptr<Type>, Factory>
    : Factory {
    struct alignas(alignof(Type)) buffer {
        uint8_t data[sizeof(Type)];
    };

    class deleter {
      public:
        void set_constructed() { constructed_ = true; }
        bool is_constructed() const { return constructed_; }

        void operator()(Type* ptr) {
            if constexpr (!std::is_trivially_destructible_v<Type>) {
                if (constructed_)
                    ptr->~Type();
            }

            delete reinterpret_cast<buffer*>(ptr);
        }

      private:
        bool constructed_ = false;
    };

  public:
    template <typename... Args>
    storage_instance(Args&&... args) : Factory(std::forward<Args>(args)...) {}

    template <typename Context> std::shared_ptr<Type> resolve(Context&) {
        assert(!instance_);
        instance_.reset(reinterpret_cast<Type*>(new buffer), deleter());
        return instance_;
    }

    std::shared_ptr<Type> get() { return instance_; }

    template <typename Context, typename Container>
    void construct(Context& context, Container& container) {
        assert(instance_);
        Factory::template construct<Type*>(instance_.get(), context, container);
        std::get_deleter<deleter>(instance_)->set_constructed();
    }

    bool empty() const { return !instance_; }

    void reset() { instance_.reset(); }

  private:
    std::shared_ptr<Type> instance_;
};

template <typename Instance, typename Context, typename Container>
struct shared_cyclical_constructible : constructible_i {
    shared_cyclical_constructible(Instance& instance, Context& context,
                                  Container& container)
        : instance_(instance), context_(context), container_(container) {}

    void construct() override { instance_.construct(context_, container_); }

  private:
    Instance& instance_;
    Context& context_;
    Container& container_;
};

template <typename Type, typename Factory, typename Conversions>
class storage<shared_cyclical, Type, Factory, Conversions>
    : public resettable_i {
    storage_instance<shared_cyclical, Type, Factory> instance_;

    // TODO: get rid of heap
    std::unique_ptr<constructible_i> constructible_;

  public:
    template <typename... Args>
    storage(Args&&... args) : instance_(std::forward<Args>(args)...) {}

    static constexpr bool is_caching = true;

    using conversions = Conversions;
    using type = Type;

    template <typename Context, typename Container>
    auto resolve(Context& context, Container& container)
        -> decltype(instance_.get()) {
        if (instance_.empty()) {
            constructible_.reset(
                new shared_cyclical_constructible<decltype(instance_), Context,
                                                  Container>(instance_, context,
                                                             container));
            context.register_constructible(constructible_.get());
            return instance_.resolve(context);
        }
        return instance_.get();
    }

    bool is_resolved() const { return !instance_.empty(); }

    void reset() override {
        instance_.reset();
        constructible_.reset();
    }
};
} // namespace detail
} // namespace dingo
