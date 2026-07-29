//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Constructor detection and construction implementation.
//
// Constructor-shape probes search for the highest viable arity and distinguish
// concrete constructors from generic conversion shapes. Constructor-signature
// probes then recover the parameter types for that winning arity. Construction
// reuses the recovered type list as an explicit signature, keeping reported
// dependencies and injected dependencies identical.
//
// The generic probe and signature recovery are composed below. MSVC-specific
// detection is composed in constructor_detection_msvc.hpp from the shared
// machinery in this header.

namespace dingo {
namespace detail {

// Reports constructor kind and arity without retaining concrete argument types.
struct constructor_shape {};

// Additionally recovers the concrete argument types of the selected constructor.
struct constructor_signature {};

// Converting wrappers and alternatives can both accept an inner dependency.
// Describe the requested outer type structurally so an ambiguous regular probe
// can disable those competing inner conversions.
enum class constructor_request_kind { value, wrapper, alternative };

template <constructor_request_kind Kind, size_t Depth>
struct constructor_request {};

template <size_t... Values> struct constructor_request_max;

template <> struct constructor_request_max<> : std::integral_constant<size_t, 0> {};

template <size_t Head, size_t... Tail>
struct constructor_request_max<Head, Tail...>
    : std::integral_constant<
          size_t,
          (Head > constructor_request_max<Tail...>::value
               ? Head
               : constructor_request_max<Tail...>::value)> {};

template <typename T> struct constructor_request_depth;

template <typename List> struct alternative_request_depth;

template <typename... Alternatives>
struct alternative_request_depth<type_list<Alternatives...>>
    : std::integral_constant<
          size_t,
          1 + constructor_request_max<
                  constructor_request_depth<Alternatives>::value...>::value> {};

template <typename T,
          bool IsAlternative = is_alternative_type_v<T>,
          bool IsWrapper = type_traits<T>::enabled>
struct constructor_request_depth_impl : std::integral_constant<size_t, 0> {};

template <typename T, bool IsWrapper>
struct constructor_request_depth_impl<T, true, IsWrapper>
    : alternative_request_depth<alternative_type_alternatives_t<T>> {};

template <typename T>
struct constructor_request_depth_impl<T, false, true>
    : std::integral_constant<
          size_t,
          1 + constructor_request_depth<
                  typename type_traits<T>::value_type>::value> {};

template <typename T>
struct constructor_request_depth
    : constructor_request_depth_impl<
          std::remove_cv_t<std::remove_reference_t<T>>> {};

template <typename T>
inline constexpr size_t constructor_request_depth_v =
    constructor_request_depth<T>::value;

template <typename T>
inline constexpr constructor_request_kind constructor_request_kind_v = [] {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
  if constexpr (is_alternative_type_v<type>) {
    return constructor_request_kind::alternative;
  } else if constexpr (type_traits<type>::enabled) {
    return constructor_request_kind::wrapper;
  } else {
    return constructor_request_kind::value;
  }
}();

template <class DisabledType, typename DetectionMode>
struct constructor_argument;
template <class DisabledType, typename DetectionMode>
struct opaque_constructor_argument;

template <class DisabledType>
struct constructor_argument<DisabledType, constructor_shape> {
  // Value and rvalue probes imply materialization, so they must not
  // participate for forward declarations. Otherwise constructor deduction
  // would pull incomplete dependencies into class-trait inspection.
  template <typename T, typename = typename std::enable_if_t<
                            !std::is_same_v<DisabledType, std::decay_t<T>> &&
                            is_complete<std::decay_t<T>>::value>>
  operator T &&() const;

  // Lvalue-reference probes stay available for incomplete types. They model
  // borrowed dependencies that can be satisfied by lookup without trying to
  // construct the forward-declared type.
  template <typename T, typename = typename std::enable_if_t<
                            !std::is_same_v<DisabledType, std::decay_t<T>>>>
  operator const T &() const;

  template <typename T, typename = typename std::enable_if_t<
                            !std::is_same_v<DisabledType, std::decay_t<T>>>>
  operator T &() const;

