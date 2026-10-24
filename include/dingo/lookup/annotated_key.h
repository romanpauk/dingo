//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The keys of annotated requests: a request annotated by a tag is looked up by
// the key of the request and the annotation that names the interface, and a
// factory argument annotated by a tag is resolved by such a key.

#include <dingo/lookup/key.h>
#include <dingo/type/annotated.h>

namespace dingo {

// The key of a request annotated by `Tag`: the key `Key` of the request,
// with the annotation that names the interface.
template <typename Tag, typename Key> struct annotated_key : Key {
  using annotation = Tag;
};

template <typename Tag, typename Key>
class key_store<annotated_key<Tag, Key>> : public key_store<Key> {
public:
  using key_store<Key>::key_store;
};

namespace detail {
template <typename Tag, typename Key>
struct key_type_of<annotated_key<Tag, Key>> : key_type_of<Key> {};
} // namespace detail

namespace detail {
// The request and the key an argument of a factory is resolved by, as a
// container resolves it.
template <typename Argument> struct argument_request {
  using type = Argument;
  using key = no_key;
};

template <typename Type, typename Key>
struct argument_request<keyed<Type, Key>> {
  using type = Type;
  using key = Key;
};

template <typename Type, typename Tag>
struct argument_request<::dingo::annotated<Type, Tag>> {
  using type = Type;
  using key = annotated_key<Tag, no_key>;
};
} // namespace detail

} // namespace dingo
