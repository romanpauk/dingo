//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/key/annotated.h>
#include <dingo/runtime/data.h>

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace dingo::detail {

struct runtime_binding_value {
  runtime_binding_value(const runtime_binding_value &) = delete;
  runtime_binding_value &operator=(const runtime_binding_value &) = delete;
  runtime_binding_value(runtime_binding_value &&) = delete;
  runtime_binding_value &operator=(runtime_binding_value &&) = delete;

protected:
  runtime_binding_value() = default;
  ~runtime_binding_value() = default;
};

template <std::size_t Index, typename Binding>
struct runtime_binding_member : Binding {
  template <typename... Args>
  explicit runtime_binding_member(Args &&...args)
      : Binding(std::forward<Args>(args)...) {}

  typename Binding::container_type &get_container() {
    return Binding::get_container();
  }
};

template <typename BindingInterface, typename Derived, typename Indexes,
          typename... Bindings>
struct runtime_binding_value_impl_base;

template <typename BindingInterface, typename Derived, std::size_t... Indexes,
          typename... Bindings>
struct runtime_binding_value_impl_base<
    BindingInterface, Derived, std::index_sequence<Indexes...>, Bindings...>
    : runtime_binding_value, runtime_binding_member<Indexes, Bindings>... {
  using container_type =
      typename type_list_head_t<type_list<Bindings...>>::container_type;

  // NOLINTBEGIN(bugprone-use-after-move)
  template <typename... Args>
  explicit runtime_binding_value_impl_base(Args &&...args)
      : runtime_binding_member<Indexes, Bindings>(
            std::forward<Args>(args)...)... {}
  // NOLINTEND(bugprone-use-after-move)

  template <typename Interface>
  runtime_lookup_binding_view<BindingInterface> view() {
    return view_impl<Interface, 0, Bindings...>();
  }

  container_type &get_container() {
    return static_cast<runtime_binding_member<
        0, type_list_head_t<type_list<Bindings...>>> &>(*this)
        .get_container();
  }

private:
  template <typename Interface, std::size_t Index, typename Head,
            typename... Tail>
  runtime_lookup_binding_view<BindingInterface> view_impl() {
    if constexpr (std::is_same_v<typename Head::interface_type,
                                 typename annotated_traits<Interface>::type>) {
      auto &binding = static_cast<runtime_binding_member<Index, Head> &>(*this);
      return {std::addressof(binding)};
    } else if constexpr (sizeof...(Tail) > 0) {
      return view_impl<Interface, Index + 1, Tail...>();
    } else {
      static_assert(sizeof...(Tail) > 0,
                    "lookup interface is not present in runtime binding "
                    "value");
    }
  }
};

template <typename BindingInterface, typename... Bindings>
struct runtime_binding_value_impl final
    : runtime_binding_value_impl_base<
          BindingInterface,
          runtime_binding_value_impl<BindingInterface, Bindings...>,
          std::index_sequence_for<Bindings...>, Bindings...> {
  using base_type = runtime_binding_value_impl_base<
      BindingInterface,
      runtime_binding_value_impl<BindingInterface, Bindings...>,
      std::index_sequence_for<Bindings...>, Bindings...>;

  using base_type::base_type;
};

} // namespace dingo::detail
