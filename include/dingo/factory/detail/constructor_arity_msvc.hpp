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

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          typename = void>
struct constructor_arity_msvc;

template <bool Match,
          template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity>
struct constructor_arity_msvc_step;

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity>
struct constructor_arity_msvc<ConstructorProbe, T, DetectionMode,
                              IsConstructible, Arity,
                              std::enable_if_t<(Arity > 0)>>
    : constructor_arity_msvc_step<
          ConstructorProbe<T, DetectionMode, constructor_argument,
                           IsConstructible, Arity>::value,
          ConstructorProbe, T, DetectionMode, IsConstructible, Arity> {};

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, typename DetectionMode,
          template <typename...> typename IsConstructible>
struct constructor_arity_msvc<ConstructorProbe, T, DetectionMode,
                              IsConstructible, 0, void>
    : std::integral_constant<
          size_t,
          ConstructorProbe<T, DetectionMode, constructor_argument,
                           IsConstructible, 0>::value
              ? 0
              : invalid_arity> {};

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity>
struct constructor_arity_msvc_step<
    false, ConstructorProbe, T, DetectionMode, IsConstructible, Arity>
    : constructor_arity_msvc<ConstructorProbe, T, DetectionMode,
                             IsConstructible, Arity - 1> {};

template <template <typename, typename, template <class, class> class,
                    template <typename...> typename, size_t>
          typename ConstructorProbe,
          typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity>
struct constructor_arity_msvc_step<
    true, ConstructorProbe, T, DetectionMode, IsConstructible, Arity>
    : std::integral_constant<size_t, Arity> {};
