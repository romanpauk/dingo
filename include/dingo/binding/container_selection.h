//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The one consumer of a container's binding sources for one request. The
// container declares which sources it has and exposes them; this class owns the
// order in which they answer, the combination of a compile-time static source
// with a runtime one, and the hand-off to the container's miss rule. It is a
// base of the container, so the inherited resolve replaces the container's own
// entry: adding a wrapper here would cost one template level per dependency
// link, which the static-only path cannot afford.

#include <dingo/base/config.h>
#include <dingo/base/construction_scope.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/key.h>

#include <cstdlib>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

// `Self` declares:
//
//   static constexpr bool has_runtime_source;
//   static constexpr bool has_parent;
//   template <typename R, typename K> static constexpr binding_status
//       static_status_v;
//   template <typename R, typename K> using static_selection_t;
//   template <typename T> static constexpr bool may_auto_construct_v;
//   template <typename L, typename K> static constexpr bool parent_serves_v;
//   static_source(); runtime_source(); parent();
//   template <typename Request, typename R> resolve_collection_request(...);
//   template <typename Request, typename R, bool MayAuto, bool ParentServes>
//       resolve_missing(...);
template <typename Self> class container_selection {
  Self &self() { return static_cast<Self &>(*this); }

  // The collection miss hand-off. The singular path spells its two miss calls
  // inline: a helper there would add a template level to every dependency link,
  // and the static-only graph has no headroom for that.
  template <typename Request, typename R, typename Lookup, typename Context,
            typename Origin, typename LookupKey>
  R miss_collection(construction_scope scope, Context &context, Origin &origin,
                    LookupKey key) {
    return self()
        .template resolve_missing<
            Request, R, false,
            Self::template parent_serves_v<Lookup, LookupKey>>(
            scope, context, origin, std::move(key));
  }

  template <typename Request, typename LookupKey>
  auto select_runtime(LookupKey &key) {
    return self()
        .runtime_source()
        .template select_binding<typename Request::value_type,
                                 typename Request::exact_type>(key);
  }

  template <typename Request, typename R, typename Selection, typename Context>
  R resolve_runtime(Selection &selected, construction_scope scope,
                    Context &context) {
    return self()
        .runtime_source()
        .template resolve_named<typename Request::interface_type, R>(
            selected, scope, context);
  }

  template <typename Request, typename R, typename LookupKey, typename Context>
  R resolve_static(construction_scope scope, Context &context) {
    using lookup_type = typename Request::lookup_type;
    using selection =
        typename Self::template static_selection_t<lookup_type, LookupKey>;
    if constexpr (selection::status == binding_status::found) {
      return self()
          .static_source()
          .template resolve_binding<
              lookup_type, typename Request::interface_type, selection>(
              scope, context, self());
    } else {
      std::abort();
    }
  }

  template <typename Request, typename R, typename Context, typename Origin,
            typename LookupKey>
  R resolve_collection(construction_scope scope, Context &context,
                       Origin &origin, LookupKey key) {
    using lookup_type = typename Request::lookup_type;
    using resolve_type = typename collection_traits<R>::resolve_type;

    if constexpr (!Self::has_runtime_source) {
      constexpr std::size_t count =
          Self::template static_collection_count<R, LookupKey>();
      if constexpr (Self::template parent_serves_v<resolve_type, LookupKey> &&
                    count == 0) {
        return miss_collection<Request, R, resolve_type>(scope, context, origin,
                                                         std::move(key));
      } else {
        return self().template construct_static_collection<R, LookupKey>(
            scope, context);
      }
    } else {
      if constexpr (detail::is_no_lookup_key_v<LookupKey>) {
        auto selected = select_runtime<Request>(key);
        if (selected.status == binding_status::ambiguous) {
          throw make_type_ambiguous_exception<lookup_type>(context);
        }
        if (selected.status == binding_status::found) {
          return resolve_runtime<Request, R>(selected, scope, context);
        }
      }
      if (self().template count_collection<R>(key) == 0) {
        return miss_collection<Request, R, resolve_type>(scope, context, origin,
                                                         std::move(key));
      }
      if constexpr (detail::default_collection_append_v<R>) {
        return self().template construct_collection_request<R>(scope, context,
                                                               std::move(key));
      } else {
        return origin.template unresolved<Request, false, LookupKey, R>(
            scope, context, key);
      }
    }
  }

public:
  template <typename Request, typename R, typename Context, typename Origin,
            typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  R resolve(construction_scope scope, Context &context, Origin &origin,
            LookupKey key) {
    using lookup_type = typename Request::lookup_type;
    if constexpr (collection_traits<R>::is_collection) {
      return resolve_collection<Request, R>(scope, context, origin,
                                            std::move(key));
    } else if constexpr (!Self::has_runtime_source) {
      constexpr auto static_status =
          Self::template static_status_v<lookup_type, LookupKey>;
      if constexpr (static_status == binding_status::found) {
        return resolve_static<Request, R, LookupKey>(scope, context);
      } else if constexpr (static_status == binding_status::ambiguous) {
        static_assert(static_status != binding_status::ambiguous,
                      "static_container cannot resolve an ambiguously bound "
                      "type");
      } else {
        return self()
            .template resolve_missing<
                Request, R, false,
                Self::template parent_serves_v<lookup_type, LookupKey>>(
                scope, context, origin, std::move(key));
      }
    } else {
      auto selected = select_runtime<Request>(key);
      return detail::resolve_selection<R, lookup_type>(
          selected, context,
          [&]() -> R {
            return resolve_runtime<Request, R>(selected, scope, context);
          },
          [&]() -> R {
            return self()
                .template resolve_missing<
                    Request, R,
                    Self::template may_auto_construct_v<
                        typename Request::user_type>,
                    Self::template parent_serves_v<lookup_type, LookupKey>>(
                    scope, context, origin, std::move(key));
          });
    }
  }

  // Resolves a request from a runtime selection the container already made,
  // so a cache probe does not select the same key twice.
  template <typename Request, typename R, typename Selection, typename Context,
            typename Origin, typename LookupKey>
  R resolve_selected(Selection selected, construction_scope scope,
                     Context &context, Origin &origin, LookupKey key) {
    using lookup_type = typename Request::lookup_type;
    return detail::resolve_selection<R, lookup_type>(
        selected, context,
        [&]() -> R {
          return resolve_runtime<Request, R>(selected, scope, context);
        },
        [&]() -> R {
          return self()
              .template resolve_missing<
                  Request, R,
                  Self::template may_auto_construct_v<
                      typename Request::user_type>,
                  Self::template parent_serves_v<lookup_type, LookupKey>>(
                  scope, context, origin, std::move(key));
        });
  }
};

} // namespace detail
} // namespace dingo
