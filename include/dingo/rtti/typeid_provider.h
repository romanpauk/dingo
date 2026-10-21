//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/rtti/typeid_provider.h of the next implementation: the type identity
// provider container traits may name. The next implementation identifies
// types by their tags, so a provider does not change what it does.

namespace dingo {

template <typename Provider> struct rtti {};

struct typeid_provider {};

} // namespace dingo
