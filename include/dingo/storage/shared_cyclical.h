#pragma once

#include <dingo/config.h>

#include <dingo/class_factory.h>
#include <dingo/constructible_i.h>
#include <dingo/decay.h>
#include <dingo/storage.h>

#include <atomic>
#include <memory>

namespace dingo {
struct shared_cyclical {};

template <typename Type, typename U> struct conversions<shared_cyclical, Type, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
};

template <typename Type, typename U> struct conversions<shared_cyclical, Type*, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
};

template <typename Type, typename U> struct conversions<shared_cyclical, std::shared_ptr<Type>, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&, std::shared_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::shared_ptr<U>*>;
};

template <typename Type> class storage_instance<Type, shared_cyclical> {
  public:
    ~storage_instance() {
        // TODO: should not have dtor for trivially-destructible Type
        reset();
    }

    template <typename Context> Type* resolve(Context&) {
        assert(!resolved_);
        resolved_ = true;
        return get();
    }

    Type* get() { return reinterpret_cast<Type*>(&instance_); }

    template <typename Context> void construct(Context& context) {
        class_factory<decay_t<Type>>::template construct<Type*, constructor_argument<Type, Context>>(&instance_,
                                                                                                     context);
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

template <typename Type> class storage_instance<std::shared_ptr<Type>, shared_cyclical> {
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
    template <typename Context> std::shared_ptr<Type> resolve(Context&) {
        assert(!instance_);
        instance_.reset(reinterpret_cast<Type*>(new buffer), deleter());
        return instance_;
    }

    std::shared_ptr<Type> get() { return instance_; }

    template <typename Context> void construct(Context& context) {
        assert(instance_);
        class_factory<decay_t<Type>>::template construct<Type*, constructor_argument<Type, Context>>(instance_.get(),
                                                                                                     context);
        std::get_deleter<deleter>(instance_)->set_constructed();
    }

    bool empty() const { return !instance_; }

    void reset() { instance_.reset(); }

  private:
    std::shared_ptr<Type> instance_;
};

template <typename Type, typename Conversions, typename Container>
class storage<shared_cyclical, Type, Container, Conversions> : public resettable_i, public constructible_i<Container> {
    static_assert(!std::is_same_v<Container, void>, "concrete container type required");

    storage_instance<Type, shared_cyclical> instance_;

  public:
    static constexpr bool is_caching = true;

    using conversions = Conversions;
    using type = Type;

    template <typename Context> auto resolve(Context& context) -> decltype(instance_.get()) {
        if (instance_.empty()) {
            context.register_constructible(this);
            return instance_.resolve(context);
        }
        return instance_.get();
    }

    bool is_resolved() const { return !instance_.empty(); }

    void reset() override { instance_.reset(); }

    void construct(resolving_context<Container>& context, int phase) override {
        if (phase == 0)
            instance_.construct(context);
    }
};
} // namespace dingo
