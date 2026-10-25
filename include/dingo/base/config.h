//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#if !defined(DINGO_CONSTRUCTOR_DETECTION_ARGS)
#define DINGO_CONSTRUCTOR_DETECTION_ARGS 32
#endif

#if !defined(DINGO_CONTEXT_ARENA_BUFFER_SIZE)
#define DINGO_CONTEXT_ARENA_BUFFER_SIZE 256
#endif

#if !defined(DINGO_RUNTIME_ARENA_BLOCK_SIZE)
#define DINGO_RUNTIME_ARENA_BLOCK_SIZE 256
#endif

#if !defined(DINGO_ALWAYS_INLINE)
#if defined(_MSC_VER)
#define DINGO_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define DINGO_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define DINGO_ALWAYS_INLINE inline
#endif
#endif

#if !defined(DINGO_NOINLINE)
#if defined(_MSC_VER)
#define DINGO_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define DINGO_NOINLINE __attribute__((noinline))
#else
#define DINGO_NOINLINE
#endif
#endif

// MSVC applies the empty base optimization only to the first empty base class
// unless the derived class opts in.
#if !defined(DINGO_EMPTY_BASES)
#if defined(_MSC_VER)
#define DINGO_EMPTY_BASES __declspec(empty_bases)
#else
#define DINGO_EMPTY_BASES
#endif
#endif

#if __cplusplus > 202002L || (defined(_MSVC_LANG) && _MSVC_LANG > 202002L)
#define DINGO_CXX_STANDARD 23
#elif (__cplusplus > 201703L && __cplusplus <= 202002L) ||                     \
    (defined(_MSVC_LANG) && _MSVC_LANG == 202002L)
#define DINGO_CXX_STANDARD 20
#elif __cplusplus <= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG == 201703L)
#define DINGO_CXX_STANDARD 17
#endif

// Indexes a type pack with the compiler builtin, in place of the portable
// inheritance selection.
#if defined(__has_builtin)
#if __has_builtin(__type_pack_element)
#define DINGO_HAS_TYPE_PACK_ELEMENT 1
#else
#define DINGO_HAS_TYPE_PACK_ELEMENT 0
#endif
#else
#define DINGO_HAS_TYPE_PACK_ELEMENT 0
#endif

// Indexes a type pack directly with Ts...[I], in place of the builtin.
#if defined(__cpp_pack_indexing) && __cpp_pack_indexing >= 202311L
#define DINGO_HAS_PACK_INDEXING 1
#else
#define DINGO_HAS_PACK_INDEXING 0
#endif

#if !defined(DINGO_VARIANT_PACK_AT_INDEXING)
#define DINGO_VARIANT_PACK_AT_INDEXING DINGO_HAS_PACK_INDEXING
#endif

#if !defined(DINGO_VARIANT_PACK_AT_BUILTIN)
#define DINGO_VARIANT_PACK_AT_BUILTIN DINGO_HAS_TYPE_PACK_ELEMENT
#endif
