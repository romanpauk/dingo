//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/container.h of the next implementation.

#include <dingo/core/api/collection.h>
#include <dingo/core/api/container_traits.h>    // IWYU pragma: export
#include <dingo/core/api/factory/callable.h>    // IWYU pragma: export
#include <dingo/core/api/factory/constructor.h> // IWYU pragma: export
#include <dingo/core/api/factory/function.h>    // IWYU pragma: export
#include <dingo/core/api/key.h>                 // IWYU pragma: export
#include <dingo/core/api/lookup/lookup.h>
#include <dingo/core/api/registration.h>
#include <dingo/core/api/registration_proxy.h>
#include <dingo/core/backend/runtime_backend.h>
#include <dingo/core/backend/selection.h>
#include <dingo/core/backend/static_backend.h>
#include <dingo/core/base/mechanics.h>
#include <dingo/core/primitives/exceptions.h> // IWYU pragma: export

#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {
template <typename Type> struct is_bindings : std::false_type {};

template <typename... Binds>
struct is_bindings<bindings<Binds...>> : std::true_type {};

template <typename... Rest> struct runtime_parent {
  using type = void;
};

template <typename Allocator, typename Parent>
struct runtime_parent<Allocator, Parent> {
  using type = Parent;
};

template <typename Type, typename = void> struct is_traits : std::false_type {};

template <typename Type>
struct is_traits<Type, std::void_t<typename Type::lookup_definition_type>>
    : std::true_type {};

// The traits and the parent of a mixed container: container<bindings<...>>,
// container<bindings<...>, Traits>, container<bindings<...>, Parent>, or
// container<bindings<...>, Traits, Parent>.
template <typename... Rest> struct mixed_parameters {
  using traits = dynamic_container_traits;
  using parent = void;
};

template <typename Second> struct mixed_parameters<Second> {
  using traits = std::conditional_t<is_traits<Second>::value, Second,
                                    dynamic_container_traits>;
  using parent = std::conditional_t<is_traits<Second>::value, void, Second>;
};

template <typename Traits, typename Parent>
struct mixed_parameters<Traits, Parent> {
  using traits = Traits;
  using parent = Parent;
};

// The operations of a container on its selection: constructing objects and
// collections no binding stores, and registering a binding that stores a
// collection.
template <typename Self> class container_operations {
public:
  // Registers a binding that stores a collection of the bindings of its
  // element type, each added by `append`, or by its collection traits.
  template <typename... Parts, typename... Append>
  void register_type_collection(Append &&...append) {
    static_assert(sizeof...(Append) <= 1,
                  "a collection takes at most one append function");
    if constexpr (sizeof...(Append) == 0) {
      self().template register_type<Parts...>(
          core::collection_factory<core::collection_append>(
              core::collection_append()));
    } else {
      self().template register_type<Parts...>(
          core::collection_factory<std::decay_t<Append>...>(
              std::forward<Append>(append)...));
    }
  }

  // Constructs an object of `Type` that no binding stores, by its detected
  // constructor or by the callable factory given, resolving its dependencies
  // from the container.
  template <typename Type> Type construct() {
    return core::detected_factory().make<Type>(self());
  }

  template <typename Type, typename Factory> Type construct(Factory &&factory) {
    return factory.template make<Type>(self());
  }

  // Constructs a collection of every binding of its element type, by a key
  // and an append function, either of which may be given.
  template <typename Collection, typename... Arguments>
  Collection construct_collection(Arguments &&...arguments) {
    return core::collect_by<Collection>(self(),
                                        std::forward<Arguments>(arguments)...);
  }

private:
  Self &self() { return static_cast<Self &>(*this); }
};
} // namespace detail

