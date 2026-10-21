//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// A long chain of dependencies resolves at the default template depth of the
// compiler, in a static and in a runtime container.

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <utility>

namespace {

using dingo::scope;
using dingo::storage;

// Every link depends on the previous one, so resolving the last link nests the
// construction of all of them. A compile-time container instantiates the
// resolution of a link from the resolution of the link that depends on it, so
// the chain is longer than the default template depth allows when a link
// costs more than sixteen levels in Clang (1024 / 64) or more than eighteen
// levels in GCC (900 / 48). A link costs about fourteen levels in Clang and
// about seventeen in GCC.
#if defined(__clang__)
constexpr std::size_t chain_length = 64;
#elif defined(__GNUC__)
constexpr std::size_t chain_length = 48;
#else
constexpr std::size_t chain_length = 16;
#endif

template <std::size_t Index> struct chain_link {
  chain_link(chain_link<Index - 1> &) {}
};

template <> struct chain_link<0> {
  chain_link() {}
};

using last_link = chain_link<chain_length - 1>;

template <typename Indexes> struct chain_bindings;

template <std::size_t... Indexes>
struct chain_bindings<std::index_sequence<Indexes...>> {
  using type = dingo::bindings<
      dingo::bind<scope<dingo::shared>, storage<chain_link<Indexes>>>...>;
};

template <typename Container, std::size_t... Indexes>
void register_chain(Container &container, std::index_sequence<Indexes...>) {
  (container.template register_type<scope<dingo::shared>,
                                    storage<chain_link<Indexes>>>(),
   ...);
}

TEST(next_dependency_chain, resolves_a_static_chain) {
  dingo::container<chain_bindings<std::make_index_sequence<chain_length>>::type>
      container;
  last_link &link = container.resolve<last_link &>();
  ASSERT_EQ(&link, &container.resolve<last_link &>());
}

TEST(next_dependency_chain, resolves_a_runtime_chain) {
  dingo::container<> container;
  register_chain(container, std::make_index_sequence<chain_length>{});
  last_link &link = container.resolve<last_link &>();
  ASSERT_EQ(&link, &container.resolve<last_link &>());
}

} // namespace
