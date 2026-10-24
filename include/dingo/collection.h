//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Collection requests of the public API of the next implementation: the
// arguments of construct_collection, and the factory of a binding that stores
// a collection.

#include <dingo/diagnostics/compile_diagnostics.gen.h>
#include <dingo/lookup/key.h>
#include <dingo/type/collection_traits.h>

#include <type_traits>
#include <utility>

namespace dingo {

// What an argument of construct_collection is: the function that adds an
// element of `Collection`, or a key, which a key_type object, a key_value, or
// a value of the key type names.
template <typename Collection, typename Argument>
inline constexpr bool is_collection_key_v =
    is_key_type<std::decay_t<Argument>>::value ||
    is_key_value<std::decay_t<Argument>>::value ||
    !std::is_invocable_v<
        Argument &, Collection &,
        typename ::dingo::collection_traits<Collection>::resolve_type>;

// Collects the bindings of the element type of `Collection` by the key and the
// append function of the arguments, either of which may be missing.
template <typename Collection, typename Backend, typename... Arguments>
Collection collect_by(Backend &backend, Arguments &&...arguments);

template <typename Collection, typename Backend>
Collection collect_by(Backend &backend) {
  static_assert(require<::dingo::collection_traits<Collection>::is_collection,
                        diag::collection_traits_missing, Collection>());
  return backend.template collect<Collection, no_key>(nullptr,
                                                      collection_append());
}

template <typename Collection, typename Backend, typename Argument>
Collection collect_by(Backend &backend, Argument &&argument) {
  static_assert(require<::dingo::collection_traits<Collection>::is_collection,
                        diag::collection_traits_missing, Collection>());
  if constexpr (!::dingo::collection_traits<Collection>::is_collection) {
    return Collection();
  } else if constexpr (is_collection_key_v<Collection, Argument>) {
    using key = request_key_of<std::decay_t<Argument>>;
    return backend.template collect<Collection, typename key::type>(
        key::value(argument), collection_append());
  } else {
    return backend.template collect<Collection, no_key>(
        nullptr, std::forward<Argument>(argument));
  }
}

// The factory of a binding that stores a collection: it collects the bindings
// of its element type by `Argument`, the key of the collected bindings or the
// function that adds each, as construct_collection takes it.
template <typename Argument> class collection_factory {
public:
  explicit collection_factory(Argument argument)
      : argument_(std::move(argument)) {}

  template <typename Stored, typename Resolver>
  Stored make(Resolver &resolver) {
    return collect_by<Stored>(resolver, argument_);
  }

private:
  Argument argument_;
};

template <typename Type> struct is_collection_factory : std::false_type {};

template <typename Append>
struct is_collection_factory<collection_factory<Append>> : std::true_type {};

} // namespace dingo
