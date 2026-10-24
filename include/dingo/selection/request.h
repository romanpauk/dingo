//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The descriptor of a request type.

#include <dingo/conversion/route.h>
#include <dingo/lookup/annotated_key.h>
#include <dingo/lookup/key.h>
#include <dingo/type/annotated.h>
#include <dingo/type/wrapper_traits.h>

#include <type_traits>

namespace dingo {

namespace detail {
template <typename Key, typename = void> struct annotation_of {
  using type = void;
};

template <typename Key>
struct annotation_of<Key, std::void_t<typename Key::annotation>> {
  using type = typename Key::annotation;
};

// The nodes of the type tree of a request of `Type`, whose leaf has the
// identity of the interface annotated by `Annotation`, unless it is void, in an
// array of exactly as many.
template <typename Type, typename Annotation> constexpr auto request_nodes() {
  auto result = type_tree_nodes_v<Type>;
  if constexpr (!std::is_void_v<Annotation>) {
    using leaf = leaf_of_t<Type>;
    std::size_t index = 0;
    while (is_wrapper(result.items[index].kind)) {
      // A wrapper of more than one argument keeps its leaf.
      if (result.items[index].arity != 1) {
        return result;
      }
      ++index;
    }
    result.items[index] = type_tree_nodes_v<leaf>.items[0];
    result.items[index].type = type_id_v<::dingo::annotated<leaf, Annotation>>;
  }
  return result;
}
} // namespace detail

// The descriptor of a request of the type `Request` by the key `Key`, which
// may carry the annotation of the interface it names.
template <typename Request, typename Key = no_key> struct request_traits {
  static constexpr request_kind kind =
      std::is_lvalue_reference_v<Request>   ? request_kind::lvalue_reference
      : std::is_rvalue_reference_v<Request> ? request_kind::rvalue_reference
                                            : request_kind::value;
  // The type the request asks for, which a reachable type of its type tree
  // provides, with the qualifiers of the objects it holds or refers to.
  using value_type = std::remove_cv_t<std::remove_reference_t<Request>>;
  // The type a binding makes the object of a constructed request as, without
  // qualifiers, which converts to the request.
  using made_type = canonical_t<std::remove_reference_t<Request>>;
  static constexpr auto nodes =
      detail::request_nodes<std::remove_reference_t<Request>,
                            typename detail::annotation_of<Key>::type>();
  static constexpr request_descriptor descriptor{kind, nodes.view(),
                                                 Key::descriptor};
};

} // namespace dingo