  template <typename T, typename = typename std::enable_if_t<
                            !std::is_same_v<DisabledType, std::decay_t<T>> &&
                            is_complete<std::decay_t<T>>::value>>
  operator T();

  template <typename T, typename Selector,
            typename = typename std::enable_if_t<
                !std::is_same_v<DisabledType, std::decay_t<T>>>>
  operator detail::selected<T, Selector>() const;
};

template <class DisabledType, constructor_request_kind Kind, size_t Depth>
struct constructor_argument<DisabledType, constructor_request<Kind, Depth>> {
  template <
      typename T,
      typename = typename std::enable_if_t<
          !std::is_same_v<DisabledType, std::decay_t<T>> &&
          is_complete<std::decay_t<T>>::value &&
          constructor_request_kind_v<std::decay_t<T>> == Kind &&
          constructor_request_depth_v<std::decay_t<T>> == Depth>>
  operator T();
};

template <class DisabledType>
struct opaque_constructor_argument<DisabledType, constructor_shape> {
  opaque_constructor_argument() = default;
  opaque_constructor_argument(const opaque_constructor_argument &) = default;
  opaque_constructor_argument(opaque_constructor_argument &&) = default;
  opaque_constructor_argument &
  operator=(const opaque_constructor_argument &) = default;
  opaque_constructor_argument &
  operator=(opaque_constructor_argument &&) = default;
};

template <typename DisabledType, typename Context, typename Container,
          typename DetectionMode>
class constructor_argument_impl;

template <typename DisabledType, typename Context, typename Container>
class constructor_argument_impl<DisabledType, Context, Container,
                                constructor_shape> {
public:
  constructor_argument_impl(construction_scope scope, Context &context,
                            Container &container)
      : scope_(scope), context_(context), container_(container) {}

  template <typename T, typename = typename std::enable_if_t<
                            !std::is_same_v<DisabledType, std::decay_t<T>>>>
  operator const T &() const {
    return context_.template resolve<const T &>(
        detail::dependency_scope<const T &>(scope_), container_);
  }

  template <typename T, typename = typename std::enable_if_t<
                            !std::is_same_v<DisabledType, std::decay_t<T>>>>
  operator T &() const {
    return context_.template resolve<T &>(detail::dependency_scope<T &>(scope_),
                                          container_);
  }

  template <typename T, typename = typename std::enable_if_t<
                            !std::is_same_v<DisabledType, std::decay_t<T>> &&
                            is_complete<std::decay_t<T>>::value>>
  operator T() {
    // A prvalue `T` still satisfies `T&&` constructor parameters, so
    // constructor injection can stay on the regular value path here.
    return context_.template resolve<T>(detail::dependency_scope<T>(scope_),
                                        container_);
  }

  template <typename T, typename Annotation,
            typename = std::enable_if_t<
                !std::is_same_v<DisabledType, std::decay_t<T>>>>
  operator annotated<T, Annotation>() {
    using request_type = annotated<T, Annotation>;
    return context_.template resolve<request_type>(
        detail::dependency_scope<request_type>(scope_), container_);
  }

  template <typename T, typename Selector,
            typename = std::enable_if_t<
                !std::is_same_v<DisabledType, std::decay_t<T>>>>
  operator detail::selected<T, Selector>() {
    using request_type = detail::selected<T, Selector>;
    return context_.template resolve<request_type>(
        detail::dependency_scope<request_type>(scope_), container_);
  }

private:
  construction_scope scope_;
  Context &context_;
  Container &container_;
};

template <typename DisabledType, typename Context, typename Container,
          constructor_request_kind Kind, size_t Depth>
class constructor_argument_impl<DisabledType, Context, Container,
                                constructor_request<Kind, Depth>> {
public:
  constructor_argument_impl(construction_scope scope, Context &context,
                            Container &container)
      : scope_(scope), context_(context), container_(container) {}

  template <
      typename T,
      typename = typename std::enable_if_t<
          !std::is_same_v<DisabledType, std::decay_t<T>> &&
          is_complete<std::decay_t<T>>::value &&
          constructor_request_kind_v<std::decay_t<T>> == Kind &&
          constructor_request_depth_v<std::decay_t<T>> == Depth>>
  operator T() {
    return context_.template resolve<T>(detail::dependency_scope<T>(scope_),
                                        container_);
  }

private:
  construction_scope scope_;
  Context &context_;
  Container &container_;
};

template <typename T, typename... Args>
using list_initialization_expr = decltype(T{std::declval<Args>()...});

template <typename T, typename... Args>
using direct_initialization_expr = decltype(T(std::declval<Args>()...));

template <typename T, size_t N, typename, typename... Args>
struct array_list_initialization : std::false_type {};

template <typename T, size_t N, typename... Args>
struct array_list_initialization<
    T, N,
    std::void_t<decltype(std::array<T, N>{
        {std::declval<Args>()...}})>,
    Args...> : std::true_type {};

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
struct direct_initialization
    : initialization_impl<direct_initialization_expr, T, void, Args...> {};
#endif

// std::array stores its elements in a nested native array. MSVC does not apply
// brace elision while probing that aggregate, which otherwise makes every
// std::array appear to have arity one. Model the required braces explicitly on
// every compiler so detection and construction agree on the element count.
template <typename T, size_t N, typename... Args>
struct list_initialization<std::array<T, N>, Args...>
    : array_list_initialization<T, N, void, Args...> {};

template <typename T, typename... Args>
inline constexpr bool is_list_initializable_v =
    list_initialization<T, Args...>::value;

template <typename T, typename... Args>
inline constexpr bool is_direct_initializable_v =
    direct_initialization<T, Args...>::value;

template <typename...> inline constexpr bool always_false_v = false;

inline constexpr size_t invalid_arity = static_cast<size_t>(-1);

template <typename T, size_t> using repeated_type = T;

struct ambiguous_constructor_request {};

template <typename T> struct is_constructor_request : std::false_type {};

template <constructor_request_kind Kind, size_t Depth>
struct is_constructor_request<constructor_request<Kind, Depth>>
    : std::true_type {};

template <typename T>
inline constexpr bool is_constructor_request_v = is_constructor_request<T>::value;

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, template <typename...> typename IsConstructible,
          size_t Depth>
struct constructor_request_search;

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, template <typename...> typename IsConstructible,
          size_t Depth, bool WrapperMatch, bool AlternativeMatch>
struct constructor_request_match;

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, template <typename...> typename IsConstructible,
          size_t Depth>
struct constructor_request_match<ConstructorProbe, T, IsConstructible, Depth,
                                 false, false>
    : constructor_request_search<ConstructorProbe, T, IsConstructible,
                                 Depth - 1> {};

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, template <typename...> typename IsConstructible,
          size_t Depth>
struct constructor_request_match<ConstructorProbe, T, IsConstructible, Depth,
                                 true, false> {
  using type = constructor_request<constructor_request_kind::wrapper, Depth>;
};

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, template <typename...> typename IsConstructible,
          size_t Depth>
struct constructor_request_match<ConstructorProbe, T, IsConstructible, Depth,
                                 false, true> {
  using type = constructor_request<constructor_request_kind::alternative, Depth>;
};

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, template <typename...> typename IsConstructible,
          size_t Depth>
struct constructor_request_match<ConstructorProbe, T, IsConstructible, Depth,
                                 true, true> {
  using type = ambiguous_constructor_request;
};

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, template <typename...> typename IsConstructible,
          size_t Depth>
struct constructor_request_search
    : constructor_request_match<
          ConstructorProbe, T, IsConstructible, Depth,
          ConstructorProbe<
              T,
              constructor_request<constructor_request_kind::wrapper, Depth>,
              constructor_argument, IsConstructible, 1>::value,
          ConstructorProbe<
              T,
              constructor_request<constructor_request_kind::alternative, Depth>,
              constructor_argument, IsConstructible, 1>::value> {};

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, template <typename...> typename IsConstructible>
struct constructor_request_search<ConstructorProbe, T, IsConstructible, 0> {
  using type = void;
};

template <bool Search,
          template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, template <typename...> typename IsConstructible,
          size_t MaxDepth>
struct constructor_request_detection {
  using type = void;
};

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, template <typename...> typename IsConstructible,
          size_t MaxDepth>
struct constructor_request_detection<true, ConstructorProbe, T,
                                     IsConstructible, MaxDepth>
    : constructor_request_search<ConstructorProbe, T, IsConstructible,
                                 MaxDepth> {};

template <typename T, typename DetectionMode, size_t Arity>
struct constructor_methods {
private:
  template <typename Type, typename Context, typename Container, size_t... Is>
  static auto construct_impl(construction_scope scope, Context &ctx,
                             Container &container, std::index_sequence<Is...>) {
    (void)scope;
    // `Is...` only drives the pack expansion; the runtime construction
    // path still receives `Arity` copies of the same constructor argument
    // adapter without first materializing a type_list of placeholders.
    return detail::construction_dispatch<Type, T>::construct(
        ((void)Is, constructor_argument_impl<T, Context, Container, DetectionMode>(
                       scope, ctx, container))...);
  }

