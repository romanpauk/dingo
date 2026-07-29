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
// The detector also names constructor_arity_probe_msvc directly and selects
// the recursive or terminal case on constructor_arity_msvc itself. Keeping the
// probe out of the template arguments and avoiding a separate step class keeps
// every tested arity to one detector node.
//
// This header is included from constructor_detection_msvc.hpp inside
// dingo::detail after the constructor probe primitives are defined.

template <typename T, template <typename...> typename IsConstructible,
          size_t MaxDepth,
          typename Request =
              typename constructor_request_search<
                  constructor_probe_msvc, T, IsConstructible, MaxDepth>::type>
struct constructor_arity_one_msvc
    : constructor_arity_probe_msvc<T, constructor_shape, constructor_argument,
                                   IsConstructible, 1> {};

template <typename T, template <typename...> typename IsConstructible,
          size_t MaxDepth, constructor_request_kind Kind, size_t Depth>
struct constructor_arity_one_msvc<
    T, IsConstructible, MaxDepth, constructor_request<Kind, Depth>>
    : std::true_type {};

template <typename T, template <typename...> typename IsConstructible,
          size_t MaxDepth>
struct constructor_arity_one_msvc<T, IsConstructible, MaxDepth,
                                  ambiguous_constructor_request>
    : std::false_type {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          size_t MaxDepth>
struct constructor_arity_match_msvc
    : constructor_arity_probe_msvc<T, DetectionMode, constructor_argument,
                                   IsConstructible, Arity> {};

// VS2022's C++17 std::variant may instantiate an invalid nested alternative
// conversion while probing the unrestricted one-argument shape. A structural
// request is sufficient to establish arity one and keeps that probe lazy.
template <typename T, template <typename...> typename IsConstructible,
          size_t MaxDepth>
struct constructor_arity_match_msvc<T, constructor_shape, IsConstructible, 1,
                                    MaxDepth>
    : constructor_arity_one_msvc<T, IsConstructible, MaxDepth> {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          size_t MaxDepth = DINGO_CONSTRUCTOR_REQUEST_DEPTH,
          bool Match = constructor_arity_match_msvc<
              T, DetectionMode, IsConstructible, Arity, MaxDepth>::value,
          typename = void>
struct constructor_arity_msvc;

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          size_t MaxDepth>
struct constructor_arity_msvc<T, DetectionMode, IsConstructible, Arity,
                              MaxDepth, false,
                              std::enable_if_t<(Arity > 0)>>
    : constructor_arity_msvc<T, DetectionMode, IsConstructible, Arity - 1,
                             MaxDepth> {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t Arity,
          size_t MaxDepth>
struct constructor_arity_msvc<T, DetectionMode, IsConstructible, Arity,
                              MaxDepth, true,
                              std::enable_if_t<(Arity > 0)>>
    : std::integral_constant<size_t, Arity> {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t MaxDepth>
struct constructor_arity_msvc<T, DetectionMode, IsConstructible, 0, MaxDepth,
                              false, void>
    : std::integral_constant<size_t, invalid_arity> {};

template <typename T, typename DetectionMode,
          template <typename...> typename IsConstructible, size_t MaxDepth>
struct constructor_arity_msvc<T, DetectionMode, IsConstructible, 0, MaxDepth,
                              true, void>
    : std::integral_constant<size_t, 0> {};
