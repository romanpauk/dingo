//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/registration/annotated.h>
#include <dingo/factory/class_traits.h>
#include <dingo/type/complete_type.h>
#include <dingo/type/normalized_type.h>
#include <dingo/factory/constructor_typedef.h>

#include <type_traits>
#include <utility>

namespace dingo {

template <typename T> struct constructor_detection_traits {
    static constexpr size_t max_arity = DINGO_CONSTRUCTOR_DETECTION_ARGS;
};

namespace detail {
struct automatic {};

template <class DisabledType, typename Tag> struct constructor_argument;
template <class DisabledType, typename Tag> struct opaque_constructor_argument;

template <class DisabledType>
struct constructor_argument<DisabledType, automatic> {
    // Value and rvalue probes imply materialization, so they must not
    // participate for forward declarations. Otherwise constructor deduction
    // would pull incomplete dependencies into class-trait inspection.
    template <
        typename T,
        typename = typename std::enable_if_t<
            !std::is_same_v<DisabledType, std::decay_t<T>> &&
            is_complete<std::decay_t<T>>::value>
    >
    operator T&&() const;

    // Lvalue-reference probes stay available for incomplete types. They model
    // borrowed dependencies that can be satisfied by lookup without trying to
    // construct the forward-declared type.
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
        typename = typename std::enable_if_t<
            !std::is_same_v<DisabledType, std::decay_t<T>> &&
            is_complete<std::decay_t<T>>::value>
    >
    operator T();
};

template <class DisabledType>
struct opaque_constructor_argument<DisabledType, automatic> {
    opaque_constructor_argument() = default;
    opaque_constructor_argument(const opaque_constructor_argument&) = default;
    opaque_constructor_argument(opaque_constructor_argument&&) = default;
    opaque_constructor_argument& operator=(const opaque_constructor_argument&) =
        default;
    opaque_constructor_argument& operator=(opaque_constructor_argument&&) =
        default;
};

template <typename DisabledType, typename Context, typename Container,
          typename Tag>
class constructor_argument_impl;

template <typename DisabledType, typename Context, typename Container>
class constructor_argument_impl<DisabledType, Context, Container, automatic> {
  public:
    constructor_argument_impl(Context& context, Container& container)
        : context_(context), container_(container) {}

