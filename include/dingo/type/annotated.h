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

#include <dingo/type/type_tree.h>
#include <dingo/type/wrapper_traits.h>

#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {
// The value of an annotated request. An annotated abstract interface names an
// interface and holds none, so it is complete wherever the interface is.
template <typename Type, bool = std::is_abstract_v<Type>>
class annotated_value {
public:
  // A value that cannot be moved is copied.
  template <
      typename Value = Type,
      std::enable_if_t<std::is_constructible_v<Value, const Value &>, int> = 0>
  annotated_value(const Type &value) : value_(value) {}

  template <typename Value = Type,
            std::enable_if_t<std::is_constructible_v<Value, Value &&>, int> = 0>
  annotated_value(Type &&value) : value_(std::move(value)) {}

  operator Type() && {
    if constexpr (std::is_move_constructible_v<Type>) {
      return std::move(value_);
    } else {
      return value_;
    }
  }
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

namespace dingo {

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

// The type tree of a declared interface: the type tree of its objects, whose
// leaf has the identity of the interface.
template <typename Interface> constexpr type_tree interface_type_tree() {
  type_tree value = type_tree_v<interface_object_t<Interface>>;
  value.nodes[0].type = type_id_v<Interface>;
  return value;
}

template <typename Interface>
inline constexpr type_tree interface_type_tree_v =
    interface_type_tree<Interface>();

// The nodes of the type tree of a declared interface, and a view of them.
template <typename Interface>
inline constexpr auto interface_nodes_v =
    exact_nodes<interface_type_tree_v<Interface>.size>(
        interface_type_tree_v<Interface>);

template <typename Interface>
inline constexpr type_tree_view interface_view_v =
    interface_nodes_v<Interface>.view();

} // namespace dingo
