//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/context/context_base.h>

#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {

// The resolution context of both containers. The backend provides the memory
// of the instances a resolution constructs, construct_storage<T>(scope, fn),
// and the hooks a transactional resolution reports to: on_rollback and
// add_runtime_destructor. A backend without a transaction ignores them.
template <typename Backend>
class resolution_context : public detail::context_path_state, public Backend {
public:
  using backend_type = Backend;
  using Backend::Backend;

  resolution_context() = default;
  resolution_context(const resolution_context &) = delete;
  resolution_context &operator=(const resolution_context &) = delete;

  template <typename T, typename... Args>
  T &construct(construction_scope scope, Args &&...args) {
    return Backend::template construct_storage<T>(
        scope, [&](void *ptr) { new (ptr) T(std::forward<Args>(args)...); });
  }

  template <typename T, typename DetectionMode, typename Container>
  T construct(construction_scope scope, Container &container) {
    using temporary_type = normalized_type_t<T>;
    auto &instance = Backend::template construct_storage<temporary_type>(
        scope, [&](void *ptr) {
          constructor_detection<temporary_type, DetectionMode>()
              .template construct<temporary_type>(ptr, scope, *this, container);
        });

    if constexpr (std::is_lvalue_reference_v<T>) {
      return instance;
    } else if constexpr (!std::is_rvalue_reference_v<T> &&
                         std::is_copy_constructible_v<temporary_type> &&
                         !std::is_move_constructible_v<temporary_type>) {
      return static_cast<const temporary_type &>(instance);
    } else {
      return std::move(instance);
    }
  }

  template <typename T, typename Container>
  T resolve(construction_scope scope, Container &container) {
    if constexpr (detail::is_selected_v<T>) {
      using request_type = detail::selected_type_t<T>;
      using selector_type = detail::selected_selector_t<T>;
      if constexpr (detail::is_type_selector_v<selector_type>) {
        using selector_key_type = detail::type_selector_type_t<selector_type>;
        return T(container.template resolve<request_type, false>(
            scope, *this,
            detail::make_lookup_key(
                detail::type_selector<selector_key_type>{})));
      } else {
        static_assert(detail::is_value_selector_v<selector_type>);
        return T(container.template resolve<request_type, false>(
            scope, *this, detail::make_lookup_key(selector_type{})));
      }
    } else {
      return container.template resolve<T, false>(scope, *this,
                                                  detail::no_lookup_key());
    }
  }
};

namespace detail {
template <typename Backend>
struct is_runtime_context<resolution_context<Backend>>
    : std::bool_constant<Backend::is_transactional> {};

template <typename Backend>
struct is_static_context<resolution_context<Backend>>
    : std::bool_constant<!Backend::is_transactional> {};
} // namespace detail

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
