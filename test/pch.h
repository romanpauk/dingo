#pragma once

#if defined(_WIN32)
#define _ENFORCE_MATCHING_ALLOCATORS 0
#endif

#include <array>
#include <forward_list>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <typeindex>

#if defined(_WIN32)
#include <windows.h>

#if defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
// Use _CrtSetBreakAlloc(x);
#endif

#endif
