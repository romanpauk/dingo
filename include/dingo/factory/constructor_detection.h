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
#include <dingo/type/normalized_type.h>
#include <dingo/factory/constructor_typedef.h>

#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {
struct automatic {};

// Constructor detection is split into two phases:
// 1. Probe which constructor arity is viable by substituting placeholder
//    arguments into a chosen initialization form.
// 2. Re-run the selected arity through the real resolving context at runtime.
//
// The code below looks more indirect than a straightforward `is_constructible`
// check because Dingo needs its detection step to model the same conversions
// that the runtime resolver can perform.

template <class DisabledType, typename Tag> struct constructor_argument;

template <class DisabledType>
struct constructor_argument<DisabledType, automatic> {
    // Detection works by asking "could T be formed from N placeholder
    // arguments?". The placeholder exposes the same conversion surface as the
    // runtime resolver so overload resolution during probing matches the final
    // construction path closely enough to select the same constructor.
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

template <typename T, typename... Args>
using list_initialization_expr = decltype(T{std::declval<Args>()...});

template <typename T, typename... Args>
using direct_initialization_expr = decltype(T(std::declval<Args>()...));

// During detection we are interested in "dependency-taking" constructors, not
// in trivially matching the type against itself. A single placeholder argument
// of type `T` would otherwise make copy-construction look like a meaningful
// injection constructor and pollute arity selection.
template <typename T, typename... Args>
inline constexpr bool is_non_copy_constructor_argument_v =
    sizeof...(Args) != 1 || (!std::is_same_v<T, std::decay_t<Args>> && ...);

// Normalize "is this initialization form well-formed?" into one reusable
// trait so list/direct initialization share the same substitution path.
template <template <typename, typename...> typename InitExpr, typename T,
          typename = void, typename... Args>
struct initialization_impl : std::false_type {};

template <template <typename, typename...> typename InitExpr, typename T,
          typename... Args>
struct initialization_impl<InitExpr, T, std::void_t<InitExpr<T, Args...>>,
                           Args...>
    // Filter out the accidental "copy-constructible from itself" case so the
    // detector does not report `T(T&)` as a meaningful injection constructor.
    : std::bool_constant<is_non_copy_constructor_argument_v<T, Args...>> {};

template <typename T, typename... Args>
inline constexpr bool is_list_initializable_v =
    initialization_impl<list_initialization_expr, T, void, Args...>::value;

template <typename T, typename... Args>
inline constexpr bool is_direct_initializable_v =
    initialization_impl<direct_initialization_expr, T, void, Args...>::value;

inline constexpr size_t invalid_constructor_detection_arity =
    static_cast<size_t>(-1);

template <typename T, size_t>
using repeated_type = T;

template <template <typename, typename...> typename InitExpr, typename T, typename Tag,
          size_t... Is>
constexpr bool is_constructible(std::index_sequence<Is...>) {
    // The integer sequence is only a cheap way to repeat the same placeholder
    // type `Arity` times without materializing an intermediate type_list.
    return initialization_impl<InitExpr, T, void,
                               repeated_type<constructor_argument<T, Tag>, Is>...>::value;
}

template <template <typename, typename...> typename InitExpr, typename T, typename Tag,
          size_t Arity>
inline constexpr bool is_constructible_v =
    is_constructible<InitExpr, T, Tag>(std::make_index_sequence<Arity>{});

template <template <typename, typename...> typename InitExpr, typename T,
          typename Tag, size_t Arity>
struct constructor_arity_detector {
    // Probe arities one-by-one from high to low. This keeps the compile-time
    // search equivalent to the previous "highest arity wins" rule, but avoids
    // instantiating the full [0, N] probe set in one large pack expansion,
    // which MSVC can overflow on for larger aggregate graphs.
    static constexpr size_t value =
        is_constructible_v<InitExpr, T, Tag, Arity>
            ? Arity
            : constructor_arity_detector<InitExpr, T, Tag, Arity - 1>::value;
};

template <template <typename, typename...> typename InitExpr, typename T,
          typename Tag>
struct constructor_arity_detector<InitExpr, T, Tag, 0> {
    static constexpr size_t value =
        is_constructible_v<InitExpr, T, Tag, 0>
            ? 0
            : invalid_constructor_detection_arity;
};

template <template <typename, typename...> typename InitExpr, typename T,
          typename Tag, size_t N>
struct constructor_detection_tag {
    using tag_type = Tag;
    // Run the arity scan once for a single detection tag and expose the small
    // result shape used by the rest of the detector. Keeping this as a tiny
    // `{tag_type, arity, valid}` carrier makes it easy to experiment with
    // alternate tag behaviors without changing the outer runtime path.
    static constexpr size_t arity =
        constructor_arity_detector<InitExpr, T, Tag, N>::value;
    static constexpr bool valid = arity != invalid_constructor_detection_arity;
};

// Searches constructor arity in the inclusive range [0, N].
template <typename T, typename Tag,
          template <typename, typename...> typename InitExpr,
          bool Assert = true, size_t N = DINGO_CONSTRUCTOR_DETECTION_ARGS>
struct constructor_detection;

template <typename T, typename Tag, size_t Arity> struct constructor_methods {
    static constexpr size_t arity = Arity;
    static constexpr bool valid = true;

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

template <typename T, typename Tag,
          template <typename, typename...> typename InitExpr,
          bool Assert, size_t N>
struct constructor_detection {
  private:
    // Detection computes the static shape once. Runtime construction below only
    // instantiates the winning arity instead of replaying all probes.
    using detection = constructor_detection_tag<InitExpr, T, Tag, N>;

  public:
    static constexpr size_t arity = detection::valid ? detection::arity : 0;
    static constexpr bool valid = detection::valid;

    static_assert(!Assert || valid,
                  "class T construction not detected or ambiguous");

    template <typename Type, typename Context, typename Container>
    static auto construct(Context& ctx, Container& container) {
        static_assert(valid, "class T construction not detected or ambiguous");
        // The final runtime path expands only the selected arity and tag.
        return constructor_methods<T, Tag, detection::arity>::template construct<
            Type>(ctx, container);
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        static_assert(valid, "class T construction not detected or ambiguous");
        constructor_methods<T, Tag, detection::arity>::template construct<Type>(
            ptr, ctx, container);
    }
};

} // namespace detail

template <typename T, typename DetectionType = detail::automatic> struct constructor_detection
    : std::conditional_t<
        has_constructor_typedef_v<T>,
        constructor_typedef<T>,
        // Automatic detection prefers list initialization so aggregates and
        // regular constructors share the same default path. Explicit
        // `constructor<T(...)>` registrations still bypass this detector.
        detail::constructor_detection<T, DetectionType, detail::list_initialization_expr>
    >
{};

} // namespace dingo
