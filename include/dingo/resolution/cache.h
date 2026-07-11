//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

namespace dingo::detail::cache {
struct sink {
  void *state = nullptr;
  void (*publish)(void *, void *) = nullptr;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  void operator()(void *address) const {
    if (publish != nullptr) {
      publish(state, address);
    }
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
};
} // namespace dingo::detail::cache
