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
#include <dingo/type_cache.h>
#include <dingo/type_map.h>

template <typename Tag = void>
struct static_container_with_dynamic_rtti_traits {
    template <typename TagT>
    using rebind_t = static_container_with_dynamic_rtti_traits<TagT>;

    using tag_type = Tag;
    using rtti_type = dingo::rtti<dingo::typeid_provider>;
    template <typename Value, typename Allocator>
    using type_map_type = dingo::static_type_map<Value, Tag, Allocator>;
    template <typename Value, typename Allocator>
    using type_cache_type = dingo::static_type_cache<Value, Tag, Allocator>;
    using allocator_type = dingo::static_allocator<char, Tag>;
    using index_definition_type = std::tuple<>;
    static constexpr bool cache_enabled = true;
};

template <typename Tag = void>
struct static_container_without_cache : dingo::static_container_traits<> {
    template <typename TagT>
    using rebind_t = static_container_without_cache<TagT>;
    static constexpr bool cache_enabled = false;
    using tag_type = Tag;
};

struct dynamic_container_with_static_rtti_traits {
    template <typename>
    using rebind_t = dynamic_container_with_static_rtti_traits;

    using tag_type = void;
    using rtti_type = dingo::rtti<dingo::static_provider>;
    template <typename Value, typename Allocator>
    using type_map_type = dingo::dynamic_type_map<Value, rtti_type, Allocator>;
    template <typename Value, typename Allocator>
    using type_cache_type =
        dingo::dynamic_type_cache<Value, rtti_type, Allocator>;
    using allocator_type = std::allocator<char>;
    using index_definition_type = std::tuple<>;
    static constexpr bool cache_enabled = true;
};

struct dynamic_container_without_cache : dingo::dynamic_container_traits {
    static constexpr bool cache_enabled = true;
};

using container_types = ::testing::Types<
    dingo::container<dingo::static_container_traits<>>,
    dingo::container<dingo::dynamic_container_traits>,
    dingo::container<static_container_with_dynamic_rtti_traits<>>,
    dingo::container<static_container_without_cache<>>,
    dingo::container<dynamic_container_with_static_rtti_traits>,
    dingo::container<dynamic_container_without_cache>>;
