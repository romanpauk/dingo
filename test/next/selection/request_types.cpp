//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// What a request type names: the kind of the request, the type it asks for,
// the interface an annotation or a key decorates, the leaf it holds, and
// whether its wrappers are copied; and the annotated and keyed values a
// constructor parameter takes.

#include <dingo/key.h>
#include <dingo/lookup/key.h>
#include <dingo/selection/request.h>
#include <dingo/selection/selection.h>
#include <dingo/type/annotated.h>
#include <dingo/type/rebind_type.h>
#include <dingo/type/wrapper_traits.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <type_traits>

namespace {

using dingo::annotated;
using dingo::is_annotated;
using dingo::is_keyed;
using dingo::keyed;
using dingo::leaf_of_t;
using dingo::request_kind;
using dingo::request_traits;
using dingo::resolved_t;
using dingo::type_id_v;
using dingo::type_tree_v;

struct tag;
struct service {};
struct iface {
  virtual ~iface() = default;
};
struct impl : iface {};

// A user wrapper of the shared kind and one of the unique kind.
struct user_family {};
template <typename Type> struct user_shared {
  std::shared_ptr<Type> held;
};
template <typename Type> struct user_unique {
  user_unique() = default;
  user_unique(user_unique &&) = default;
  std::unique_ptr<Type> held;
};

} // namespace

namespace dingo {
template <typename Type> struct wrapper_traits<user_shared<Type>> {
  using semantics = ::dingo::semantics::shared_owner;
  using family = user_family;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = user_shared<Element>;
  static Type &element(const user_shared<Type> &value) { return *value.held; }
};

template <typename Type> struct wrapper_traits<user_unique<Type>> {
  using semantics = ::dingo::semantics::sole_owner;
  using family = user_family;
  using arguments = type_list<Type>;
  template <typename Element> using rebind = user_unique<Element>;
  static Type &element(const user_unique<Type> &value) { return *value.held; }
};
} // namespace dingo