  template <typename Type, typename Context, typename Container, size_t... Is>
  static void construct_impl(void *ptr, construction_scope scope, Context &ctx,
                             Container &container, std::index_sequence<Is...>) {
    (void)scope;
    detail::construction_dispatch<Type, T>::construct(
        ptr, ((void)Is, constructor_argument_impl<T, Context, Container, DetectionMode>(
                            scope, ctx, container))...);
  }

public:
  template <typename Type, typename Context, typename Container>
  static auto construct(construction_scope scope, Context &ctx,
                        Container &container) {
    return construct_impl<Type>(scope, ctx, container,
                                std::make_index_sequence<Arity>{});
  }

  template <typename Type, typename Context, typename Container>
  static void construct(void *ptr, construction_scope scope, Context &ctx,
                        Container &container) {
    construct_impl<Type>(ptr, scope, ctx, container,
                         std::make_index_sequence<Arity>{});
  }
};

template <typename T, size_t N, typename DetectionMode>
struct constructor_methods<std::array<T, N>, DetectionMode, N> {
private:
  template <typename Type, typename Context, typename Container, size_t... Is>
  static auto construct_impl(construction_scope scope, Context &ctx,
                             Container &container, std::index_sequence<Is...>) {
    // std::array publishes its complete construction signature. Resolve its
    // elements as T before aggregate initialization instead of asking the
    // aggregate to select conversions from constructor-shape arguments.
    return ::dingo::constructor<
        std::array<T, N>(repeated_type<T, Is>...)>::template construct<Type>(
        scope, ctx, container);
  }

