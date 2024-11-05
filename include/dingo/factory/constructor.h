//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/annotated.h>
#include <dingo/class_traits.h>
#include <dingo/decay.h>
#include <dingo/type_list.h>

namespace dingo {
template <typename...> struct constructor;

namespace detail {
struct reference {};
struct const_reference {};

template <class DisabledType, typename Tag> struct constructor_argument;

template <class DisabledType>
struct constructor_argument<DisabledType, reference> {
    using tag_type = reference;

    template <typename T, typename = typename std::enable_if_t<!std::is_same_v<
                              DisabledType, typename std::decay_t<T>>>>
    operator T&();

    template <typename T, typename = typename std::enable_if_t<!std::is_same_v<
                              DisabledType, typename std::decay_t<T>>>>
    operator T*();

    template <typename T, typename = typename std::enable_if_t<!std::is_same_v<
                              DisabledType, typename std::decay_t<T>>>>
    operator T&&();
};

template <class DisabledType>
struct constructor_argument<DisabledType, const_reference> {
    using tag_type = reference;

    template <typename T, typename = typename std::enable_if_t<!std::is_same_v<
                              DisabledType, typename std::decay_t<T>>>>
    operator const T&();
};

template <typename DisabledType, typename Context, typename Container,
          typename Tag>
class constructor_argument_impl;

template <typename DisabledType, typename Context, typename Container>
class constructor_argument_impl<DisabledType, Context, Container, reference> {
  public:
    constructor_argument_impl(Context& context, Container& container)
        : context_(context), container_(container) {}

    template <typename T, typename = std::enable_if_t<
                              !std::is_same_v<DisabledType, std::decay_t<T>>>>
    operator T*() {
        return context_.template resolve<T*>(container_);
    }

    template <typename T, typename = std::enable_if_t<
                              !std::is_same_v<DisabledType, std::decay_t<T>>>>
    operator T&() {
        return context_.template resolve<T&>(container_);
    }

    template <typename T, typename = std::enable_if_t<
                              !std::is_same_v<DisabledType, std::decay_t<T>>>>
    operator const T&() {
        return context_.template resolve<const T&>(container_);
    }

    template <typename T, typename = std::enable_if_t<
                              !std::is_same_v<DisabledType, std::decay_t<T>>>>
    operator T&&() {
        return context_.template resolve<T&&>(container_);
    }

    template <typename T, typename Tag,
              typename = std::enable_if_t<
                  !std::is_same_v<DisabledType, std::decay_t<T>>>>
    operator annotated<T, Tag>() {
        return context_.template resolve<annotated<T, Tag>>(container_);
    }

  private:
    Context& context_;
    Container& container_;
};

template <typename T, typename = void, typename... Args>
struct list_initialization_impl : std::false_type {};

template <typename T, typename... Args>
struct list_initialization_impl<
    T, std::void_t<decltype(T{std::declval<Args>()...})>, Args...>
    : std::true_type {};

template <typename T, typename... Args>
struct list_initialization : list_initialization_impl<T, void, Args...> {};

template <typename T, typename... Args>
struct list_initialization<T, std::tuple<Args...>>
    : list_initialization<T, Args...> {};

// Filters out T(T&).
// TODO: It should be possible to write all into single class
template <typename T, typename Arg>
struct list_initialization<T, Arg>
    : std::conjunction<list_initialization_impl<T, void, Arg>,
                       std::negation<std::is_same<std::decay_t<Arg>, T>>> {};

template <typename T, typename = void, typename... Args>
struct direct_initialization_impl : std::false_type {};

template <typename T, typename... Args>
struct direct_initialization_impl<
    T, std::void_t<decltype(T(std::declval<Args>()...))>, Args...>
    : std::true_type {};

template <typename T, typename... Args>
struct direct_initialization : direct_initialization_impl<T, void, Args...> {};

template <typename T, typename... Args>
struct direct_initialization<T, std::tuple<Args...>>
    : direct_initialization<T, Args...> {};

// Filters out T(T&).
// TODO: It should be possible to write all into single class
template <typename T, typename Arg>
struct direct_initialization<T, Arg>
    : std::conjunction<direct_initialization_impl<T, void, Arg>,
                       std::negation<std::is_same<std::decay_t<Arg>, T>>> {};

template <typename T, template <typename...> typename IsConstructible,
          typename Tuple, typename Value, size_t I = 0,
          bool = std::tuple_size_v<Tuple> == I>
struct constructor_arguments_rewrite
    : std::conditional_t<
          // If T is constructible using modified args,
          IsConstructible<T,
                          typename tuple_replace<Tuple, I, Value>::type>::value,
          // Continue with modified args
          constructor_arguments_rewrite<
              T, IsConstructible, typename tuple_replace<Tuple, I, Value>::type,
              Value, I + 1>,
          // Else continue with unmodified args
          constructor_arguments_rewrite<T, IsConstructible, Tuple, Value,
                                        I + 1>> {};

template <typename T, template <typename...> typename IsConstructible,
          typename Tuple, typename Value, size_t I>
struct constructor_arguments_rewrite<T, IsConstructible, Tuple, Value, I,
                                     true> {
    // This is the final modified tuple that can be constructed
    using type = Tuple;
};

template <typename T, template <typename...> typename IsConstructible,
          bool Assert, size_t N, bool Constructible, typename... Args>
struct constructor_detection_impl;

// Generates N arguments of type class_factory_argument<T>, going 1, 2, 3... N.
template <typename T, template <typename...> typename IsConstructible,
          bool Assert = true, size_t N = DINGO_CONSTRUCTOR_DETECTION_ARGS,
          typename = void, typename... Args>
struct constructor_detection
    : constructor_detection<T, IsConstructible, Assert, N, void,
                            constructor_argument<T, reference>, Args...> {};

// Upon reaching N, generates N attempts to instantiate, going N, N-1, N-2... 0
// arguments. This assures that container will select the construction method
// with the most arguments as it will be the first seen in the hierarchy (this
// is needed to fill default-constructible aggregate types with instances from
// container).
template <typename T, template <typename...> typename IsConstructible,
          bool Assert, size_t N, typename... Args>
struct constructor_detection<T, IsConstructible, Assert, N,
                             typename std::enable_if_t<sizeof...(Args) == N>,
                             Args...>
    : constructor_detection_impl<T, IsConstructible, Assert, N,
                                 IsConstructible<T, Args...>::value,
                                 std::tuple<Args...>> {};

template <typename T, typename... Args> struct constructor_factory_methods;
template <typename T, typename... Args>
struct constructor_factory_methods<T, std::tuple<Args...>> {
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool valid = true;

