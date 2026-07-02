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
#include <dingo/type/type_map.h>

#include <memory>

template <typename Tag = void>
struct static_container_with_dynamic_rtti_traits {
  template <typename TagT>
  using rebind_t = static_container_with_dynamic_rtti_traits<TagT>;

  using tag_type = Tag;
  using rtti_type = dingo::rtti<dingo::typeid_provider>;
  template <typename Value, typename Allocator>
  using type_map_type = dingo::static_type_map<Value, Tag, Allocator>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = std::tuple<>;
};

template <typename Tag = void> struct static_container_variant_traits {
  template <typename TagT>
  using rebind_t = static_container_variant_traits<TagT>;
  using tag_type = Tag;
  using rtti_type = dingo::rtti<dingo::static_provider>;
  template <typename Value, typename Allocator>
  using type_map_type = dingo::static_type_map<Value, Tag, Allocator>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = std::tuple<>;
};

struct dynamic_container_with_static_rtti_traits {
  template <typename>
  using rebind_t = dynamic_container_with_static_rtti_traits;

  using tag_type = void;
  using rtti_type = dingo::rtti<dingo::static_provider>;
  template <typename Value, typename Allocator>
  using type_map_type = dingo::dynamic_type_map<Value, rtti_type, Allocator>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = std::tuple<>;
};

struct dynamic_container_variant_traits : dingo::dynamic_container_traits {};

#if defined(DINGO_TEST_SINGLE_DYNAMIC_CONTAINER_CONFIGURATION)
using container_types =
    ::testing::Types<dingo::container<dingo::dynamic_container_traits>>;
#else
using container_types = ::testing::Types<
    dingo::container<dingo::static_container_traits<>>,
    dingo::container<dingo::dynamic_container_traits>,
    dingo::container<static_container_with_dynamic_rtti_traits<>>,
    dingo::container<static_container_variant_traits<>>,
    dingo::container<dynamic_container_with_static_rtti_traits>,
    dingo::container<dynamic_container_variant_traits>>;
#endif
