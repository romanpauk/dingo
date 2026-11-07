//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/construction_scope.h>
#include <dingo/type/annotated.h>
#include <dingo/type/normalized_type.h>

#include <type_traits>

namespace dingo {

namespace detail {
// The interface a request names, without its selector or annotation.
template <typename Request> using request_interface_t = unannotated_t<Request>;
} // namespace detail

namespace detail {
// A request without its rvalue reference, also inside an annotation or a
// selector: a resolved rvalue reference is returned by value.
template <typename Request> struct remove_rvalue_request {
  using type = std::conditional_t<std::is_rvalue_reference_v<Request>,
                                  std::remove_reference_t<Request>, Request>;
};

template <typename T, typename Tag>
struct remove_rvalue_request<annotated<T, Tag>> {
  using type = annotated<typename remove_rvalue_request<T>::type, Tag>;
};

template <typename T, typename Selector>
struct remove_rvalue_request<selected<T, Selector>> {
  using type = selected<typename remove_rvalue_request<T>::type, Selector>;
};

template <typename Request, bool RemoveRvalueReferences>
using request_lookup_t =
    std::conditional_t<RemoveRvalueReferences,
                       typename remove_rvalue_request<Request>::type, Request>;
} // namespace detail

// Lazy single-member views of request_type. Naming one of these does not
// instantiate request_type, so default template arguments that only need the
// result or the normalized value avoid forming the remaining members.
template <typename Request, bool RemoveRvalueReferences = false>
using request_result_t = detail::request_interface_t<
    detail::request_lookup_t<Request, RemoveRvalueReferences>>;

template <typename Request, bool RemoveRvalueReferences = false>
using request_value_t = normalized_type_t<
    detail::request_lookup_t<Request, RemoveRvalueReferences>>;

template <typename Request, bool RemoveRvalueReferences = false>
struct request_type {
  using user_type = Request;
  using lookup_type = detail::request_lookup_t<Request, RemoveRvalueReferences>;
  using interface_type = detail::request_interface_t<lookup_type>;
  using value_type = normalized_type_t<lookup_type>;
  using exact_type = std::remove_cv_t<std::remove_reference_t<interface_type>>;

  static constexpr bool removes_rvalue_references = RemoveRvalueReferences;
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
// The scope a dependency is resolved in. A reference or a pointer a class
// keeps lives as long as the class: as long as the container, when a scope
// keeps the class, or as long as the caller uses it, when the class is
// returned to the caller. Anything else is used while the class is
// constructed.
template <typename Request, typename Scope>
constexpr auto dependency_scope(Scope scope) {
  if constexpr (request_may_escape_v<Request>) {
    return scope;
  } else {
    (void)scope;
    return ephemeral_scope;
  }
}
} // namespace detail

} // namespace dingo
