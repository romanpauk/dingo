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

// Filters out T(T&).
// TODO: It should be possible to write all into single class
template <typename T, typename Arg>
struct direct_initialization<T, Arg>
    : std::conjunction<direct_initialization_impl<T, void, Arg>,
                       std::negation<std::is_same<std::decay_t<Arg>, T>>> {};

template <typename T, typename... Args>
inline constexpr bool is_direct_initializable_v = direct_initialization<T, Args...>{};

inline constexpr size_t invalid_constructor_detection_arity =
    static_cast<size_t>(-1);

template <typename T, typename Tag,
          template <typename...> typename IsConstructible, typename Sequence>
struct is_constructible;

template <typename T, typename Tag,
          template <typename...> typename IsConstructible, size_t... Is>
struct is_constructible<T, Tag, IsConstructible, std::index_sequence<Is...>>
    : IsConstructible<
          T,
          std::conditional_t<true, constructor_argument<T, Tag>,
                             std::integral_constant<size_t, Is>>...> {};

template <typename T, typename Tag,
          template <typename...> typename IsConstructible, size_t Arity>
inline constexpr bool is_constructible_v =
    is_constructible<T, Tag, IsConstructible, std::make_index_sequence<Arity>>::value;

template <typename T, typename Tag,
          template <typename...> typename IsConstructible, typename Sequence>
struct detected_constructor_arity;

template <typename T, typename Tag,
          template <typename...> typename IsConstructible, size_t... Arities>
struct detected_constructor_arity<T, Tag, IsConstructible,
                                  std::index_sequence<Arities...>> {
        // Constructor detection only needs to know which arity matched. Searching
        // by integer arity keeps the state small and avoids materializing a fresh
        // type_list for every suffix that used to be tested.
    static constexpr size_t arity = [] {
        constexpr bool matches[] = {
            is_constructible_v<T, Tag, IsConstructible, Arities>...};

        for (size_t i = sizeof...(Arities); i > 0; --i) {
            if (matches[i - 1]) {
                return i - 1;
            }
        }

        return invalid_constructor_detection_arity;
    }();

    static constexpr bool valid = arity != invalid_constructor_detection_arity;
};

template <typename T, typename Tag,
          template <typename...> typename IsConstructible, size_t N>
struct constructor_detection_tag {
  private:
    // Run the arity scan once for a single detection tag and expose the small
    // result shape used by the rest of the detector.
    using detection =
        detected_constructor_arity<T, Tag, IsConstructible,
                                   std::make_index_sequence<N + 1>>;

  public:
    using tag_type = Tag;
    static constexpr size_t arity = detection::arity;
    static constexpr bool valid = detection::valid;
};

template <typename Borrow, typename Value>
struct constructor_detection_select
    : std::conditional_t<
          !Borrow::valid, Value,
          std::conditional_t<!Value::valid, Borrow,
                             std::conditional_t<(Borrow::arity >= Value::arity),
                                                Borrow, Value>>> {};

template <typename T, typename Tag,
          template <typename...> typename IsConstructible, size_t N>
struct constructor_detection_result
    : constructor_detection_tag<T, Tag, IsConstructible, N> {};

template <typename T, template <typename...> typename IsConstructible, size_t N>
// Borrowed-reference detection has to compete with the exact by-value wrapper
// path. Both produce the same `{ tag_type, arity, valid }` shape, so the
// selector can keep the better match without changing the outer detector.
struct constructor_detection_result<T, reference, IsConstructible, N>
    : constructor_detection_select<
          constructor_detection_tag<T, reference, IsConstructible, N>,
          constructor_detection_tag<T, reference_exact, IsConstructible, N>> {};

// Searches constructor arity in the inclusive range [0, N].
template <typename T, typename Tag, template <typename...> typename IsConstructible,
          bool Assert = true, size_t N = DINGO_CONSTRUCTOR_DETECTION_ARGS>
struct constructor_detection;

template <typename T, typename Tag, size_t Arity> struct constructor_methods {
    using tag_type = Tag;
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

template <typename T, typename Tag, template <typename...> typename IsConstructible,
          bool Assert, size_t N>
struct constructor_detection {
  private:
    // The selected result owns the tag-specific policy. Most tags resolve to a
    // single arity scan, while `reference` selects between borrow and exact
    // value detection without duplicating the outer implementation body.
    // At this point detection is reduced to `{ tag_type, arity, valid }`.
    using detection = constructor_detection_result<T, Tag, IsConstructible, N>;

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
        detail::constructor_detection<T, DetectionType, detail::list_initialization>
    >
{};

} // namespace dingo
