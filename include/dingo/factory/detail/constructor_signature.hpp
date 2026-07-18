//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Arity-first constructor-signature detection.
//
// Constructor-shape detection proves that `T` is constructible from N
// placeholder arguments but does not expose the concrete parameter types
// selected by overload resolution. Constructor-signature detection first finds
// the winning arity, then probes that constructor with indexed placeholders.
//
// Each indexed placeholder covers the same dependency shapes as the conversion
// dependency placeholder. Its value conversion also covers rvalue-reference
// parameters and selected wrappers: a `T` prvalue binds to `T&&`, while a
// selected wrapper is itself the value conversion's target type. When overload
// resolution selects, for example, `operator U&()` for argument index I, the
// operator declaration instantiates a definition that associates `U&` with
// that indexed placeholder. Querying the placeholder's injected friend then
// recovers the resolved type for each index and assembles `type_list<Args...>`.
//
// The recovered types are passed to `constructor<T(Args...)>`, so dependency
// reporting and construction use the same explicit constructor signature.
//
// constructor_signature.hpp is included from a constructor detection backend
// inside dingo::detail, after the shared detection primitives are defined.

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-template-friend"
#endif

template <typename Dependency> struct constructor_signature_argument_result {
  using type = Dependency;
};

#if !defined(_MSC_VER)
template <typename Dependency, typename Argument>
struct constructor_signature_argument_definition {
  // `value` is referenced by a conversion's noexcept expression. Instantiating
  // this class therefore records `Dependency` under the placeholder `Argument`.
  friend auto constructor_signature_argument_type(Argument) {
    return constructor_signature_argument_result<Dependency>{};
  }

  static constexpr bool value = true;
};

template <class DisabledType, typename Probe, size_t I>
struct constructor_signature_argument {
  // This declaration makes the later hidden-friend definition visible to ADL.
  // `Probe` and `I` make the placeholder, and therefore its friend, unique.
  friend auto constructor_signature_argument_type(constructor_signature_argument);

  template <typename T, typename = typename std::enable_if_t<
                            !std::is_same_v<DisabledType, std::decay_t<T>> &&
                            is_complete<std::decay_t<T>>::value>>
  operator T &&() const noexcept(constructor_signature_argument_definition<
                                 T &&, constructor_signature_argument>::value);

  template <typename T, typename = typename std::enable_if_t<
                            !std::is_same_v<DisabledType, std::decay_t<T>>>>
  operator const T &() const
      noexcept(constructor_signature_argument_definition<
               const T &, constructor_signature_argument>::value);

  template <typename T, typename = typename std::enable_if_t<
                            !std::is_same_v<DisabledType, std::decay_t<T>>>>
  operator T &() const noexcept(constructor_signature_argument_definition<
                                T &, constructor_signature_argument>::value);

  template <typename T, typename = typename std::enable_if_t<
                            !std::is_same_v<DisabledType, std::decay_t<T>> &&
                            is_complete<std::decay_t<T>>::value>>
  operator T() noexcept(constructor_signature_argument_definition<
                        T, constructor_signature_argument>::value);

  template <typename T, typename Selector,
            typename = typename std::enable_if_t<
                !std::is_same_v<DisabledType, std::decay_t<T>>>>
  operator detail::selected<T, Selector>() const noexcept(
      constructor_signature_argument_definition<detail::selected<T, Selector>,
                                                constructor_signature_argument>::value);
};
#endif

// A value conversion can satisfy a `T&&` parameter. Some compilers expose that
// selected conversion as `T&&`, but construction must request the underlying
// value dependency. A partial specialization keeps this hot normalization to
// one class instantiation instead of composing several standard type traits.
template <typename T> struct constructor_signature_dependency {
  using type = T;
};

template <typename T> struct constructor_signature_dependency<T &&> {
  using type = T;
};

template <typename T>
using constructor_signature_dependency_t =
    typename constructor_signature_dependency<T>::type;

template <typename... Args> struct constructor_signature_argument_list {
  using type = std::conditional_t<
      (std::is_void_v<Args> || ...), void,
      type_list<constructor_signature_dependency_t<Args>...>>;
};

#if !defined(_MSC_VER)
template <typename T, typename Probe, typename Sequence, typename = void>
struct constructor_signature_arguments {
  using type = void;
};

template <typename T, typename Probe, size_t... Is>
struct constructor_signature_arguments<
    T, Probe, std::index_sequence<Is...>,
    std::void_t<decltype(constructor_signature_argument_type(
        constructor_signature_argument<T, Probe, Is>{}))...>> {
  using type = type_list<constructor_signature_dependency_t<
      typename decltype(constructor_signature_argument_type(
          constructor_signature_argument<T, Probe, Is>{}))::type>...>;
};

template <typename T, typename Probe,
          template <typename...> typename IsConstructible, typename Sequence>
struct constructor_signature_probe;

template <typename T, typename Probe,
          template <typename...> typename IsConstructible, size_t... Is>
struct constructor_signature_probe<T, Probe, IsConstructible,
                                   std::index_sequence<Is...>>
    : IsConstructible<T, constructor_signature_argument<T, Probe, Is>...> {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity>
struct constructor_signature_probe_id {};

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#endif

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          constructor_kind Kind>
struct constructor_signature_recovery {
  using type = void;
};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible>
struct constructor_signature_recovery<T, DetectionMode, IsConstructible, 0,
                                      constructor_kind::concrete> {
  using type = type_list<>;
};

#if !defined(_MSC_VER)
template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity>
struct constructor_signature_recovery<T, DetectionMode, IsConstructible, Arity,
                                      constructor_kind::concrete> {
private:
  // These compilers instantiate only the winning conversion's noexcept
  // expression, so all argument categories can share one friend key.
  using probe = constructor_signature_probe_id<T, DetectionMode,
                                               IsConstructible, Arity>;
  static constexpr bool resolved =
      constructor_signature_probe<T, probe, IsConstructible,
                                  std::make_index_sequence<Arity>>::value;

public:
  using type =
      std::conditional_t<resolved,
                         typename constructor_signature_arguments<
                             T, probe, std::make_index_sequence<Arity>>::type,
                         void>;
};
#endif
