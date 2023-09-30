#pragma once

#include <dingo/rtti.h>
#include <dingo/type_map.h>

template <typename Tag = void> struct static_container_traits_with_dynamic_rtti {
    template <typename TagT> using rebind_t = static_container_traits_with_dynamic_rtti<TagT>;

    using tag_type = Tag;
    using rtti_type = dingo::dynamic_rtti;
    template <typename Value, typename Allocator>
    using type_factory_map_type = dingo::static_type_map<rtti_type, Tag, Value, Allocator>;
};

struct dynamic_container_traits_with_static_rtti {
    template <typename> using rebind_t = dynamic_container_traits_with_static_rtti;

    using tag_type = void;
    using rtti_type = dingo::static_rtti;
    template <typename Value, typename Allocator>
    using type_factory_map_type = dingo::dynamic_type_map<rtti_type, Value, Allocator>;
};

using container_types = ::testing::Types<dingo::container<dingo::static_container_traits<>>,
                                         dingo::container<dingo::dynamic_container_traits>,
                                         dingo::container<static_container_traits_with_dynamic_rtti<>>,
                                         dingo::container<dynamic_container_traits_with_static_rtti>>;
