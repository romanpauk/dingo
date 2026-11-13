//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/static_container.h>

#include <gtest/gtest.h>

#include <memory>

struct static_container_with_dynamic_rtti_traits {
  using rtti_type = dingo::rtti<dingo::typeid_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = dingo::lookups<>;
};

struct static_container_variant_traits {
  using rtti_type = dingo::rtti<dingo::static_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = dingo::lookups<>;
};

struct dynamic_container_with_static_rtti_traits {
  using rtti_type = dingo::rtti<dingo::static_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = dingo::lookups<>;
};

struct dynamic_container_variant_traits : dingo::runtime_container_traits {};

#if defined(DINGO_TEST_SINGLE_DYNAMIC_CONTAINER_CONFIGURATION)
using container_types =
    ::testing::Types<dingo::container<dingo::runtime_container_traits>>;
#else
using container_types = ::testing::Types<
    dingo::container<dingo::static_container_traits>,
    dingo::container<dingo::runtime_container_traits>,
    dingo::container<static_container_with_dynamic_rtti_traits>,
    dingo::container<static_container_variant_traits>,
    dingo::container<dynamic_container_with_static_rtti_traits>,
    dingo::container<dynamic_container_variant_traits>>;
#endif

// The two containers a test makes of the same bindings, dingo::bind<...>
// each: the runtime container, which registers them as it is made, and the
// static container, which binds them in its type. container<Bindings...> is
// the container, and with<Bindings...>(function) calls function with one.
template <typename Traits = dingo::runtime_container_traits>
struct runtime_kind {
  static constexpr bool is_static = false;

  template <typename... Bindings>
  struct container : dingo::runtime_container<Traits> {
    container() { (add(static_cast<Bindings *>(nullptr)), ...); }

  private:
    template <typename... Args> void add(dingo::bind<Args...> *) {
      this->template register_type<Args...>();
    }
  };

  template <typename... Bindings, typename Function>
  static void with(Function &&function) {
    container<Bindings...> made;
    function(made);
  }
};

struct static_kind {
  static constexpr bool is_static = true;

  template <typename... Bindings>
  using container = dingo::static_container<dingo::bindings<Bindings...>>;

  template <typename... Bindings, typename Function>
  static void with(Function &&function) {
    container<Bindings...> made;
    function(made);
  }
};

using container_kinds = ::testing::Types<runtime_kind<>, static_kind>;
