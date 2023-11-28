//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>
#include <dingo/rtti.h>
#include <dingo/type_map.h>

template <typename Tag = void>
struct static_container_with_dynamic_rtti_traits {
    template <typename TagT>
    using rebind_t = static_container_with_dynamic_rtti_traits<TagT>;

    using tag_type = Tag;
    using rtti_type = dingo::dynamic_rtti;
    template <typename Value, typename Allocator>
    using type_factory_map_type =
        dingo::static_type_map<rtti_type, Tag, Value, Allocator>;
    using allocator_type = dingo::static_allocator<char, Tag>;
    using index_definition_type = std::tuple<>;
};

struct dynamic_container_with_static_rtti_traits {
    template <typename>
    using rebind_t = dynamic_container_with_static_rtti_traits;

    using tag_type = void;
    using rtti_type = dingo::static_rtti;
    template <typename Value, typename Allocator>
    using type_factory_map_type =
        dingo::dynamic_type_map<rtti_type, Value, Allocator>;
    using allocator_type = std::allocator<char>;
    using index_definition_type = std::tuple<>;
};

using container_types = ::testing::Types<
    dingo::container<dingo::static_container_traits<>>,
    dingo::container<dingo::dynamic_container_traits>,
    dingo::container<static_container_with_dynamic_rtti_traits<>>,
    dingo::container<dynamic_container_with_static_rtti_traits>>;
