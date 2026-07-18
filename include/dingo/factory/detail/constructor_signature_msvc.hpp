//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// MSVC-specific constructor-signature recovery.
//
// MSVC eagerly instantiates noexcept expressions for competing conversion
// candidates, so the single hidden-friend probe used by other compilers can
// attempt to record multiple types for one argument. This implementation probes
// each argument position and conversion category separately. Non-target
// positions retain ordinary conversion placeholders, preserving constructor
// overload resolution while only the target position records a type.
//
// A unique friend key isolates every probe. Marker friends make repeated writes
// idempotent, and the category results are combined into the same argument list
// consumed by the compiler-independent injection path.
//
// constructor_signature_msvc.hpp is included from
// constructor_detection_msvc.hpp inside dingo::detail, after the shared
// constructor-signature primitives are defined.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-template-friend"
#endif

struct lvalue_probe {};
struct rvalue_probe {};
struct value_probe {};
struct selected_probe {};

// Keep the friend key separate from the category placeholder. MSVC can
// canonicalize an injected placeholder name across specializations, whereas
// these explicit template arguments preserve a distinct friend signature for
// every target probe.
template <typename T, typename Probe, size_t I> struct category_key {
  friend auto category_type(category_key);
  friend constexpr int category_defined(category_key);
};

template <typename Dependency, typename Key, bool Defined>
struct category_definition_impl;

// MSVC can eagerly instantiate the same category through more than one
// conversion candidate. The marker friend turns the write into an idempotent
// operation: only the first candidate defines the type friend for this key.
template <typename Dependency, typename Key>
struct category_definition_impl<Dependency, Key, false> {
  friend auto category_type(Key) {
    return constructor_signature_argument_result<Dependency>{};
  }

  friend constexpr int category_defined(Key) { return 0; }

  static constexpr bool value = true;
};

template <typename Dependency, typename Key>
struct category_definition_impl<Dependency, Key, true> {
  static constexpr bool value = true;
};

template <typename Dependency, typename T, typename Probe, size_t I>
struct category_definition {
private:
  using key = category_key<T, Probe, I>;

  template <typename> static auto is_defined(...) -> std::false_type;

  template <typename, int = category_defined(key{})>
  static auto is_defined(int) -> std::true_type;

  using definition =
      category_definition_impl<Dependency, key,
                               decltype(is_defined<Dependency>(0))::value>;

public:
  static constexpr bool value = definition::value;
};

template <class DisabledType, typename Probe, size_t I, typename Category>
struct argument;

template <class DisabledType, typename Probe, size_t I>
struct argument<DisabledType, Probe, I, lvalue_probe> {
  template <typename T, typename = typename std::enable_if_t<
                            !std::is_same_v<DisabledType, std::decay_t<T>>>>
  operator T &() const
      noexcept(category_definition<T &, DisabledType, Probe, I>::value);
};

template <class DisabledType, typename Probe, size_t I>
struct argument<DisabledType, Probe, I, rvalue_probe> {
  template <typename T, typename = typename std::enable_if_t<
                            !std::is_same_v<DisabledType, std::decay_t<T>> &&
                            is_complete<std::decay_t<T>>::value>>
  operator T &&() const
      noexcept(category_definition<T &&, DisabledType, Probe, I>::value);
};

template <class DisabledType, typename Probe, size_t I>
struct argument<DisabledType, Probe, I, value_probe> {
  template <typename T, typename = typename std::enable_if_t<
                            !std::is_same_v<DisabledType, std::decay_t<T>> &&
                            is_complete<std::decay_t<T>>::value>>
  operator T() noexcept(category_definition<T, DisabledType, Probe, I>::value);
};

template <class DisabledType, typename Probe, size_t I>
struct argument<DisabledType, Probe, I, selected_probe> {
  template <typename T, typename Selector,
            typename = typename std::enable_if_t<
                !std::is_same_v<DisabledType, std::decay_t<T>>>>
  operator detail::selected<T, Selector>() const
      noexcept(category_definition<detail::selected<T, Selector>, DisabledType,
                                   Probe, I>::value);
};

template <typename T, typename Probe, typename Category, size_t Target,
          template <typename...> typename IsConstructible, typename Sequence>
struct category_probe;

