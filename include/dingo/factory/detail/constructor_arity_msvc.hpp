//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// MSVC-compatible constructor arity detection.
//
// MSVC can instantiate a recursive primary before selecting a zero-arity
// specialization. This implementation leaves the primary undefined and makes
// recursion available only through a nonzero specialization, so zero can never
// decrement the unsigned arity.
//
// This header is included from constructor_detection_msvc.hpp inside
// dingo::detail after the constructor probe primitives are defined.

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          bool Match = constructor_arity_probe_msvc<
              T, DetectionMode, constructor_argument, IsConstructible,
              Arity>::value,
          typename = void>
struct constructor_arity_msvc;

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity>
struct constructor_arity_msvc<T, DetectionMode, IsConstructible, Arity, false,
                              std::enable_if_t<(Arity > 0)>>
    : constructor_arity_msvc<T, DetectionMode, IsConstructible, Arity - 1> {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity>
struct constructor_arity_msvc<T, DetectionMode, IsConstructible, Arity, true,
                              std::enable_if_t<(Arity > 0)>>
    : std::integral_constant<size_t, Arity> {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible>
struct constructor_arity_msvc<T, DetectionMode, IsConstructible, 0, false, void>
    : std::integral_constant<size_t, invalid_arity> {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible>
struct constructor_arity_msvc<T, DetectionMode, IsConstructible, 0, true, void>
    : std::integral_constant<size_t, 0> {};
