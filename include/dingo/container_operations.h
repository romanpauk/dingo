//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The public operations of a container, independent of how its sources are
// configured. The container provides the resolve entry; this layer owns the
// spellings and the key normalization.

#include <dingo/base/config.h>
#include <dingo/base/none.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/key.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename Self> class container_operations {
public:
  template <typename T, typename IdType = none_t,
            typename R = request_result_t<T, true>,
            std::enable_if_t<!detail::is_lookup_key_v<IdType>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(IdType &&id = IdType()) {
    using request = request_type<T>;
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    return self().template resolve_entry<request, R>(std::move(key));
  }

  template <typename T, typename LookupKey,
            typename R = request_result_t<T, true>,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(LookupKey key) {
    using request = request_type<T>;
    return self().template resolve_entry<request, R>(std::move(key));
  }

private:
  Self &self() { return static_cast<Self &>(*this); }
};

} // namespace detail
} // namespace dingo
