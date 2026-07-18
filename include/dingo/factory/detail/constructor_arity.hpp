//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// GCC/Clang constructor arity detection.
//
// The search starts at the configured maximum and stops at the first viable
// constructor. Its zero-arity specializations rely on standard partial
// specialization ordering. This header is included from
// constructor_detection.hpp inside dingo::detail after the constructor probe
// primitives are defined.

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          bool Match = ConstructorProbe<T, DetectionMode, constructor_argument,
                                        IsConstructible, Arity>::value>
struct constructor_arity
    : constructor_arity<ConstructorProbe, T, DetectionMode, IsConstructible,
                        Arity - 1> {};

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity>
struct constructor_arity<ConstructorProbe, T, DetectionMode, IsConstructible,
                         Arity, true>
    : std::integral_constant<size_t, Arity> {};

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, typename DetectionMode,
          template <typename...> typename IsConstructible>
struct constructor_arity<ConstructorProbe, T, DetectionMode, IsConstructible, 0,
                         false>
    : std::integral_constant<size_t, invalid_arity> {};

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, typename DetectionMode,
          template <typename...> typename IsConstructible>
struct constructor_arity<ConstructorProbe, T, DetectionMode, IsConstructible, 0,
                         true>
    : std::integral_constant<size_t, 0> {};
