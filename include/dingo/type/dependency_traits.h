//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/construction_scope.h>
#include <dingo/registration/annotated.h>
#include <dingo/type/normalized_type.h>

#include <type_traits>

namespace dingo {

namespace detail {
template <typename Request> struct request_interface {
  using type = typename annotated_traits<Request>::type;
};

template <typename T, typename Selector>
struct request_interface<selected<T, Selector>> {
  using type = T;
};

template <typename T, typename Selector>
struct request_interface<selected<T, Selector> &> {
  using type = T &;
};

template <typename T, typename Selector>
struct request_interface<selected<T, Selector> &&> {
  using type = T &&;
};

template <typename T, typename Selector>
struct request_interface<selected<T, Selector> *> {
  using type = T *;
};
} // namespace detail

template <typename Request, bool RemoveRvalueReferences = false>
struct request_type {
  using user_type = Request;
  using lookup_type = std::conditional_t<
      RemoveRvalueReferences,
      std::conditional_t<std::is_rvalue_reference_v<Request>,
                         std::remove_reference_t<Request>, Request>,
      Request>;
  using interface_type = typename detail::request_interface<lookup_type>::type;
  using result_type = interface_type;
  using value_type = normalized_type_t<lookup_type>;
  using exact_type = std::remove_cv_t<std::remove_reference_t<interface_type>>;

  static constexpr bool removes_rvalue_references = RemoveRvalueReferences;
  static constexpr bool is_rvalue_request = std::is_rvalue_reference_v<Request>;
};

template <typename Request>
struct request_may_escape
    : std::bool_constant<
          std::is_lvalue_reference_v<
              typename request_type<Request>::interface_type> ||
          std::is_pointer_v<typename request_type<Request>::interface_type>> {};

template <typename Request>
inline constexpr bool request_may_escape_v = request_may_escape<Request>::value;

namespace detail {
template <typename Request>
constexpr construction_scope dependency_scope(construction_scope scope) {
  return scope.is_persistent() && request_may_escape_v<Request>
             ? persistent_scope
             : ephemeral_scope;
}
} // namespace detail

} // namespace dingo
