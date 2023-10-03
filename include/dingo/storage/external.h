#pragma once

#include <dingo/config.h>

#include <dingo/decay.h>
#include <dingo/factory/constructor.h>
#include <dingo/storage.h>
#include <dingo/type_list.h>

namespace dingo {
struct external {};

namespace detail {
template <typename Type, typename U> struct conversions<external, Type, U> {
    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
};

template <typename Type, typename U>
struct conversions<external, Type&, U> : public conversions<external, Type, U> {
};

template <typename Type, typename U>
struct conversions<external, Type*, U> : public conversions<external, Type, U> {
};

template <typename Type, typename U>
struct conversions<external, std::shared_ptr<Type>, U> {
    using value_types = type_list<std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<U&, std::shared_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, std::shared_ptr<U>*>;
};

template <typename Type, typename U>
struct conversions<external, std::shared_ptr<Type>&, U>
    : public conversions<external, std::shared_ptr<Type>, U> {};

template <typename Type, typename U>
struct conversions<external, std::unique_ptr<Type>, U> {
    using value_types = type_list<>;
    using pointer_types = type_list<U*, std::unique_ptr<U>*>;
    using lvalue_reference_types = type_list<U&, std::unique_ptr<U>&>;
    using rvalue_reference_types = type_list<>;
};

template <typename Type, typename U>
struct conversions<external, std::unique_ptr<Type>&, U>
    : public conversions<external, std::unique_ptr<Type>, U> {};

template <typename Type> class storage_instance<external, Type, void> {
  public:
    template <typename T>
    storage_instance(T&& instance) : instance_(std::forward<T>(instance)) {}

    Type& get() { return instance_; }

  private:
    Type instance_;
};

template <typename Type> class storage_instance<external, Type&, void> {
  public:
    storage_instance(Type& instance) : instance_(instance) {}

    Type& get() { return instance_; }

  private:
    Type& instance_;
};

template <typename Type> class storage_instance<external, Type*, void> {
  public:
    storage_instance(Type* instance) : instance_(instance) {}

    Type* get() { return instance_; }

  private:
    Type* instance_;
};

template <typename Type>
class storage_instance<external, std::shared_ptr<Type>, void> {
  public:
    template <typename T>
    storage_instance(T&& instance) : instance_(std::forward<T>(instance)) {}

    std::shared_ptr<Type> get() { return instance_; }

  private:
    std::shared_ptr<Type> instance_;
};

template <typename Type>
class storage_instance<external, std::shared_ptr<Type>&, void> {
  public:
    storage_instance(std::shared_ptr<Type>& instance) : instance_(instance) {}

    std::shared_ptr<Type> get() { return instance_; }

  private:
    std::shared_ptr<Type>& instance_;
};

template <typename Type>
class storage_instance<external, std::unique_ptr<Type>, void> {
  public:
    template <typename T>
    storage_instance(T&& instance) : instance_(std::forward<T>(instance)) {}

    std::unique_ptr<Type>& get() { return instance_; }

  private:
    std::unique_ptr<Type> instance_;
};

template <typename Type, typename Factory, typename Conversions>
class storage<external, Type, Factory, Conversions> : public resettable_i {
    storage_instance<external, Type, void> instance_;

  public:
    static constexpr bool is_caching = false;

    using conversions = Conversions;
    using type = Type;

    template <typename T>
    storage(T&& instance) : instance_(std::forward<T>(instance)) {}

    template <typename Context, typename Container>
    auto resolve(Context&, Container&) -> decltype(instance_.get()) {
        return instance_.get();
    }
    constexpr bool is_resolved() const { return true; }

    void reset() override {}
};
} // namespace detail
} // namespace dingo