    // Runtime value/rvalue conversion follows the same rule as the detection
    // probes above: only complete types may be materialized here.
    template <
        typename T,
        typename = typename std::enable_if_t<
            !std::is_same_v<DisabledType, std::decay_t<T>> &&
            is_complete<std::decay_t<T>>::value>
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
        typename = typename std::enable_if_t<
            !std::is_same_v<DisabledType, std::decay_t<T>> &&
            is_complete<std::decay_t<T>>::value>
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

template <typename T, typename... Args>
using list_initialization_expr = decltype(T{std::declval<Args>()...});

template <typename T, typename... Args>
using direct_initialization_expr = decltype(T(std::declval<Args>()...));

#if defined(_MSC_VER)
template <typename T, typename = void, typename... Args>
struct list_initialization_impl : std::false_type {};

template <typename T, typename... Args>
struct list_initialization_impl<
    T, std::void_t<decltype(T{std::declval<Args>()...})>, Args...>
    : std::true_type {};

template <typename T, typename... Args>
struct list_initialization : list_initialization_impl<T, void, Args...> {};

template <typename T, typename Arg>
struct list_initialization<T, Arg>
    : std::conjunction<list_initialization_impl<T, void, Arg>,
                       std::negation<std::is_same<std::decay_t<Arg>, T>>> {};

template <typename T, typename... Args>
inline constexpr bool is_list_initializable_v =
    list_initialization<T, Args...>::value;

template <typename T, typename = void, typename... Args>
struct direct_initialization_impl : std::false_type {};

template <typename T, typename... Args>
struct direct_initialization_impl<
    T, std::void_t<decltype(T(std::declval<Args>()...))>, Args...>
    : std::true_type {};

template <typename T, typename... Args>
struct direct_initialization : direct_initialization_impl<T, void, Args...> {};

template <typename T, typename Arg>
struct direct_initialization<T, Arg>
    : std::conjunction<direct_initialization_impl<T, void, Arg>,
                       std::negation<std::is_same<std::decay_t<Arg>, T>>> {};
#else
// Detection should not treat `T(T&)` as a meaningful dependency-taking
// constructor. Filtering the copy-construction shape here keeps both list and
// direct initialization probes aligned.
template <typename T, typename... Args>
inline constexpr bool is_non_copy_constructor_argument_v =
    sizeof...(Args) != 1 || (!std::is_same_v<T, std::decay_t<Args>> && ...);

template <template <typename, typename...> typename InitExpr, typename T,
          typename = void, typename... Args>
struct initialization_impl : std::false_type {};

template <template <typename, typename...> typename InitExpr, typename T,
          typename... Args>
struct initialization_impl<InitExpr, T, std::void_t<InitExpr<T, Args...>>,
                           Args...>
    : std::bool_constant<is_non_copy_constructor_argument_v<T, Args...>> {};

template <typename T, typename... Args>
struct list_initialization
    : initialization_impl<list_initialization_expr, T, void, Args...> {};

template <typename T, typename... Args>
inline constexpr bool is_list_initializable_v =
    list_initialization<T, Args...>::value;

template <typename T, typename... Args>
struct direct_initialization
    : initialization_impl<direct_initialization_expr, T, void, Args...> {};
#endif

template <typename T, typename... Args>
inline constexpr bool is_direct_initializable_v =
    direct_initialization<T, Args...>::value;

inline constexpr size_t invalid_constructor_detection_arity =
    static_cast<size_t>(-1);

template <typename...>
inline constexpr bool always_false_v = false;

template <typename T, size_t>
using repeated_type = T;

// Keep one shared detector/search shape and vary only the per-arity probe.
// Non-MSVC compilers handle a lighter constexpr probe well, while MSVC still
// needs the older type-based form to preserve behavior.
#if defined(_MSC_VER)
template <typename T, typename Tag,
          template <class, class> class ConstructorArg,
          template <typename...> typename IsConstructible, typename Sequence>
struct constructor_probe_msvc;

template <typename T, typename Tag,
          template <class, class> class ConstructorArg,
          template <typename...> typename IsConstructible, size_t... Is>
struct constructor_probe_msvc<T, Tag, ConstructorArg, IsConstructible,
                              std::index_sequence<Is...>>
    : IsConstructible<
          T,
          std::conditional_t<true, ConstructorArg<T, Tag>,
                             std::integral_constant<size_t, Is>>...> {};

template <typename T, typename Tag,
          template <class, class> class ConstructorArg,
          template <typename...> typename IsConstructible, size_t Arity>
inline constexpr bool constructor_probe_v =
    constructor_probe_msvc<T, Tag, ConstructorArg, IsConstructible,
                           std::make_index_sequence<Arity>>::value;
#else
template <typename T, typename Tag,
          template <class, class> class ConstructorArg,
          template <typename...> typename IsConstructible, size_t... Is>
constexpr bool constructor_probe(std::index_sequence<Is...>) {
    // Non-MSVC compilers handle the lighter repeated-type placeholder probe
    // well, which avoids an extra wrapper class per arity check.
    return IsConstructible<T, repeated_type<ConstructorArg<T, Tag>, Is>...>::value;
}

template <typename T, typename Tag,
          template <class, class> class ConstructorArg,
          template <typename...> typename IsConstructible, size_t Arity>
inline constexpr bool constructor_probe_v =
    constructor_probe<T, Tag, ConstructorArg, IsConstructible>(
        std::make_index_sequence<Arity>{});
#endif

// Everything above feeds the same high-to-low arity search below, so the
// selected constructor semantics stay shared even though the probe body differs
// by compiler.
template <typename T, typename Tag,
          template <typename...> typename IsConstructible, size_t Arity,
          bool Match = constructor_probe_v<T, Tag, constructor_argument,
                                           IsConstructible, Arity>>
struct constructor_arity_detector_impl
    : constructor_arity_detector_impl<T, Tag, IsConstructible, Arity - 1> {};

template <typename T, typename Tag,
          template <typename...> typename IsConstructible, size_t Arity>
struct constructor_arity_detector_impl<T, Tag, IsConstructible, Arity, true>
    : std::integral_constant<size_t, Arity> {};

template <typename T, typename Tag,
          template <typename...> typename IsConstructible>
struct constructor_arity_detector_impl<T, Tag, IsConstructible, 0, false>
    : std::integral_constant<size_t, invalid_constructor_detection_arity> {};

template <typename T, typename Tag,
          template <typename...> typename IsConstructible>
struct constructor_arity_detector_impl<T, Tag, IsConstructible, 0, true>
    : std::integral_constant<size_t, 0> {};

template <typename T, typename Tag,
          template <typename...> typename IsConstructible, size_t Arity>
using constructor_arity_detector =
    constructor_arity_detector_impl<T, Tag, IsConstructible, Arity>;

// Searches constructor arity in the inclusive range [0, N].
template <typename T, typename Tag, template <typename...> typename IsConstructible,
          size_t N = DINGO_CONSTRUCTOR_DETECTION_ARGS>
struct constructor_detection;

template <typename T, typename Tag, size_t Arity> struct constructor_methods {
  private:
    template <typename Type, typename Context, typename Container, size_t... Is>
    static auto construct_impl(Context& ctx, Container& container,
                               std::index_sequence<Is...>) {
        // `Is...` only drives the pack expansion; the runtime construction
        // path still receives `Arity` copies of the same constructor argument
        // adapter without first materializing a type_list of placeholders.
        return detail::construction_dispatch<Type, T>::construct(
            ((void)Is, constructor_argument_impl<T, Context, Container, Tag>(
                           ctx, container))...);
    }

    template <typename Type, typename Context, typename Container, size_t... Is>
    static void construct_impl(void* ptr, Context& ctx, Container& container,
                               std::index_sequence<Is...>) {
        detail::construction_dispatch<Type, T>::construct(
            ptr, ((void)Is,
                  constructor_argument_impl<T, Context, Container, Tag>(
                      ctx, container))...);
    }

  public:
    template <typename Type, typename Context, typename Container>
    static auto construct(Context& ctx, Container& container) {
        return construct_impl<Type>(ctx, container,
                                    std::make_index_sequence<Arity>{});
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        construct_impl<Type>(ptr, ctx, container,
                             std::make_index_sequence<Arity>{});
    }
};

template <typename T, typename Tag, size_t Arity, constructor_kind Kind>
struct constructor_detection_dispatch;

template <typename T, typename Tag, size_t Arity>
struct constructor_detection_dispatch<T, Tag, Arity,
                                      constructor_kind::concrete> {
    template <typename Type, typename Context, typename Container>
    static auto construct(Context& ctx, Container& container) {
        return constructor_methods<T, Tag, Arity>::template construct<Type>(
            ctx, container);
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        constructor_methods<T, Tag, Arity>::template construct<Type>(
            ptr, ctx, container);
    }
};

template <typename T, typename Tag, size_t Arity>
struct constructor_detection_dispatch<T, Tag, Arity,
                                      constructor_kind::generic> {
    template <typename Type, typename Context, typename Container>
    static Type construct(Context&, Container&) {
        static_assert(
            always_false_v<Type>,
            "generic constructor detected; use explicit "
            "factory<constructor<...>>");
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void*, Context&, Container&) {
        static_assert(
            always_false_v<Type>,
            "generic constructor detected; use explicit "
            "factory<constructor<...>>");
    }
};

template <typename T, typename Tag, size_t Arity>
struct constructor_detection_dispatch<T, Tag, Arity,
                                      constructor_kind::invalid> {
    template <typename Type, typename Context, typename Container>
    static Type construct(Context&, Container&) {
        static_assert(always_false_v<Type>,
                      "class T construction not detected or ambiguous");
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void*, Context&, Container&) {
        static_assert(always_false_v<Type>,
                      "class T construction not detected or ambiguous");
    }
};

template <typename T, typename Tag, template <typename...> typename IsConstructible,
          size_t N>
struct constructor_detection {
    // The detector owns policy: pick the highest matching arity once, then let
    // the runtime path instantiate only that winning constructor shape.
    // Search from high to low so the first match is the winning constructor
    // arity without materializing the full `[0, N]` probe set up front.
    static constexpr size_t detected_arity =
        constructor_arity_detector<T, Tag, IsConstructible, N>::value;
    static constexpr bool detected =
        detected_arity != invalid_constructor_detection_arity;
    static constexpr bool requires_explicit_factory = [] {
        if constexpr (!detected || detected_arity == 0) {
            return false;
        } else {
            return constructor_probe_v<T, Tag, opaque_constructor_argument,
                                       IsConstructible, detected_arity>;
        }
    }();
    static constexpr constructor_kind kind =
        !detected ? constructor_kind::invalid
                  : requires_explicit_factory ? constructor_kind::generic
                                              : constructor_kind::concrete;
    static constexpr size_t arity = detected ? detected_arity : 0;
    using dispatch = constructor_detection_dispatch<T, Tag, arity, kind>;

  public:
    template <typename Type, typename Context, typename Container>
    static auto construct(Context& ctx, Container& container) {
        return dispatch::template construct<Type>(ctx, container);
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        dispatch::template construct<Type>(ptr, ctx, container);
    }
};

} // namespace detail

template <typename T, typename DetectionType = detail::automatic>
struct constructor_detection
    : std::conditional_t<
          has_constructor_typedef_v<T>, constructor_typedef<T>,
          detail::constructor_detection<
              T,
              DetectionType,
              detail::list_initialization,
              constructor_detection_traits<normalized_type_t<T>>::max_arity>> {
};

} // namespace dingo
