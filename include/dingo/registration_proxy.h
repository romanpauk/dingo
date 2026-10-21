//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The proxy a runtime registration returns: it registers runtime bindings
// local to the registration, which its object resolves its dependencies from
// before the container's, and returns the proxy of each in turn.

#include <dingo/core/backend/local.h>
#include <dingo/registration.h>

#include <memory>
#include <utility>

namespace dingo::core {

template <typename Locals, typename Declarations> class registration_proxy;

// The registrations of the local bindings of a registration, which follow the
// lookup declarations `Declarations` of the host, a type_list.
template <typename Locals, typename Declarations> class local_registry {
  using statics_type =
      std::remove_reference_t<decltype(std::declval<Locals &>().statics())>;

public:
  // The registry of `locals`, which follow the lookup declarations of
  // `table`, the table of the host.
  local_registry(Locals &locals, const lookup_table &table)
      : locals_(&locals), table_(&table) {}

  // Registers a binding local to the registration, as register_type of a
  // container does. A binding whose key a local binding of a common interface
  // holds is rejected.
  template <typename... Parts, typename... Args>
  auto register_type(Args &&...args) {
    using binding =
        typename registration<Parts...>::template binding_for<Args...>;
    static_assert(
        detail::key_values_complete<Declarations, binding, Args...>());
    const lookup_table &table = *table_;
    auto &added =
        locals_->runtime_or_create(table).template add_unless<binding>(
            [&table](const void *key_value) {
              return statics_type::template holds_key_of_in<binding>(table,
                                                                     key_value);
            },
            std::forward<Args>(args)...);
    return registration_proxy<std::remove_reference_t<decltype(added)>,
                              Declarations>(added, table);
  }

private:
  Locals *locals_;
  const lookup_table *table_;
};

template <typename Locals, typename Declarations> class registration_proxy {
public:
  registration_proxy(Locals &locals, const lookup_table &table)
      : registry_(locals, table) {}

  local_registry<Locals, Declarations> *operator->() { return &registry_; }
  local_registry<Locals, Declarations> &get() { return registry_; }

private:
  local_registry<Locals, Declarations> registry_;
};

} // namespace dingo::core
