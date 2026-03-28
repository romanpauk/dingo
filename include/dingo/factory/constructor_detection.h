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
#include <dingo/normalized_type.h>
#include <dingo/factory/constructor_typedef.h>
#include <dingo/type_list.h>

#include <tuple>

namespace dingo {

namespace detail {
struct reference {};
struct reference_exact {};
struct value {};
struct automatic {};

template <typename DisabledType, typename T>
using reference_exact_resolvable_t = std::enable_if_t<
    !std::is_same_v<DisabledType, std::decay_t<T>> &&
    is_reference_resolvable_v<std::decay_t<T>>>;

template <typename DisabledType, typename T>
using reference_borrowable_t = std::enable_if_t<
    !std::is_same_v<DisabledType, std::decay_t<T>>>;

template <class DisabledType, typename Tag> struct constructor_argument;

template <class DisabledType>
struct constructor_argument<DisabledType, reference> {
    using tag_type = reference;

    template <typename T, typename = reference_borrowable_t<DisabledType, T>>
    operator T&();

    template <typename T, typename = reference_borrowable_t<DisabledType, T>>
    operator T&&();
};

template <class DisabledType>
struct constructor_argument<DisabledType, reference_exact> {
    using tag_type = reference_exact;

    template <typename T, typename = reference_exact_resolvable_t<DisabledType, T>>
    operator T();
};

template <class DisabledType>
struct constructor_argument<DisabledType, value> {
    using tag_type = value;

    template <typename T, typename = typename std::enable_if_t<!std::is_same_v<
                              DisabledType, typename std::decay_t<T>>>>
    operator T();
};

template <class DisabledType>
struct constructor_argument<DisabledType, automatic> {
    using tag_type = automatic;

    template <
        typename T,
        typename = typename std::enable_if_t< !std::is_same_v<DisabledType, std::decay_t<T>> >
    >
    operator T&&() const;

    template <
        typename T,
        typename = typename std::enable_if_t< !std::is_same_v<DisabledType, std::decay_t<T>> >
    >
    operator const T&() const;

    template <
        typename T,
        typename = typename std::enable_if_t< !std::is_same_v<DisabledType, std::decay_t<T>> >
    >
    operator T&() const;

    template <
        typename T,
        typename = typename std::enable_if_t< !std::is_same_v<DisabledType, std::decay_t<T>> >
    >
    operator T();
};

template <typename DisabledType, typename Context, typename Container,
          typename Tag>
class constructor_argument_impl;

template <typename DisabledType, typename Context, typename Container>
class constructor_argument_impl<DisabledType, Context, Container, reference> {
  public:
    constructor_argument_impl(Context& context, Container& container)
        : context_(context), container_(container) {}

    template <typename T, typename = reference_borrowable_t<DisabledType, T>>
    operator T*() {
        return context_.template resolve<T*>(container_);
    }

    template <typename T, typename = reference_borrowable_t<DisabledType, T>>
    operator T&() {
        return context_.template resolve<T&>(container_);
    }

    template <typename T, typename = reference_borrowable_t<DisabledType, T>>
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

template <typename DisabledType, typename Context, typename Container>
class constructor_argument_impl<DisabledType, Context, Container, reference_exact> {
  public:
    constructor_argument_impl(Context& context, Container& container)
        : context_(context), container_(container) {}

    template <typename T, typename = reference_exact_resolvable_t<DisabledType, T>>
    operator T() {
        return context_.template resolve<std::decay_t<T>>(container_);
    }

  private:
    Context& context_;
    Container& container_;
};

template <typename DisabledType, typename Context, typename Container>
class constructor_argument_impl<DisabledType, Context, Container, value> {
  public:
    constructor_argument_impl(Context& context, Container& container)
        : context_(context), container_(container) {}