  template <typename Type, typename Context, typename Container, size_t... Is>
  static void construct_impl(void *ptr, construction_scope scope, Context &ctx,
                             Container &container, std::index_sequence<Is...>) {
    ::dingo::constructor<
        std::array<T, N>(repeated_type<T, Is>...)>::template construct<Type>(
        ptr, scope, ctx, container);
  }

public:
  template <typename Type, typename Context, typename Container>
  static auto construct(construction_scope scope, Context &ctx,
                        Container &container) {
    return construct_impl<Type>(scope, ctx, container,
                                std::make_index_sequence<N>{});
  }

  template <typename Type, typename Context, typename Container>
  static void construct(void *ptr, construction_scope scope, Context &ctx,
                        Container &container) {
    construct_impl<Type>(ptr, scope, ctx, container,
                         std::make_index_sequence<N>{});
  }
};

template <typename T, typename DetectionMode, size_t Arity, constructor_kind Kind>
struct constructor_detection_dispatch;

template <typename T, typename DetectionMode, size_t Arity>
struct constructor_detection_dispatch<T, DetectionMode, Arity,
                                      constructor_kind::concrete> {
  template <typename Type, typename Context, typename Container>
  static auto construct(construction_scope scope, Context &ctx,
                        Container &container) {
    return constructor_methods<T, DetectionMode, Arity>::template construct<Type>(
        scope, ctx, container);
  }

  template <typename Type, typename Context, typename Container>
  static void construct(void *ptr, construction_scope scope, Context &ctx,
                        Container &container) {
    constructor_methods<T, DetectionMode, Arity>::template construct<Type>(
        ptr, scope, ctx, container);
  }
};

template <typename T, typename DetectionMode, size_t Arity>
struct constructor_detection_dispatch<T, DetectionMode, Arity,
                                      constructor_kind::generic> {
  template <typename Type, typename Context, typename Container>
  static Type construct(construction_scope, Context &, Container &) {
    static_assert(always_false_v<Type>,
                  "generic constructor detected; use explicit "
                  "factory<constructor<...>>");
  }

  template <typename Type, typename Context, typename Container>
  static void construct(void *, construction_scope, Context &, Container &) {
    static_assert(always_false_v<Type>,
                  "generic constructor detected; use explicit "
                  "factory<constructor<...>>");
  }
};

template <typename T, typename DetectionMode, size_t Arity>
struct constructor_detection_dispatch<T, DetectionMode, Arity,
                                      constructor_kind::invalid> {
  template <typename Type, typename Context, typename Container>
  static Type construct(construction_scope, Context &, Container &) {
    static_assert(always_false_v<Type>,
                  "class T construction not detected or ambiguous");
  }

  template <typename Type, typename Context, typename Container>
  static void construct(void *, construction_scope, Context &, Container &) {
    static_assert(always_false_v<Type>,
                  "class T construction not detected or ambiguous");
  }
};

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename ArityDetection,
          typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t MaxArity,
          size_t RequestDepth>
struct constructor_detection_impl {
  // The detector owns policy: pick the highest matching arity once, then let
  // the runtime path instantiate only that winning constructor shape.
  // Search from high to low so the first match is the winning constructor
  // arity without materializing the full `[0, N]` probe set up front.
  static constexpr size_t detected_arity = ArityDetection::value;
  // One-argument constructors may select a converting inner type during the
  // probe even when the runtime adapter cannot reproduce that selection.
  // Prefer a structural outer request when one exists so both stages agree.
  using request = typename constructor_request_detection<
      MaxArity >= 1 &&
          (detected_arity == invalid_arity || detected_arity == 1),
      ConstructorProbe, T, IsConstructible, RequestDepth>::type;
  static constexpr bool has_request = is_constructor_request_v<request>;
  static constexpr bool detected =
      detected_arity != invalid_arity || has_request;
  static constexpr size_t arity =
      detected_arity != invalid_arity ? detected_arity : detected ? 1 : 0;
  static constexpr bool requires_explicit_factory = [] {
    if constexpr (!detected || arity == 0) {
      return false;
    } else {
      return ConstructorProbe<T, DetectionMode, opaque_constructor_argument,
                              IsConstructible, arity>::value;
    }
  }();
  static constexpr constructor_kind kind = !detected ? constructor_kind::invalid
                                           : requires_explicit_factory
                                               ? constructor_kind::generic
                                               : constructor_kind::concrete;
  using arguments = std::conditional_t<kind == constructor_kind::concrete &&
                                           arity == 0,
                                       type_list<>, void>;
  // Construction uses the same request that made the probe succeed.
  using construction_mode =
      std::conditional_t<has_request, request, DetectionMode>;
  using dispatch =
      constructor_detection_dispatch<T, construction_mode, arity, kind>;

public:
  template <typename Type, typename Context, typename Container>
  static auto construct(construction_scope scope, Context &ctx,
                        Container &container) {
    return dispatch::template construct<Type>(scope, ctx, container);
  }

