//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The factory of a stored shape: the leaf is constructed by its detected
// constructor, whose arguments the resolver resolves, and every layer is made
// by its layer traits from an argument that converts to its element. The
// element is made at that conversion, so a layer constructs it in place.

#include <dingo/core/base/mechanics.h>
#include <dingo/core/primitives/constructor.h>
#include <dingo/core/projection/layer_traits.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <new>
#include <type_traits>
#include <utility>

namespace dingo::core {

template <typename Stored, typename Resolver>
Stored make_stored(Resolver &resolver);

namespace detail {
// Converts to the object of `Type`, made by the factory at the conversion.
template <typename Type, typename Resolver> class made_element {
public:
  explicit made_element(Resolver &resolver) : resolver_(resolver) {}

  operator Type() const { return make_stored<Type>(resolver_); }

private:
  Resolver &resolver_;
};
} // namespace detail

template <typename Stored, typename Resolver>
Stored make_stored(Resolver &resolver) {
  using traits = layer_traits<Stored>;
  if constexpr (traits::kind == layer_kind::leaf) {
    return invoke_constructor<Stored>(resolver, [](auto &&...args) {
      return Stored{std::forward<decltype(args)>(args)...};
    });
  } else {
    using element = type_list_at_t<0, typename traits::arguments>;
    return traits::make(detail::made_element<element, Resolver>(resolver));
  }
}

// Constructs the stored shape at `where`; arrays are value-initialized in
// place.
template <typename Stored, typename Resolver>
void construct_stored(void *where, Resolver &resolver) {
  if constexpr (std::is_array_v<Stored>) {
    ::new (where) Stored();
  } else {
    ::new (where) Stored(make_stored<Stored>(resolver));
  }
}

} // namespace dingo::core