    template <typename T, typename = std::enable_if_t<
                              !std::is_same_v<DisabledType, std::decay_t<T>>>>
    operator T() {
        return context_.template resolve<T>(container_);
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

template <typename DisabledType, typename Context, typename Container>
class constructor_argument_impl<DisabledType, Context, Container, automatic> {
  public:
    constructor_argument_impl(Context& context, Container& container)
        : context_(context), container_(container) {}

    template <
        typename T,
        typename = typename std::enable_if_t< !std::is_same_v<DisabledType, std::decay_t<T>> >
    >
    operator T&&() const {
        return context_.template resolve<T&&>(container_);
    }

    template <
        typename T,
        typename = typename std::enable_if_t< !std::is_same_v<DisabledType, std::decay_t<T>> >
    >
    operator const T&() const {
        return context_.template resolve<const T&>(container_);
    }

    template <
        typename T,
        typename = typename std::enable_if_t< !std::is_same_v<DisabledType, std::decay_t<T>> >
    >
    operator T&() const {
        return context_.template resolve<T&>(container_);
    }

    template <
        typename T,
        typename = typename std::enable_if_t< !std::is_same_v<DisabledType, std::decay_t<T>> >
    >
    operator T() {
        return context_.template resolve<T>(container_);
    }

    template <typename T, typename Tag,
              typename = std::enable_if_t< !std::is_same_v<DisabledType, std::decay_t<T>>> >
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

template <typename T, typename... Args>
inline constexpr bool is_list_initializable_v = list_initialization<T, Args...>{};

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

template <typename T, typename... Args>
inline constexpr bool is_direct_initializable_v = direct_initialization<T, Args...>{};

template <typename T, typename Tag, typename Sequence>
struct detection_arguments;

template <typename T, typename Tag, size_t... Is>
struct detection_arguments<T, Tag, std::index_sequence<Is...>> {
    using type = std::tuple<std::conditional_t<true, constructor_argument<T, Tag>,
                                               std::integral_constant<size_t, Is>>...>;
};

template <typename T, template <typename...> typename IsConstructible,
          typename Tuple>
struct detected_constructor_arguments;

template <typename T, template <typename...> typename IsConstructible,
          typename Tuple, bool Constructible>
struct detected_constructor_arguments_impl;

template <typename T, template <typename...> typename IsConstructible,
          typename... Args>
struct detected_constructor_arguments_impl<T, IsConstructible,
                                          std::tuple<Args...>, true> {
    static constexpr bool valid = true;
    using type = std::tuple<Args...>;
};

template <typename T, template <typename...> typename IsConstructible>
struct detected_constructor_arguments_impl<T, IsConstructible, std::tuple<>, false> {
    using type = std::tuple<>;
    static constexpr bool valid = false;
};

template <typename T, template <typename...> typename IsConstructible,
          typename Head, typename... Tail>
struct detected_constructor_arguments_impl<T, IsConstructible,
                                          std::tuple<Head, Tail...>, false>
    : detected_constructor_arguments_impl<
          T, IsConstructible, std::tuple<Tail...>,
          IsConstructible<T, Tail...>::value> {};

template <typename T, template <typename...> typename IsConstructible,
          typename... Args>
struct detected_constructor_arguments<T, IsConstructible, std::tuple<Args...>>
    : detected_constructor_arguments_impl<T, IsConstructible, std::tuple<Args...>,
                                          IsConstructible<T, Args...>::value> {
};

// Generates N arguments of type class_factory_argument<T>, going 1, 2, 3... N.
template <typename T, typename Tag, template <typename...> typename IsConstructible,
          bool Assert = true, size_t N = DINGO_CONSTRUCTOR_DETECTION_ARGS>
struct constructor_detection;

template <typename T, typename... Args> struct constructor_methods;
template <typename T, typename... Args>
struct constructor_methods<T, std::tuple<Args...>> {
    using arguments = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool valid = true;

    static_assert(detail::is_list_initializable_v<T, Args...> ||
        detail::is_direct_initializable_v<T, Args...>);

    template <typename Type, typename Context, typename Container>
    static auto construct(Context& ctx, Container& container) {
        return detail::construction_dispatch<Type, T>::construct(
            ((void)sizeof(Args),
             constructor_argument_impl<T, Context, Container,
                                       typename Args::tag_type>(ctx,
                                                                container))...);
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        detail::construction_dispatch<Type, T>::construct(
            ptr, ((void)sizeof(Args),
                  constructor_argument_impl<T, Context, Container,
                                            typename Args::tag_type>(
                      ctx, container))...);
    }
};

template <typename T, typename Tag, template <typename...> typename IsConstructible,
          bool Assert, size_t N>
struct constructor_detection {
  private:
    using candidates =
        typename detection_arguments<T, Tag, std::make_index_sequence<N>>::type;
    using detection =
        detected_constructor_arguments<T, IsConstructible, candidates>;
    using selected_arguments = typename detection::type;

  public:
    static constexpr size_t arity = std::tuple_size_v<selected_arguments>;
    static constexpr bool valid = detection::valid;

    static_assert(!Assert || valid,
                  "class T construction not detected or ambiguous");

    template <typename Type, typename Context, typename Container>
    static auto construct(Context& ctx, Container& container) {
        return constructor_methods<T, selected_arguments>::template construct<Type>(
            ctx, container);
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        constructor_methods<T, selected_arguments>::template construct<Type>(
            ptr, ctx, container);
    }
};

template <typename T, template <typename...> typename IsConstructible,
          bool Assert, size_t N>
struct constructor_detection<T, reference, IsConstructible, Assert, N> {
  private:
    template <size_t Arity>
    using borrow_candidates =
        typename detection_arguments<T, reference, std::make_index_sequence<Arity>>::type;

    template <size_t Arity>
    using value_candidates =
        typename detection_arguments<T, reference_exact, std::make_index_sequence<Arity>>::type;

    template <size_t Arity>
    using borrow_detection =
        detected_constructor_arguments<T, IsConstructible, borrow_candidates<Arity>>;

    template <size_t Arity>
    using value_detection =
        detected_constructor_arguments<T, IsConstructible, value_candidates<Arity>>;

    template <size_t Arity, typename = void>
    struct selected_arguments_impl;

    template <typename Borrow, typename Value>
    using preferred_arguments = std::conditional_t<
        (std::tuple_size_v<typename Borrow::type> >=
         std::tuple_size_v<typename Value::type>),
        typename Borrow::type,
        typename Value::type>;

    template <typename Dummy>
    struct selected_arguments_impl<0, Dummy> {
        using borrow = borrow_detection<0>;
        using value = value_detection<0>;

        using type = preferred_arguments<borrow, value>;
        static constexpr bool valid = borrow::valid || value::valid;
    };

    template <size_t Arity>
    struct selected_arguments_impl<Arity, std::enable_if_t<(Arity > 0)>> {
        using borrow = borrow_detection<Arity>;
        using value = value_detection<Arity>;
        using next = selected_arguments_impl<Arity - 1>;

        using current_type = preferred_arguments<borrow, value>;
        using type = std::conditional_t<
            (std::tuple_size_v<current_type> > 0),
            current_type,
            typename next::type>;
        static constexpr bool valid =
            borrow::valid || value::valid || next::valid;
    };

    // `detail::reference` supports the normal borrow-based constructor
    // detection plus the exact by-value wrapper path. Both remain linear in
    // constructor arity; the old mixed borrow/value combination search was
    // removed because it blew up compile-time memory usage.
    using selected = selected_arguments_impl<N>;
    using selected_arguments = typename selected::type;

  public:
    static constexpr size_t arity = std::tuple_size_v<selected_arguments>;
    static constexpr bool valid = selected::valid;

    static_assert(!Assert || valid,
                  "class T construction not detected or ambiguous");

    template <typename Type, typename Context, typename Container>
    static auto construct(Context& ctx, Container& container) {
        return constructor_methods<T, selected_arguments>::template construct<Type>(
            ctx, container);
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        constructor_methods<T, selected_arguments>::template construct<Type>(
            ptr, ctx, container);
    }
};

} // namespace detail

template <typename T, typename DetectionType = detail::automatic> struct constructor_detection
    : std::conditional_t<
        has_constructor_typedef_v<T>,
        constructor_typedef<T>,
        detail::constructor_detection<T, DetectionType, detail::list_initialization>
    >
{};

} // namespace dingo
