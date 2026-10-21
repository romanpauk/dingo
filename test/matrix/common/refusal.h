//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Whether a container refuses a request when the request is compiled rather
// than when it is resolved. The next implementation decides a request its
// static bindings name when the request is compiled, so a static_assert
// rejects one they cannot serve, where the current implementation throws; a
// test that asserts the throw skips it then. The spec checks these refusals
// as compile errors in its route and selection tables.

#include <dingo/container.h>

#include <cstddef>
#include <type_traits>

namespace dingo::matrix {

namespace detail {
template <typename Container, typename Request, typename = void>
struct static_count : std::integral_constant<std::size_t, 0> {};

#if !defined(DINGO_MATRIX_BASELINE)
template <typename Container, typename Request>
struct static_count<Container, Request,
                    std::enable_if_t<Container::has_static_source>>
    : std::integral_constant<
          std::size_t, Container::static_source_type::template fixed_count<
                           Request, ::dingo::core::no_key>()> {};
#endif
} // namespace detail

// A request one static binding names, which it cannot serve.
template <typename Container, typename Request>
inline constexpr bool conversion_refused_when_compiled_v =
    detail::static_count<Container, Request>::value == 1;

namespace detail {
template <typename Container, typename Request, typename = void>
struct ambiguity_refused : std::false_type {};

#if !defined(DINGO_MATRIX_BASELINE)
template <typename Container, typename Request>
struct ambiguity_refused<Container, Request,
                         std::enable_if_t<Container::has_static_source>>
    : std::bool_constant<static_count<Container, Request>::value != 1 &&
                         static_count<Container, Request>::value != 0> {};
#endif
} // namespace detail

// A request several static bindings of a container name, which a runtime
// registration cannot make unambiguous.
template <typename Container, typename Request>
inline constexpr bool ambiguity_refused_when_compiled_v =
    detail::ambiguity_refused<Container, Request>::value;

} // namespace dingo::matrix