template <typename T, typename Probe, typename Category, size_t Target,
          template <typename...> typename IsConstructible, size_t... Is>
struct category_probe<T, Probe, Category, Target, IsConstructible,
                      std::index_sequence<Is...>>
    : IsConstructible<T, std::conditional_t<
                             Is == Target, argument<T, Probe, Is, Category>,
                             constructor_argument<T, constructor_shape>>...> {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          size_t Target, typename Category>
struct category_probe_id {};

template <
    typename T, typename DetectionMode,
    template <typename...> typename IsConstructible, size_t Arity,
    size_t Target, typename Category,
    bool = category_probe<T,
                          category_probe_id<T, DetectionMode, IsConstructible,
                                            Arity, Target, Category>,
                          Category, Target, IsConstructible,
                          std::make_index_sequence<Arity>>::value>
struct category_result {
  using type = void;
};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          size_t Target, typename Category>
struct category_result<T, DetectionMode, IsConstructible, Arity, Target,
                       Category, true> {
private:
  using probe = category_probe_id<T, DetectionMode, IsConstructible, Arity,
                                  Target, Category>;

public:
  using type =
      typename decltype(category_type(category_key<T, probe, Target>{}))::type;
};

template <typename Lvalue, typename Rvalue> struct resolve_reference_argument {
private:
  static constexpr bool is_const_lvalue =
      std::is_lvalue_reference_v<Lvalue> &&
      std::is_const_v<std::remove_reference_t<Lvalue>>;

public:
  using type = std::conditional_t<
      is_const_lvalue, Lvalue,
      std::conditional_t<!std::is_void_v<Lvalue> && !std::is_void_v<Rvalue>,
                         std::remove_reference_t<Rvalue>,
                         std::conditional_t<!std::is_void_v<Lvalue>, Lvalue,
                                            std::remove_reference_t<Rvalue>>>>;
};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          size_t Target,
          typename Lvalue =
              typename category_result<T, DetectionMode, IsConstructible, Arity,
                                       Target, lvalue_probe>::type,
          typename Rvalue =
              typename category_result<T, DetectionMode, IsConstructible, Arity,
                                       Target, rvalue_probe>::type,
          bool Missing = std::is_void_v<Lvalue> && std::is_void_v<Rvalue>,
          bool LvalueOnly = !std::is_void_v<Lvalue> && std::is_void_v<Rvalue>>
struct resolve_argument : resolve_reference_argument<Lvalue, Rvalue> {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          size_t Target, typename Lvalue, typename Rvalue>
struct resolve_argument<T, DetectionMode, IsConstructible, Arity, Target,
                        Lvalue, Rvalue, true, false>
    : category_result<T, DetectionMode, IsConstructible, Arity, Target,
                      selected_probe> {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          size_t Target, typename Lvalue, typename Rvalue>
struct resolve_argument<T, DetectionMode, IsConstructible, Arity, Target,
                        Lvalue, Rvalue, false, true> {
private:
  using value = typename category_result<T, DetectionMode, IsConstructible,
                                         Arity, Target, value_probe>::type;

public:
  using type = std::conditional_t<
      !std::is_void_v<value>, value,
      typename resolve_reference_argument<Lvalue, Rvalue>::type>;
};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          size_t Target>
using resolved_argument_t =
    typename resolve_argument<T, DetectionMode, IsConstructible, Arity,
                              Target>::type;

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          typename Sequence>
struct constructor_signature_recovery_msvc_impl;

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          size_t... Is>
struct constructor_signature_recovery_msvc_impl<
    T, DetectionMode, IsConstructible, Arity, std::index_sequence<Is...>>
    : constructor_signature_argument_list<resolved_argument_t<
          T, DetectionMode, IsConstructible, Arity, Is>...> {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          constructor_kind Kind>
struct constructor_signature_recovery_msvc {
  using type = void;
};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible>
struct constructor_signature_recovery_msvc<
    T, DetectionMode, IsConstructible, 0, constructor_kind::concrete> {
  using type = type_list<>;
};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity>
struct constructor_signature_recovery_msvc<
    T, DetectionMode, IsConstructible, Arity, constructor_kind::concrete>
    : constructor_signature_recovery_msvc_impl<
          T, DetectionMode, IsConstructible, Arity,
          std::make_index_sequence<Arity>> {};

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
