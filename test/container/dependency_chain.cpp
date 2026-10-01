//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <utility>

using namespace dingo;

namespace {

// Every link depends on the previous one, so resolving the last link nests the
// construction of all of them. A compile-time container instantiates the
// resolution of a link from the resolution of the link that depends on it, and
// the chain is longer than the default template depth allows when a link costs
// more than ten levels in Clang or more than eighteen levels in GCC.
#if defined(__clang__)
constexpr std::size_t chain_length = 100;
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
  using type = bindings<bind<scope<shared>, storage<chain_link<Indexes>>>...>;
};

template <typename Container, std::size_t... Indexes>
void register_chain(Container &container, std::index_sequence<Indexes...>) {
  (container
       .template register_type<scope<shared>, storage<chain_link<Indexes>>>(),
   ...);
}

} // namespace

TEST(dependency_chain_test, static_chain) {
  container<chain_bindings<std::make_index_sequence<chain_length>>::type>
      container;
  last_link &link = container.resolve<last_link &>();
  ASSERT_EQ(&link, &container.resolve<last_link &>());
}

TEST(dependency_chain_test, runtime_chain) {
  container<> container;
  register_chain(container, std::make_index_sequence<chain_length>{});
  last_link &link = container.resolve<last_link &>();
  ASSERT_EQ(&link, &container.resolve<last_link &>());
}
