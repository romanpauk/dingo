#pragma once

#include <map>
#include <type_traits>
#include <memory>
#include <typeindex>
#include <iostream>
#include <optional>
#include <tuple>
#include <array>
#include <forward_list>
#include <list>

#if defined(_WIN32)
#include <windows.h>

#if defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
// Use _CrtSetBreakAlloc(x);
#endif

#endif