    template <typename Type, typename Context, typename Container>
    static Type construct(Context& ctx, Container& container) {
        return class_traits<Type>::construct(
            ((void)sizeof(Args),
             constructor_argument_impl<T, Context, Container,
                                       typename Args::tag_type>(ctx,
                                                                container))...);
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        class_traits<Type>::construct(
            ptr, ((void)sizeof(Args),
                  constructor_argument_impl<T, Context, Container,
                                            typename Args::tag_type>(
                      ctx, container))...);
    }
};

//
// Construction was found. Bail out (by not inheriting detection anymore).
// Try to rewrite detected T&/T&& into T (if that still constructs). Construct
// construct() factories with final rewritten constructor args.
// TODO: try to rewrite detected T&/T&& into const T&, as:
//  1) T& and T&& would be deduced correctly, so they will be passed directly.
//  2) T and const T& would be both deduced as const T&. They will require
//  context in destructor in resolve<>.
// TODO: add a DINGO_INJECT() macro to be able to get 100% correct injection if
// required.
//

template <typename T, template <typename...> typename IsConstructible,
          bool Assert, size_t N, typename... Args>
struct constructor_detection_impl<T, IsConstructible, Assert, N, true,
                                  std::tuple<Args...>>
    : constructor_factory_methods<
          T,
          // TODO: rewrite is now disabled (note that it is post-rewrite
          // after constructibility check).
          //typename constructor_arguments_rewrite<
          //     T, IsConstructible, std::tuple<Args...>,
          //     constructor_argument<T, value>>::type
          std::tuple<Args...>
          > {};

// Construction was not found. Generate next level of inheritance with one less
// argument.
template <typename T, template <typename...> typename IsConstructible,
          bool Assert, size_t N, typename Head, typename... Tail>
struct constructor_detection_impl<T, IsConstructible, Assert, N, false,
                                  std::tuple<Head, Tail...>>
    : constructor_detection_impl<T, IsConstructible, Assert, N,
                                 IsConstructible<T, Tail...>::value,
                                 std::tuple<Tail...>> {};

// Construction was not found, and no more arguments can be removed.
template <typename T, template <typename...> typename IsConstructible,
          bool Assert, size_t N>
struct constructor_detection_impl<T, IsConstructible, Assert, N, false,
                                  std::tuple<>> {
    static constexpr bool valid = false;
    static_assert(!Assert || valid,
                  "class T construction not detected or ambiguous");
};

template <typename T, typename = void>
struct has_constructor_typedef_type : std::false_type {};

template <typename T>
struct has_constructor_typedef_type<
    T, typename std::void_t<typename T::dingo_constructor_type>>
    : std::true_type {};

template <typename T>
static constexpr bool has_constructor_typedef_type_v =
    has_constructor_typedef_type<T>::value;

template <typename T, bool = has_constructor_typedef_type_v<T>>
struct constructor_typedef_impl;

template <typename T>
struct constructor_typedef_impl<T, true> : T::dingo_constructor_type {};

template <typename T>
struct constructor_typedef_impl<T, false>
    : constructor_detection<T, detail::list_initialization> {};

template <typename T>
struct constructor_typedef : constructor_typedef_impl<T> {};

} // namespace detail

template <typename T, typename... Args> struct constructor<T(Args...)> {
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool valid = std::is_constructible_v<T, Args...>;

    template <typename Type, typename Context, typename Container>
    static Type construct(Context& ctx, Container& container) {
        return class_traits<Type>::construct(
            ctx.template resolve<Args>(container)...);
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        class_traits<Type>::construct(ptr,
                                      ctx.template resolve<Args>(container)...);
    }
};

template <typename T, typename... Args>
struct constructor<T, Args...> : constructor<T(Args...)> {};

template <typename T> struct constructor<T> : detail::constructor_typedef<T> {};

} // namespace dingo
