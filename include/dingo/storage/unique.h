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
#if (DINGO_CLASS_FACTORY_CONSERVATIVE == 1)
    using lvalue_reference_types = type_list<>;
#else
    using lvalue_reference_types = type_list<U&>;
#endif
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

template <typename Type, typename Conversions> class storage<unique, Type, void, Conversions> {
  public:
    static constexpr bool is_caching = false;

    using conversions = Conversions;
    using type = Type;

    template <typename Context> Type resolve(Context& context) {
        return class_factory<decay_t<Type>>::template construct<Type, constructor_argument<decay_t<Type>, Context>>(
            context);
    }
};
} // namespace dingo