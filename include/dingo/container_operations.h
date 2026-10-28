//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The public operations of a container, independent of how its sources are
// configured. The container provides the resolve entry; this layer owns the
// spellings, the key normalization, and the static minimal-context decision.

#include <dingo/base/config.h>
#include <dingo/base/construction_scope.h>
#include <dingo/base/none.h>
#include <dingo/static/resolution_policy.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/key.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename Self, bool RuntimeSource> class container_operations {
public:
  // A runtime-backed container resolves an id by building its lookup key.
  template <typename T, typename IdType = none_t,
            typename R = request_result_t<T, true>,
            std::enable_if_t<RuntimeSource && !detail::is_lookup_key_v<IdType>,
                             int> = 0>
  DINGO_ALWAYS_INLINE R resolve(IdType &&id = IdType()) {
    using request = request_type<T>;
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    return self().template resolve_entry<request, R>(std::move(key));
  }

  // A static-only container spells the compile-time key as a template
  // argument. The keyed overload below performs the resolution.
  template <typename T, typename Key = key_type<none_t>,
            typename R = request_result_t<T, true>,
            std::enable_if_t<!RuntimeSource && !std::is_void_v<T>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve() {
    using lookup_key_type =
        decltype(detail::make_lookup_key(type_selector<Key>{}));
    return resolve<T, lookup_key_type, R>(
        detail::make_lookup_key(type_selector<Key>{}));
  }

  template <typename T, typename LookupKey,
            typename R = request_result_t<T, true>,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(LookupKey key) {
    if constexpr (RuntimeSource) {
      using request = request_type<T>;
      return self().template resolve_entry<request, R>(std::move(key));
    } else {
      using lookup_request = request_type<T, true>;
      using request = request_type<T, false>;
      using check =
          typename Self::template resolve_request_check<request, R, LookupKey>;
      (void)sizeof(check);
      using policy =
          resolution_policy<T, LookupKey, typename Self::static_bindings_type,
                            resolution_mode::compile_time>;
      return policy::template resolve<R>(
          [&](auto) -> R {
            if constexpr (!collection_traits<R>::is_collection) {
              if constexpr (Self::template uses_static_minimal_context_v<
                                request, R, LookupKey>) {
                using selected =
                    typename Self::static_registry_type::template selection<
                        typename lookup_request::lookup_type, LookupKey>;
                typename Self::template static_resolution_context_t<request, R,
                                                                    LookupKey>
                    context;
                return self()
                    .static_registry_.template resolve_binding<
                        typename lookup_request::lookup_type, R, selected>(
                        ephemeral_scope, context, self());
              }
            }
            typename Self::context_type context;
            return self().selection_type::template resolve<request, R>(
                ephemeral_scope, context, self(), key);
          },
          [&](auto tag) -> R {
            using selected_policy = typename decltype(tag)::policy;
            return selected_policy::lift_type::make(
                resolve<typename selected_policy::inner_type, LookupKey,
                        request_result_t<typename selected_policy::inner_type,
                                         true>>(key));
          });
    }
  }

private:
  Self &self() { return static_cast<Self &>(*this); }
};

} // namespace detail
} // namespace dingo