// A container of runtime registrations, with the lookup declarations of its
// traits: container<Traits>, container<Traits, Allocator>, or
// container<Traits, Allocator, Parent>, a child of a parent container that a
// request falls back to.
template <typename Traits = dynamic_container_traits, typename... Rest>
class container
    : public core::selection<container<Traits, Rest...>>,
      public detail::container_operations<container<Traits, Rest...>> {
  static_assert(sizeof...(Rest) <= 2,
                "container<Traits, Allocator, Parent> takes at most three "
                "parameters");

public:
  static constexpr bool has_static_source = false;
  static constexpr bool has_runtime_source = true;
  static constexpr bool has_local_static_source = false;
  static constexpr bool has_local_runtime_source = false;
  using parent_type = typename detail::runtime_parent<Rest...>::type;

  container() : backend_(core::lookup_table_v<Traits>) {}

  // A child of `parent`, which outlives it.
  template <bool Child = !std::is_void_v<parent_type>,
            typename = std::enable_if_t<Child>>
  explicit container(std::conditional_t<Child, parent_type, container> *parent)
      : backend_(core::lookup_table_v<Traits>), parent_(parent) {}

  // Registers a binding declared by scope<>, storage<>, interfaces<>, a key,
  // and a factory. An external binding takes its instance as an argument, a
  // binding of a runtime key its key_value, and a binding of a callable
  // factory the factory.
  // Returns the proxy that registers bindings local to the registration.
  template <typename... Parts, typename... Args>
  auto register_type(Args &&...args) {
    using binding =
        typename core::registration<Parts...>::template binding_for<Args...>;
    auto &locals = backend_.template add<binding>(std::forward<Args>(args)...);
    return core::registration_proxy<std::remove_reference_t<decltype(locals)>>(
        locals, core::lookup_table_v<Traits>);
  }

  // The sources of the selection of the container, which resolves the
  // dependencies of their bindings, and keeps the objects requests construct.
  auto &container_resolver() { return *this; }
  core::retained_objects &retained() { return retained_; }
  core::runtime_backend<container> &runtime_source() { return backend_; }
  parent_type *parent() { return parent_; }
  core::publication_log &publications() { return backend_.publications(); }

private:
  core::retained_objects retained_;
  core::runtime_backend<container> backend_;
  parent_type *parent_ = nullptr;
};

// A container of the static bindings of its type and of runtime
// registrations, which share its lookup declarations: a request selects among
// both, and a runtime registration whose key a static binding holds is
// rejected. It takes the parent, when it has one, and the instance of every
// external static binding as constructor arguments, in the order of the
// bindings.
template <typename... Binds, typename... Rest>
class container<bindings<Binds...>, Rest...>
    : public core::selection<container<bindings<Binds...>, Rest...>>,
      public detail::container_operations<
          container<bindings<Binds...>, Rest...>> {
  using parameters = detail::mixed_parameters<Rest...>;
  using lookups = core::lookup_table_of<typename core::lookup_list<
      typename parameters::traits::lookup_definition_type>::type>;

public:
  using static_source_type = core::static_backend<
      lookups, typename core::bind_registration<Binds>::type::binding...>;
  static constexpr bool has_static_source = true;
  static constexpr bool has_runtime_source = true;
  static constexpr bool has_local_static_source = false;
  static constexpr bool has_local_runtime_source = false;
  using parent_type = typename parameters::parent;

  template <typename... Instances, bool Orphan = std::is_void_v<parent_type>,
            typename = std::enable_if_t<Orphan>>
  explicit container(Instances &&...instances)
      : static_(std::forward<Instances>(instances)...),
        runtime_(lookups::value) {}

  // A child of `parent`, which outlives it.
  template <typename... Instances, bool Child = !std::is_void_v<parent_type>,
            typename = std::enable_if_t<Child>>
  explicit container(std::conditional_t<Child, parent_type, container> *parent,
                     Instances &&...instances)
      : static_(std::forward<Instances>(instances)...),
        runtime_(lookups::value), parent_(parent) {}

  // Registers a runtime binding, as container<Traits> does.
  template <typename... Parts, typename... Args>
  auto register_type(Args &&...args) {
    using binding =
        typename core::registration<Parts...>::template binding_for<Args...>;
    auto &locals = runtime_.template add_unless<binding>(
        [](const void *key_value) {
          return static_source_type::template holds_key_of<binding>(key_value);
        },
        std::forward<Args>(args)...);
    return core::registration_proxy<std::remove_reference_t<decltype(locals)>>(
        locals, lookups::value);
  }

  // The sources of the selection of the container, which resolves the
  // dependencies of their bindings, and keeps the objects requests construct.
  auto &container_resolver() { return *this; }
  core::retained_objects &retained() { return retained_; }
  static_source_type &static_source() { return static_; }
  core::runtime_backend<container> &runtime_source() { return runtime_; }
  parent_type *parent() { return parent_; }
  core::publication_log &publications() { return runtime_.publications(); }

private:
  core::retained_objects retained_;
  static_source_type static_;
  core::runtime_backend<container> runtime_;
  parent_type *parent_ = nullptr;
};

} // namespace dingo
