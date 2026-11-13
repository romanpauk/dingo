//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/construction_scope.h>
#include <dingo/base/selected.h>
#include <dingo/type/annotated.h>
#include <dingo/type/key.h>
#include <dingo/type/shape.h>

#include <type_traits>

// A request names what a container resolves: a type, perhaps annotated or
// selected by a key, in a category. This header takes it apart.

namespace dingo::detail {
template <typename Selector> struct dependency_selector {
  using type = Selector;
};

template <typename T> struct dependency_selector<key_type<T>> {
  using type = type_selector<T>;
};

template <typename Selector>
using dependency_selector_t = typename dependency_selector<Selector>::type;
} // namespace dingo::detail

namespace dingo {

// A dependency on T, selected by the key Selector names.
template <typename T, typename Selector> struct dependency_type {
  using type = detail::selected<T, detail::dependency_selector_t<Selector>>;
};

template <typename T> struct dependency_type<T, key_type<none_t>> {
  using type = T;
};

template <typename T, typename Selector = key_type<none_t>>
using dependency = typename dependency_type<T, std::decay_t<Selector>>::type;

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
using request_result_t =
    unannotated_t<detail::request_lookup_t<Request, RemoveRvalueReferences>>;

template <typename Request, bool RemoveRvalueReferences = false>
using request_value_t = normalized_type_t<
    detail::request_lookup_t<Request, RemoveRvalueReferences>>;

// The type a request constructs, which an annotation or a key selection only
// names.
template <typename Request>
using request_constructed_t = unannotated_t<normalized_type_t<Request>>;

template <typename Request, bool RemoveRvalueReferences = false>
struct request_type {
  using user_type = Request;
  using lookup_type = detail::request_lookup_t<Request, RemoveRvalueReferences>;
  using interface_type = unannotated_t<lookup_type>;
  using value_type = normalized_type_t<lookup_type>;
  using exact_type = std::remove_cv_t<std::remove_reference_t<interface_type>>;
  static constexpr bool removes_rvalue_references = RemoveRvalueReferences;
};

// The value a reference or a pointer request refers to.
template <typename Request>
using referred_value_t =
    std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<Request>>>;

// Whether what a request resolves may outlive the resolution: a reference or
// a pointer.
template <typename Request>
inline constexpr bool request_may_escape_v =
    std::is_lvalue_reference_v<
        typename request_type<Request>::interface_type> ||
    std::is_pointer_v<typename request_type<Request>::interface_type>;

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
  } else if constexpr (is_fixed_construction_scope<Scope>::value) {
    (void)scope;
    return ephemeral_scope;
  } else {
    (void)scope;
    return construction_scope(construction_lifetime::ephemeral);
  }
}
} // namespace detail

} // namespace dingo