namespace {

// The kind of a request and the type it asks for.
static_assert(request_traits<int>::kind == request_kind::value);
static_assert(request_traits<int &>::kind == request_kind::lvalue_reference);
static_assert(request_traits<int &&>::kind == request_kind::rvalue_reference);
static_assert(request_traits<int *>::kind == request_kind::value);
static_assert(std::is_same_v<request_traits<int>::value_type, int>);
static_assert(std::is_same_v<request_traits<int &>::value_type, int>);
static_assert(std::is_same_v<request_traits<const int &>::value_type, int>);
static_assert(std::is_same_v<request_traits<int &&>::value_type, int>);
static_assert(std::is_same_v<request_traits<int *>::value_type, int *>);
static_assert(std::is_same_v<request_traits<const int *>::made_type, int *>);
static_assert(std::is_same_v<request_traits<int (&)[3]>::value_type, int[3]>);

// The leaf a request holds, through its wrappers.
static_assert(std::is_same_v<leaf_of_t<int>, int>);
static_assert(std::is_same_v<leaf_of_t<int &>, int>);
static_assert(std::is_same_v<leaf_of_t<int *>, int>);
static_assert(std::is_same_v<leaf_of_t<int[3]>, int>);
static_assert(std::is_same_v<leaf_of_t<int (&)[3]>, int>);
static_assert(std::is_same_v<leaf_of_t<std::shared_ptr<service>>, service>);
static_assert(std::is_same_v<leaf_of_t<std::unique_ptr<service>>, service>);
static_assert(std::is_same_v<leaf_of_t<std::shared_ptr<std::optional<service>>>,
                             service>);
static_assert(std::is_same_v<leaf_of_t<user_shared<service>>, service>);

// An annotated request asks for the type it annotates, of the interface the
// annotation names; the annotation is not a wrapper.
using annotated_request = annotated<service &, tag>;
static_assert(is_annotated<annotated_request>::value);
static_assert(!is_annotated<service &>::value);
static_assert(std::is_same_v<resolved_t<annotated_request>, service &>);
static_assert(std::is_same_v<dingo::annotated_request<annotated_request>::type,
                             service &>);
static_assert(
    std::is_same_v<dingo::annotated_request<annotated_request>::tag, tag>);
static_assert(std::is_same_v<dingo::annotated_traits<annotated_request>::type,
                             service &>);
static_assert(
    std::is_same_v<dingo::annotated_traits<annotated<service, tag> *>::type,
                   service *>);
static_assert(std::is_same_v<leaf_of_t<annotated_request>, annotated_request>);
static_assert(
    request_traits<service &,
                   dingo::annotated_key<tag, dingo::no_key>>::descriptor.value
        .nodes[0]
        .type == type_id_v<annotated<service, tag>>);
static_assert(request_traits<service &>::descriptor.value.nodes[0].type ==
              type_id_v<service>);

// A keyed parameter asks for the type it decorates, by its key; a parameter
// without a key is the type itself.
using keyed_request = dingo::dependency<service &, dingo::key_type<tag>>;
static_assert(is_keyed<keyed_request>::value);
static_assert(std::is_same_v<resolved_t<keyed_request>, service &>);
static_assert(std::is_same_v<keyed_request::type, service &>);
static_assert(std::is_same_v<keyed_request::key, dingo::tag_key<tag>>);
static_assert(std::is_same_v<dingo::dependency<service &>, service &>);
static_assert(std::is_same_v<resolved_t<service &>, service &>);

// Whether a request's wrappers are copied: a sharing handle is, a sole-owner
// handle is not, and an optional is as its element is.
static_assert(type_tree_v<std::shared_ptr<service>>.nodes[0].copyable);
static_assert(!type_tree_v<std::unique_ptr<service>>.nodes[0].copyable);
static_assert(type_tree_v<std::optional<service>>.nodes[0].copyable);
static_assert(
    !type_tree_v<std::optional<std::unique_ptr<service>>>.nodes[0].copyable);
static_assert(type_tree_v<user_shared<service>>.nodes[0].copyable);
static_assert(!type_tree_v<user_unique<service>>.nodes[0].copyable);
static_assert(
    type_tree_v<std::optional<user_shared<service>>>.nodes[0].copyable);
static_assert(
    !type_tree_v<std::optional<user_unique<service>>>.nodes[0].copyable);

// A wrapper rebinds to an interface of its element through its traits.
static_assert(std::is_same_v<dingo::replace_leaf_t<user_shared<impl>, iface>,
                             user_shared<iface>>);
static_assert(std::is_same_v<dingo::rebind_type_t<user_unique<impl>, iface>,
                             user_unique<iface>>);
static_assert(std::is_same_v<dingo::leaf_type_t<user_unique<impl>>, impl>);

// An annotated or keyed value holds a copy of a copyable value and refuses a
// value it cannot copy.
using copyable_annotated = annotated<std::shared_ptr<service>, tag>;
using move_only_annotated = annotated<std::unique_ptr<service>, tag>;
static_assert(std::is_constructible_v<copyable_annotated,
                                      const std::shared_ptr<service> &>);
static_assert(!std::is_constructible_v<move_only_annotated,
                                       const std::unique_ptr<service> &>);
static_assert(
    std::is_constructible_v<move_only_annotated, std::unique_ptr<service> &&>);
using move_only_keyed = keyed<std::unique_ptr<service>, dingo::tag_key<tag>>;
static_assert(!std::is_constructible_v<move_only_keyed,
                                       const std::unique_ptr<service> &>);

TEST(next_request_types, an_annotated_value_converts_to_the_value_it_holds) {
  copyable_annotated value(std::make_shared<service>());
  std::shared_ptr<service> copied = value;
  EXPECT_NE(copied, nullptr);

  move_only_annotated owned(std::make_unique<service>());
  std::unique_ptr<service> moved = std::move(owned);
  EXPECT_NE(moved, nullptr);

  service object;
  annotated<service &, tag> reference(object);
  EXPECT_EQ(&static_cast<service &>(reference), &object);
}

struct copy_only {
  explicit copy_only(int init_value) : value(init_value) {}
  copy_only(const copy_only &) = default;
  copy_only(copy_only &&) = delete;

  int value;
};

static_assert(std::is_copy_constructible_v<copy_only>);
static_assert(!std::is_move_constructible_v<copy_only>);
static_assert(
    std::is_constructible_v<annotated<copy_only, tag>, const copy_only &>);

// An annotated or keyed value copies a value that cannot be moved.
TEST(next_request_types, an_annotated_value_copies_a_copy_only_value) {
  const copy_only value(17);
  annotated<copy_only, tag> request(value);
  EXPECT_EQ(static_cast<copy_only>(request).value, 17);
  EXPECT_EQ(static_cast<copy_only>(std::move(request)).value, 17);
  keyed<copy_only, dingo::tag_key<tag>> keyed_value(value);
  EXPECT_EQ(static_cast<copy_only>(keyed_value).value, 17);
  EXPECT_EQ(static_cast<copy_only>(std::move(keyed_value)).value, 17);
}

} // namespace
