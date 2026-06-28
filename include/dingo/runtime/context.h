//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/context_base.h>

namespace dingo {

class runtime_context : public detail::context_state {
public:
  template <typename T, typename Container> T resolve(Container &container) {
    if constexpr (detail::is_selected_v<T>) {
      using request_type = detail::selected_type_t<T>;
      using selector_type = detail::selected_selector_t<T>;
      if constexpr (detail::is_type_selector_v<selector_type>) {
        using key_type = detail::type_selector_type_t<selector_type>;
        return T(container.template resolve<request_type, false, true>(
            *this, key<key_type>{}));
      } else if constexpr (detail::is_value_selector_v<selector_type>) {
        return T(container.template resolve<request_type, false, true>(
            *this, detail::is_value_selector<selector_type>::make()));
      } else {
        static_assert(detail::is_type_selector_v<selector_type> ||
                          detail::is_value_selector_v<selector_type>,
                      "detail::selected<T, Selector> requires a "
                      "type_selector or value_selector");
      }
    } else if constexpr (is_keyed_v<T>) {
      using request_type = keyed_type_t<T>;
      using key_type = keyed_key_t<T>;
      return T(container.template resolve<request_type, false, true>(
          *this, key<key_type>{}));
    } else if constexpr (is_indexed_v<T>) {
      using request_type = indexed_type_t<T>;
      using selector_type = indexed_selector_t<T>;
      if constexpr (detail::is_key_value_v<selector_type>) {
        using key_type =
            typename detail::key_selector_value<selector_type>::type;
        return T(container.template resolve<request_type, false, true>(
            *this,
            key_type{detail::key_selector_value<selector_type>::make()}));
      } else {
        static_assert(detail::is_key_value_v<selector_type>,
                      "dingo::indexed<T, dingo::key<Key>> constructor "
                      "injection requires dingo::key<Key, Value>");
      }
    } else {
      return container.template resolve<T, false>(*this);
    }
  }

  template <typename T, typename DetectionTag, typename Container>
  T construct_temporary(Container &container) {
    using temporary_type = normalized_type_t<T>;

    auto &active_closure = *closures_.back();
    arena_allocator<void> alloc(active_closure.arena_storage());
    auto allocator = allocator_traits::rebind<temporary_type>(alloc);
    auto instance = allocator_traits::allocate(allocator, 1);
    detail::default_constructor_detection<temporary_type, DetectionTag>()
        .template construct<temporary_type>(instance, *this, container);
    if constexpr (!std::is_trivially_destructible_v<temporary_type>) {
      register_destructor(instance);
    }

    if constexpr (std::is_lvalue_reference_v<T>) {
      return *instance;
    } else {
      return std::move(*instance);
    }
  }
};

} // namespace dingo
