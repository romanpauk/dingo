//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// MSVC-compatible constructor detection composition.
//
// This keeps the type-based per-arity probe, explicit-zero arity search, and
// targeted signature recovery separate from the lighter GCC/Clang path. The
// header is also directly usable by GCC/Clang tests.

#include <dingo/factory/detail/constructor_detection.hpp>

namespace dingo {
namespace detail {

template <typename T, typename DetectionMode,
          template <class, class> class ConstructorArg,
          template <typename...> typename IsConstructible, typename Sequence>
struct constructor_probe_msvc_impl;

template <typename T, typename DetectionMode,
          template <class, class> class ConstructorArg,
          template <typename...> typename IsConstructible, size_t... Is>
struct constructor_probe_msvc_impl<T, DetectionMode, ConstructorArg, IsConstructible,
                                   std::index_sequence<Is...>>
    : IsConstructible<
          T, std::conditional_t<true, ConstructorArg<T, DetectionMode>,
                                std::integral_constant<size_t, Is>>...> {};

// Arity search can never receive invalid_arity, so it bypasses the guarded
// wrapper below. The direct alias avoids one class instantiation per tested
// arity while constructor_probe_msvc remains safe for the shared result code.
template <typename T, typename DetectionMode,
          template <class, class> class ConstructorArg,
          template <typename...> typename IsConstructible, size_t Arity>
using constructor_arity_probe_msvc = constructor_probe_msvc_impl<
    T, DetectionMode, ConstructorArg, IsConstructible,
    std::make_index_sequence<Arity>>;

template <typename T, typename DetectionMode,
          template <class, class> class ConstructorArg,
          template <typename...> typename IsConstructible, size_t Arity>
struct constructor_probe_msvc
    : constructor_probe_msvc_impl<T, DetectionMode, ConstructorArg, IsConstructible,
                                  std::make_index_sequence<Arity>> {};

// MSVC may instantiate a probe from a discarded if constexpr branch. Keep its
// invalid-arity sentinel away from make_index_sequence without burdening the
// shared detection path with compiler-specific normalization.
template <typename T, typename DetectionMode,
          template <class, class> class ConstructorArg,
          template <typename...> typename IsConstructible>
struct constructor_probe_msvc<T, DetectionMode, ConstructorArg, IsConstructible,
                              invalid_arity> : std::false_type {};

#include <dingo/factory/detail/constructor_arity_msvc.hpp>
#include <dingo/factory/detail/constructor_signature.hpp>
#include <dingo/factory/detail/constructor_signature_msvc.hpp>

// Keep the shape result as an alias and reuse it for signature recovery.
template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible,
          size_t N = DINGO_CONSTRUCTOR_DETECTION_ARGS>
using constructor_detection_msvc_shape = constructor_detection_impl<
    constructor_probe_msvc,
    constructor_arity_msvc<T, DetectionMode, IsConstructible, N>, T,
    DetectionMode, IsConstructible, N>;

#if defined(_MSC_VER)
template <typename T, typename Sequence>
struct constructor_array_arguments;

template <typename T, size_t... Is>
struct constructor_array_arguments<T, std::index_sequence<Is...>> {
  using type = type_list<repeated_type<T, Is>...>;
};

// MSVC 2022 in C++17 mode sees std::array as its single internal native-array
// member. Publish the standard container's element signature instead.
template <typename T, size_t N, typename DetectionMode>
struct constructor_array_detection
    : constructor_detection_dispatch<std::array<T, N>, DetectionMode, N,
                                     constructor_kind::concrete> {
  static constexpr constructor_kind kind = constructor_kind::concrete;
  static constexpr size_t arity = N;
  using arguments = std::conditional_t<
      std::is_same_v<DetectionMode, constructor_signature>,
      typename constructor_array_arguments<T,
                                           std::make_index_sequence<N>>::type,
      std::conditional_t<N == 0, type_list<>, void>>;
};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible,
          size_t N = DINGO_CONSTRUCTOR_DETECTION_ARGS>
struct constructor_detection_msvc
    : std::conditional_t<
          std::is_same_v<DetectionMode, constructor_signature>,
          constructor_detection_signature_impl<
              constructor_detection_msvc_shape,
              constructor_signature_recovery_msvc, T, IsConstructible, N>,
          constructor_detection_msvc_shape<T, DetectionMode, IsConstructible,
                                           N>> {};

template <typename T, size_t Size, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t N>
struct constructor_detection_msvc<std::array<T, Size>, DetectionMode,
                                  IsConstructible, N>
    : constructor_array_detection<T, Size, DetectionMode> {};
#else
// Searches constructor arity in the inclusive range [0, N].
template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible,
          size_t N = DINGO_CONSTRUCTOR_DETECTION_ARGS>
using constructor_detection_msvc = std::conditional_t<
    std::is_same_v<DetectionMode, constructor_signature>,
    constructor_detection_signature_impl<
        constructor_detection_msvc_shape,
        constructor_signature_recovery_msvc, T, IsConstructible, N>,
    constructor_detection_msvc_shape<T, DetectionMode, IsConstructible, N>>;
#endif

} // namespace detail
} // namespace dingo
