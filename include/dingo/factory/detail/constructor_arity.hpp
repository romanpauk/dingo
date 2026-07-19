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
//
// The recursion deliberately calls constructor_probe_value directly instead
// of carrying constructor_probe as a template-template argument. This keeps
// each tested arity to one detector node and avoids materializing a probe
// wrapper class for every step of the search.

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          bool Match = constructor_probe_value<
              T, DetectionMode, constructor_argument, IsConstructible>(
              std::make_index_sequence<Arity>{})>
struct constructor_arity
    : constructor_arity<T, DetectionMode, IsConstructible, Arity - 1> {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity>
struct constructor_arity<T, DetectionMode, IsConstructible, Arity, true>
    : std::integral_constant<size_t, Arity> {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible>
struct constructor_arity<T, DetectionMode, IsConstructible, 0, false>
    : std::integral_constant<size_t, invalid_arity> {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible>
struct constructor_arity<T, DetectionMode, IsConstructible, 0, true>
    : std::integral_constant<size_t, 0> {};
