//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/memory/static_allocator.h>
#include <dingo/core/keyed.h>
#include <dingo/registration/type_registration.h>
#include <dingo/resolution/type_cache.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/type/complete_type.h>
#include <dingo/type/type_list.h>
#include <dingo/type/type_map.h>

#include <tuple>
#include <type_traits>

namespace dingo {

struct unique;
struct shared;
struct external;
struct shared_cyclical;
struct dynamic_container_traits;
template <typename Tag> struct static_container_traits;

template <typename... Registrations> struct static_registry;
template <typename StaticSource> class static_container;
template <typename Registry> class runtime_injector;
template <typename ContainerTraits = dynamic_container_traits,
          typename Allocator = typename ContainerTraits::allocator_type,
          typename ParentRegistry = void,
          typename ResolveRoot = void>
class runtime_registry;
template <typename ContainerTraits = dynamic_container_traits,
          typename Allocator = typename ContainerTraits::allocator_type,
          typename ParentContainer = void>
class runtime_container;
template <typename StaticRegistry> class hybrid_container;
template <typename... Params> class container;

struct none_t {};

template <typename T> struct is_none : std::bool_constant<false> {};
template <> struct is_none<none_t> : std::bool_constant<true> {};
template <typename T> static constexpr auto is_none_v = is_none<T>::value;

namespace detail {

template <typename T, bool = is_complete<T>::value>
struct default_auto_constructible : std::false_type {};

template <typename T>
struct default_auto_constructible<T, true>
    : std::bool_constant<std::is_aggregate_v<T>> {};

template <typename T>
struct is_typed_key : std::false_type {};

template <typename T>
struct is_typed_key<key<T>> : std::bool_constant<!std::is_void_v<T>> {};

template <typename T>
inline constexpr bool is_typed_key_v = is_typed_key<std::decay_t<T>>::value;

template <typename Identity, typename Binding>
struct keyed_binding_identity : Binding {
    using Binding::Binding;
};

template <typename ParentContainer>
struct runtime_parent_registry {
    using type = typename ParentContainer::registry_type;
};

template <>
struct runtime_parent_registry<void> {
    using type = void;
};

template <typename ParentContainer>
using runtime_parent_registry_t =
    typename runtime_parent_registry<ParentContainer>::type;

template <typename ParentContainer>
runtime_parent_registry_t<ParentContainer>*
runtime_parent_registry_ptr(ParentContainer* parent) {
    return parent ? &parent->registry() : nullptr;
}

template <typename T, typename = void>
struct is_runtime_container_traits : std::false_type {};

template <typename T>
struct is_runtime_container_traits<
    T, std::void_t<typename T::tag_type, typename T::rtti_type,
                   typename T::allocator_type,
                   typename T::index_definition_type,
                   typename T::template rebind_t<void>>>
    : std::true_type {};

template <typename T>
inline constexpr bool is_runtime_container_traits_v =
    is_runtime_container_traits<T>::value;

template <typename T> struct is_static_registry : std::false_type {};

template <typename... Registrations>
struct is_static_registry<static_registry<Registrations...>> : std::true_type {};

template <typename T>
inline constexpr bool is_static_registry_v = is_static_registry<T>::value;

template <typename T> struct is_bindings_wrapper : std::false_type {};

template <typename... Args>
struct is_bindings_wrapper<::dingo::bindings<Args...>> : std::true_type {};

template <typename T>
inline constexpr bool is_bindings_wrapper_v = is_bindings_wrapper<T>::value;

template <typename T> struct bindings_wrapper_registry;

template <typename... Args>
struct bindings_wrapper_registry<::dingo::bindings<Args...>> {
    using type = typename ::dingo::bindings<Args...>::type;
};

template <typename T>
using bindings_wrapper_registry_t = typename bindings_wrapper_registry<T>::type;

template <typename... Ts>
inline constexpr bool container_dependent_false_v = false;

} // namespace detail

template <typename T>
struct is_auto_constructible : detail::default_auto_constructible<T> {};

struct dynamic_container_traits {
    template <typename> using rebind_t = dynamic_container_traits;

    using tag_type = none_t;
    using rtti_type = rtti<typeid_provider>;
    template <typename Value, typename Allocator>
    using type_map_type = dynamic_type_map<Value, rtti_type, Allocator>;
    template <typename Value, typename Allocator>
    using type_cache_type = dynamic_type_cache<Value, rtti_type, Allocator>;
    using allocator_type = std::allocator<char>;
    using index_definition_type = std::tuple<>;
    static constexpr bool cache_enabled = true;
};

template <typename Tag = void> struct static_container_traits {
    template <typename TagT> using rebind_t = static_container_traits<TagT>;

    using tag_type = Tag;
    using rtti_type = rtti<static_provider>;
    template <typename Value, typename Allocator>
    using type_map_type = static_type_map<Value, Tag, Allocator>;
    template <typename Value, typename Allocator>
    using type_cache_type = static_type_cache<void*, Tag, Allocator>;
    using allocator_type = static_allocator<char, Tag>;
    using index_definition_type = std::tuple<>;
    static constexpr bool cache_enabled = true;
};

template <typename Traits>
static constexpr bool is_tagged_container_v =
    !std::is_same_v<typename Traits::template rebind_t<
                        type_list<typename Traits::tag_type, void>>,
                    Traits>;

} // namespace dingo