  template <typename Type, typename Context, typename Container>
  static void construct(void *ptr, construction_scope scope, Context &ctx,
                        Container &container) {
    dispatch::template construct<Type>(ptr, scope, ctx, container);
  }
};

template <typename T, typename Arguments> struct constructor_from_arguments;

template <typename T, typename... Args>
struct constructor_from_arguments<T, type_list<Args...>> {
  // Reuse the recovered list as the construction signature. This keeps
  // reflection and construction on the same types instead of detecting twice.
  template <typename Type, typename Context, typename Container>
  static auto construct(construction_scope scope, Context &ctx,
                        Container &container) {
    return ::dingo::constructor<T(Args...)>::template construct<Type>(
        scope, ctx, container);
  }

  template <typename Type, typename Context, typename Container>
  static void construct(void *ptr, construction_scope scope, Context &ctx,
                        Container &container) {
    ::dingo::constructor<T(Args...)>::template construct<Type>(ptr, scope, ctx,
                                                               container);
  }
};

template <
    template <typename, typename, template <typename...> typename, size_t>
    typename Detection,
    template <typename, typename, template <typename...> typename, size_t,
              constructor_kind>
    typename Arguments,
    typename T, template <typename...> typename IsConstructible, size_t N>
struct constructor_detection_signature_impl
    : Detection<T, constructor_shape, IsConstructible, N> {
private:
  // Shape detection remains the source of truth for arity and constructor kind.
  // Signature detection only recovers arguments for its winning concrete arity.
  using base_type =
      Detection<T, constructor_shape, IsConstructible, N>;

public:
  using arguments = typename Arguments<
      T, constructor_signature, IsConstructible, base_type::arity,
      base_type::kind>::type;
  static_assert(base_type::kind != constructor_kind::concrete ||
                    !std::is_void_v<arguments>,
                "constructor-signature detection must cover every constructor "
                "selected by constructor-shape detection");

  template <typename Type, typename Context, typename Container>
  static auto construct(construction_scope scope, Context &ctx,
                        Container &container) {
    if constexpr (base_type::kind == constructor_kind::concrete) {
      return constructor_from_arguments<T, arguments>::template construct<Type>(
          scope, ctx, container);
    } else {
      return base_type::template construct<Type>(scope, ctx, container);
    }
  }

  template <typename Type, typename Context, typename Container>
  static void construct(void *ptr, construction_scope scope, Context &ctx,
                        Container &container) {
    if constexpr (base_type::kind == constructor_kind::concrete) {
      constructor_from_arguments<T, arguments>::template construct<Type>(
          ptr, scope, ctx, container);
    } else {
      base_type::template construct<Type>(ptr, scope, ctx, container);
    }
  }
};

} // namespace detail
} // namespace dingo

