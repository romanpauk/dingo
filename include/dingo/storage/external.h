#pragma once

#include <dingo/config.h>

#include <dingo/class_factory.h>
#include <dingo/decay.h>
#include <dingo/storage.h>
#include <dingo/type_list.h>

namespace dingo {
struct external {};

template <typename Type, typename U> struct conversions<external, Type, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
};

template <typename Type, typename U> struct conversions<external, Type&, U> : public conversions<external, Type, U> {};

template <typename Type, typename U> struct conversions<external, Type*, U> : public conversions<external, Type, U> {};

template <typename Type, typename U> struct conversions<external, std::shared_ptr<Type>, U> {
    using value_types = type_list<std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<U&, std::shared_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::shared_ptr<U>*>;
};

template <typename Type, typename U>
struct conversions<external, std::shared_ptr<Type>&, U> : public conversions<external, std::shared_ptr<Type>, U> {};

template <typename Type, typename U> struct conversions<external, std::unique_ptr<Type>, U> {
    using value_types = type_list<>;
    using pointer_types = type_list<U*, std::unique_ptr<U>*>;
    using lvalue_reference_types = type_list<U&, std::unique_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
};

template <typename Type, typename U>
struct conversions<external, std::unique_ptr<Type>&, U> : public conversions<external, std::unique_ptr<Type>, U> {};

template <typename Type> class storage_instance<Type, external> {
  public:
    template <typename T> storage_instance(T&& instance) : instance_(std::forward<T>(instance)) {}

    Type& get() { return instance_; }

  private:
    Type instance_;
};

template <typename Type> class storage_instance<Type&, external> {
  public:
    storage_instance(Type& instance) : instance_(instance) {}

    Type& get() { return instance_; }

  private:
    Type& instance_;
};

template <typename Type> class storage_instance<Type*, external> {
  public:
    storage_instance(Type* instance) : instance_(instance) {}

    Type* get() { return instance_; }

  private:
    Type* instance_;
};

template <typename Type> class storage_instance<std::shared_ptr<Type>, external> {
  public:
    template <typename T> storage_instance(T&& instance) : instance_(std::forward<T>(instance)) {}

    std::shared_ptr<Type> get() { return instance_; }

  private:
    std::shared_ptr<Type> instance_;
};

template <typename Type> class storage_instance<std::shared_ptr<Type>&, external> {
  public:
    storage_instance(std::shared_ptr<Type>& instance) : instance_(instance) {}

    std::shared_ptr<Type> get() { return instance_; }

  private:
    std::shared_ptr<Type>& instance_;
};

template <typename Type> class storage_instance<std::unique_ptr<Type>, external> {
  public:
    template <typename T> storage_instance(T&& instance) : instance_(std::forward<T>(instance)) {}

    std::unique_ptr<Type>& get() { return instance_; }

  private:
    std::unique_ptr<Type> instance_;
};

template <typename Type, typename Conversions> class storage<external, Type, void, Conversions> : public resettable_i {
    storage_instance<Type, external> instance_;

  public:
    static constexpr bool is_caching = false;

    using conversions = Conversions;
    using type = Type;

    template <typename T> storage(T&& instance) : instance_(std::forward<T>(instance)) {}

    template <typename Context> auto resolve(Context&) -> decltype(instance_.get()) { return instance_.get(); }
    constexpr bool is_resolved() const { return true; }

    void reset() override {}
};
} // namespace dingo
