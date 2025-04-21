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

#if !defined(DINGO_CONTEXT_SIZE)
#define DINGO_CONTEXT_SIZE 32
#endif

#if __cplusplus > 202002L || (defined(_MSVC_LANG) && _MSVC_LANG > 202002L)
#define DINGO_CXX_STANDARD 23
#elif (__cplusplus > 201703L && __cplusplus <= 202002L) || (defined(_MSVC_LANG) && _MSVC_LANG == 202002L)
#define DINGO_CXX_STANDARD 20
#elif __cplusplus <= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG == 201703L)
#define DINGO_CXX_STANDARD 17
#endif