namespace dingo {
namespace detail {

#if !defined(_MSC_VER)
template <typename T, typename DetectionMode,
          template <class, class> class ConstructorArg,
          template <typename...> typename IsConstructible, size_t... Is>
constexpr bool constructor_probe_value(std::index_sequence<Is...>) {
  // Non-MSVC compilers handle the lighter repeated-type placeholder probe
  // well, which avoids an extra wrapper class per arity check.
  return IsConstructible<T,
                         repeated_type<ConstructorArg<T, DetectionMode>, Is>...>::value;
}

template <typename T, typename DetectionMode,
          template <class, class> class ConstructorArg,
          template <typename...> typename IsConstructible, size_t Arity>
struct constructor_probe
    : std::bool_constant<constructor_probe_value<
          T, DetectionMode, ConstructorArg, IsConstructible>(
          std::make_index_sequence<Arity>{})> {};

#include <dingo/factory/detail/constructor_arity.hpp>
#include <dingo/factory/detail/constructor_signature.hpp>

// Keep shape/signature selection specialization-based on this path. Expressing
// it as a conditional alias makes Clang instantiate more signature machinery
// and loses part of the direct-arity-probe compile-time improvement.
// Searches constructor arity in the inclusive range [0, N].
template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible,
          size_t N = DINGO_CONSTRUCTOR_DETECTION_ARGS>
struct constructor_detection;

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t N>
struct constructor_detection
    : constructor_detection_impl<
          constructor_probe,
          constructor_arity<T, DetectionMode, IsConstructible, N>,
          T, DetectionMode, IsConstructible, N,
          DINGO_CONSTRUCTOR_REQUEST_DEPTH> {};

template <typename T, template <typename...> typename IsConstructible, size_t N>
struct constructor_detection<T, constructor_signature, IsConstructible, N>
    : constructor_detection_signature_impl<constructor_detection,
                                           constructor_signature_recovery, T,
                                           IsConstructible, N> {};
#endif

} // namespace detail
} // namespace dingo
