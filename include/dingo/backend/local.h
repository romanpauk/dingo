//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The local bindings of a registration: static bindings it declares with
// bindings<...>, and runtime bindings registered through the proxy its
// registration returns. The object of the registration resolves its
// dependencies through a view whose local sources are those bindings, whose
// container sources are the host's own bindings, and whose parent is the
// host's parent, so the local bindings shadow the host's.

#include <dingo/backend/selection.h>
#include <dingo/base/mechanics.h>
#include <dingo/conversion/lookup.h>
#include <dingo/primitives/retained.h>
#include <dingo/primitives/slot.h>

#include <cstddef>
#include <memory>
#include <utility>

namespace dingo::core {

template <typename LookupTable, typename... Bindings> class static_backend;
template <typename Resolver, typename Allocator = std::allocator<char>>
class runtime_backend;

namespace detail {
template <typename Locals> struct local_static_backend;

template <typename... Bindings>
struct local_static_backend<type_list<Bindings...>> {
  using type = static_backend<lookup_table_of<type_list<>>, Bindings...>;
};

template <typename Locals> struct has_locals : std::true_type {};

template <> struct has_locals<type_list<>> : std::false_type {};
} // namespace detail

// The static bindings a registration declares locally.
template <typename Locals>
using local_static_backend_t =
    typename detail::local_static_backend<Locals>::type;

// The own bindings of a host as one source of a view: they name what the
// host's local and container sources name, and the host resolves them.
template <typename Host> class own_bindings {
public:
  explicit own_bindings(Host &host) : host_(&host) {}

  template <typename Request, typename Key>
  std::size_t count(const void *key_value) const {
    return host_->template own_count<Request, Key>(key_value);
  }

  template <typename Request, typename Key, typename Resolver>
  Request resolve_named(Resolver & /*resolver*/, const void *key_value) {
    return host_->template resolve_own<Request, Key>(key_value);
  }

  template <typename Request, typename Key, typename Resolver,
            typename Otherwise>
  Request select_named(Resolver & /*resolver*/, const void *key_value,
                       Otherwise &&otherwise) {
    const std::size_t count =
        host_->template own_count<Request, Key>(key_value);
    if (count == 0) {
      return otherwise();
    }
    if (count > 1) {
      raise_ambiguous<Request>();
    }
    return host_->template resolve_own<Request, Key>(key_value);
  }

  template <typename Collection, typename Key, typename Resolver,
            typename Append>
  void collect(Resolver & /*resolver*/, Collection &collection,
               const void *key_value, Append &append) {
    host_->template collect_own<Collection, Key>(collection, key_value, append);
  }

private:
  Host *host_;
};

template <typename Host, typename Locals> class local_view;

// The local bindings of a runtime registration in a host: its static
// bindings, and its runtime bindings, which are created by the first
// registration through its proxy and follow the lookup declarations of the
// host.
template <typename Host, typename Locals> class local_bindings {
public:
  using view_type = local_view<Host, Locals>;
  using runtime_type = runtime_backend<view_type>;

  local_static_backend_t<Locals> &statics() { return statics_; }
  runtime_type *runtime() { return runtime_.get(); }

  runtime_type &runtime_or_create(const lookup_table &table) {
    if (!runtime_) {
      runtime_ = std::make_unique<runtime_type>(table);
    }
    return *runtime_;
  }

private:
  local_static_backend_t<Locals> statics_;
  std::unique_ptr<runtime_type> runtime_;
};

// The resolver of the object of a registration with local bindings, valid
// while the object is being constructed.
template <typename Host, typename Locals>
class local_view : public selection<local_view<Host, Locals>> {
public:
  static constexpr bool has_static_source = false;
  static constexpr bool has_runtime_source = true;
  static constexpr bool has_local_static_source =
      detail::has_locals<Locals>::value;
  static constexpr bool has_local_runtime_source = true;
  using local_static_source_type = local_static_backend_t<Locals>;
  using local_runtime_source_type = runtime_backend<local_view>;
  using parent_type = typename Host::parent_type;
  using lookup_definition_type = typename Host::lookup_definition_type;
  using default_indexing_type = typename Host::default_indexing_type;

  local_view(Host &host, local_static_source_type *statics,
             local_runtime_source_type *runtime)
      : host_(host), statics_(statics), runtime_(runtime) {}

  own_bindings<Host> runtime_source() { return own_bindings<Host>(host_); }
  local_static_source_type &local_static_source() { return *statics_; }
  local_runtime_source_type *local_runtime_source() { return runtime_; }
  parent_type *parent() { return host_.parent(); }

  local_view &container_resolver() { return *this; }
  retained_objects &retained() { return host_.retained(); }
  auto &transactions() { return host_.transactions(); }

private:
  Host &host_;
  local_static_source_type *statics_;
  local_runtime_source_type *runtime_;
};

} // namespace dingo::core
