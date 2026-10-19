//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The configuration macros of the public API. A user defines any of them before
// including a Dingo header to override the default.

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
