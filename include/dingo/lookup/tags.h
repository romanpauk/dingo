//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

namespace dingo {
template <typename... Definitions> struct lookups {};
struct no_key {};
template <typename Key> struct typed_key {};
template <typename Key> struct runtime_key {};
struct one {};
struct many {};

namespace detail {
struct no_lookup_backend {};
template <typename Interface, typename KeyDomain, typename Cardinality,
          typename Backend = no_lookup_backend>
struct lookup_definition {};
} // namespace detail

} // namespace dingo
