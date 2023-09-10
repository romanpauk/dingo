#pragma once

#include <dingo/config.h>

#include <dingo/class_factory.h>
#include <dingo/decay.h>
#include <dingo/storage.h>
#include <dingo/type_list.h>

namespace dingo {
struct unique {};

template <typename Type, typename U> struct conversions<unique, Type, U> {
    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<U&&>;
    using pointer_types = type_list<>;
};

template <typename Type, typename U> struct conversions<unique, Type*, U> {
    using value_types = type_list<>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
};

template <typename Type, typename U> struct conversions<unique, std::shared_ptr<Type>, U> {
    using value_types = type_list<std::shared_ptr<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<std::shared_ptr<U>&&>;
    using pointer_types = type_list<>;
};

template <typename Type, typename U> struct conversions<unique, std::unique_ptr<Type>, U> {
    using value_types = type_list<std::unique_ptr<U>>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<std::unique_ptr<U>&&>;
    using pointer_types = type_list<>;
};

template <typename Type, typename Factory, typename Conversions>
class storage<unique, Type, Factory, void, Conversions> {
  public:
    static constexpr bool is_caching = false;

    using conversions = Conversions;
    using type = Type;

    template <typename Context> Type resolve(Context& context) {
        return storage_factory_t<Factory, Type, Context>::template construct<Type>(context);
    }
};
} // namespace dingo