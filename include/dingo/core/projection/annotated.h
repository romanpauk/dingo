//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Annotated interfaces and requests: annotated<Interface, Tag> is an interface
// of its own identity whose objects are Interface objects, so bindings of one
// type serve distinct annotated interfaces, and a request annotated<T, Tag>
// asks for T of the interface annotated by Tag.

#include <dingo/core/primitives/shape.h>
#include <dingo/core/projection/key.h>
#include <dingo/core/projection/layer_traits.h>

#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {
// The value of an annotated request. An annotated abstract interface names an
// interface and holds none, so it is complete wherever the interface is.
template <typename Type, bool = std::is_abstract_v<Type>>
class annotated_value {
public:
  annotated_value(Type value) : value_(std::move(value)) {}

  operator Type() && { return std::move(value_); }
  operator Type() const & { return value_; }

private:
  Type value_;
};

template <typename Type> class annotated_value<Type, true> {};
} // namespace detail

template <typename Type, typename Tag>
class annotated : public detail::annotated_value<Type> {
public:
  using detail::annotated_value<Type>::annotated_value;
};

template <typename Type, typename Tag> class annotated<Type &, Tag> {
public:
  annotated(Type &value) : value_(&value) {}

  operator Type &() const { return *value_; }

private:
  Type *value_;
};

template <typename Type> struct annotated_traits {
  using type = Type;
};

template <typename Type, typename Tag>
struct annotated_traits<annotated<Type, Tag>> {
  using type = Type;
};

template <typename Type, typename Tag>
struct annotated_traits<annotated<Type, Tag> &> {
  using type = Type &;
};

template <typename Type, typename Tag>
struct annotated_traits<annotated<Type, Tag> &&> {
  using type = Type &&;
};

template <typename Type, typename Tag>
struct annotated_traits<annotated<Type, Tag> *> {
  using type = Type *;
};

} // namespace dingo

namespace dingo::core {

template <typename Type> struct is_annotated : std::false_type {};

template <typename Type, typename Tag>
struct is_annotated<::dingo::annotated<Type, Tag>> : std::true_type {};

// The type the objects of an interface are: the interface, or the type it
// annotates.
template <typename Interface> struct interface_object {
  using type = Interface;
};

template <typename Interface, typename Tag>
struct interface_object<::dingo::annotated<Interface, Tag>> {
  using type = Interface;
};

template <typename Interface>
using interface_object_t = typename interface_object<Interface>::type;

// The shape of a declared interface: the shape of its objects, whose leaf has
// the identity of the interface.
template <typename Interface> constexpr shape interface_shape() {
  shape value = shape_v<interface_object_t<Interface>>;
  value.nodes[0].type = type_id_v<Interface>;
  return value;
}

template <typename Interface>
inline constexpr shape interface_shape_v = interface_shape<Interface>();

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

} // namespace dingo::core
