//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// #include <dingo/config.h>

#include <dingo/factory/constructor.h>
#include <dingo/resolution/interface_storage_traits.h>
#include <dingo/type/rebind_type.h>
#include <dingo/storage/storage.h>
#include <dingo/type/type_list.h>

namespace dingo {
struct unique;
struct external;

template <typename T> struct storage {
    using type = T;
    template <typename U> using rebind_t = storage<U>;
};

template <typename T> struct scope {
    using type = T;
    template <typename U> using rebind_t = scope<U>;
};

template <typename... Args> struct interfaces {
    using type = type_list<Args...>;

    template <typename U> using rebind_t = interfaces<U>;
};

template <typename... Args> struct interfaces<type_list<Args...>> {
    using type = type_list<Args...>;

    template <typename U> using rebind_t = interfaces<U>;
};

template <typename T> struct factory {
    using type = T;
    template <typename U> using rebind_t = factory<U>;
};

template <typename T> struct conversions {
    using type = T;
    template <typename U> using rebind_t = conversions<U>;
};

namespace detail {
template <typename Current, typename Expected, typename Candidate>
using registration_arg_t = std::conditional_t<
    !std::is_same_v<Current, Expected>, Current,
    std::conditional_t<
        std::is_same_v<Expected, typename Candidate::template rebind_t<void>> &&
            !std::is_same_v<Candidate, Expected>,
        Candidate, Current>>;

template <typename ScopeType, typename StorageType, typename FactoryType,
          typename InterfaceType, typename ConversionsType>
struct registration_args {
    using scope_type = ScopeType;
    using storage_type = StorageType;
    using factory_type = FactoryType;
    using interface_type = InterfaceType;
    using conversions_type = ConversionsType;
};

template <typename ParsedArgs, typename... Args> struct parse_registration_args;

template <typename ParsedArgs>
struct parse_registration_args<ParsedArgs> {
    using type = ParsedArgs;
};

template <typename ScopeType, typename StorageType, typename FactoryType,
          typename InterfaceType, typename ConversionsType, typename Head,
          typename... Tail>
struct parse_registration_args<
    registration_args<ScopeType, StorageType, FactoryType, InterfaceType,
                      ConversionsType>,
    Head, Tail...> {
  private:
    using parsed_head = registration_args<
        registration_arg_t<ScopeType, ::dingo::scope<void>, Head>,
        registration_arg_t<StorageType, ::dingo::storage<void>, Head>,
        registration_arg_t<FactoryType, ::dingo::factory<void>, Head>,
        registration_arg_t<InterfaceType, ::dingo::interfaces<void>, Head>,
        registration_arg_t<ConversionsType, ::dingo::conversions<void>, Head>>;

  public:
    using type = typename parse_registration_args<parsed_head, Tail...>::type;
};

template <typename... Args>
using parse_registration_args_t = typename parse_registration_args<
    registration_args<::dingo::scope<void>, ::dingo::storage<void>,
                      ::dingo::factory<void>, ::dingo::interfaces<void>,
                      ::dingo::conversions<void>>,
    Args...>::type;

template <typename ParsedArgs>
using registration_scope_t = typename ParsedArgs::scope_type;

template <typename ParsedArgs>
using registration_storage_t = std::conditional_t<
    !std::is_same_v<typename ParsedArgs::storage_type,
                    ::dingo::storage<void>>,
    typename ParsedArgs::storage_type,
    ::dingo::storage<typename ParsedArgs::factory_type::type>>;

template <typename ParsedArgs>
using registration_factory_default_t =
    ::dingo::factory<::dingo::constructor_detection<
        leaf_type_t<typename registration_storage_t<ParsedArgs>::type>>>;

template <typename ParsedArgs>
using registration_factory_t = std::conditional_t<
    !std::is_same_v<typename ParsedArgs::factory_type,
                    ::dingo::factory<void>>,
    typename ParsedArgs::factory_type,
    registration_factory_default_t<ParsedArgs>>;

template <typename StorageType>
using registration_storage_value_t =
    std::remove_cv_t<std::remove_reference_t<StorageType>>;

template <typename StorageType>
using registration_storage_leaf_value_t =
    std::remove_cv_t<leaf_type_t<StorageType>>;

template <typename StorageType, typename ScopeType, typename = void>
struct deduced_interface_type {
    using type = ::dingo::interfaces<leaf_type_t<StorageType>>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::void_t<typename ::dingo::detail::alternative_type_interface_types<
        registration_storage_value_t<StorageType>>::type>> {
    using type = ::dingo::interfaces<
        typename ::dingo::detail::alternative_type_interface_types<
            registration_storage_value_t<StorageType>>::type>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::enable_if_t<!std::is_same_v<typename ScopeType::type, unique> &&
                     type_traits<registration_storage_value_t<
                         StorageType>>::enabled &&
                     type_traits<registration_storage_value_t<
                         StorageType>>::is_value_borrowable &&
                     is_alternative_type_v<
                         registration_storage_leaf_value_t<StorageType>>>> {
    using type = ::dingo::interfaces<
        typename ::dingo::detail::alternative_type_interface_types<
            registration_storage_leaf_value_t<StorageType>>::type>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::enable_if_t<!std::is_same_v<typename ScopeType::type, unique> &&
                     type_traits<StorageType>::enabled &&
                     !std::is_pointer_v<StorageType> &&
                     std::is_array_v<typename type_traits<StorageType>::value_type>>> {
    using type = ::dingo::interfaces<typename type_traits<StorageType>::value_type,
                                     leaf_type_t<StorageType>>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::enable_if_t<std::is_array_v<StorageType> &&
                     (std::rank_v<StorageType> > 1)>> {
    using type = ::dingo::interfaces<std::remove_extent_t<StorageType>,
                                     StorageType, leaf_type_t<StorageType>>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::enable_if_t<std::is_array_v<StorageType> &&
                     (std::rank_v<StorageType> == 1)>> {
    using type = ::dingo::interfaces<StorageType, leaf_type_t<StorageType>>;
};

template <typename ParsedArgs>
using registration_interface_from_storage_t = typename deduced_interface_type<
    typename registration_storage_t<ParsedArgs>::type,
    registration_scope_t<ParsedArgs>>::type;

template <typename ParsedArgs>
using registration_interface_from_factory_t = ::dingo::interfaces<leaf_type_t<
    typename registration_factory_t<ParsedArgs>::type>>;

template <typename ParsedArgs>
using registration_interface_t = std::conditional_t<
    !std::is_same_v<typename ParsedArgs::interface_type,
                    ::dingo::interfaces<void>>,
    typename ParsedArgs::interface_type,
    std::conditional_t<
        !std::is_same_v<registration_interface_from_storage_t<ParsedArgs>,
                        ::dingo::interfaces<void>>,
        registration_interface_from_storage_t<ParsedArgs>,
        registration_interface_from_factory_t<ParsedArgs>>>;

template <typename ParsedArgs>
using registration_conversions_t = std::conditional_t<
    !std::is_same_v<typename ParsedArgs::conversions_type,
                    ::dingo::conversions<void>>,
    typename ParsedArgs::conversions_type,
    ::dingo::conversions<detail::conversions<
        typename registration_scope_t<ParsedArgs>::type,
        typename registration_storage_t<ParsedArgs>::type, runtime_type>>>;

template <typename Registration>
using registration_interface_types_t =
    typename Registration::interface_type::type;

template <typename Registration>
using registration_registered_storage_type_t =
    typename Registration::storage_type::type;

template <typename Registration>
static constexpr bool registration_stores_single_interface_v =
    type_list_size_v<registration_interface_types_t<Registration>> == 1;

template <typename Registration>
using registration_stored_interface_t =
    type_list_head_t<registration_interface_types_t<Registration>>;

template <typename Registration>
static constexpr bool registration_should_store_interface_v =
    registration_stores_single_interface_v<Registration> &&
    std::has_virtual_destructor_v<registration_stored_interface_t<Registration>> &&
    is_interface_storage_rebindable_v<
        registration_registered_storage_type_t<Registration>,
        registration_stored_interface_t<Registration>>;

template <typename Registration>
using registration_stored_leaf_type_t =
    std::conditional_t<registration_should_store_interface_v<Registration>,
                       registration_stored_interface_t<Registration>,
                       leaf_type_t<registration_registered_storage_type_t<
                           Registration>>>;

template <typename Registration>
using registration_stored_type_t = rebind_leaf_t<
    registration_registered_storage_type_t<Registration>,
    registration_stored_leaf_type_t<Registration>>;

template <typename Registration>
using registration_runtime_storage_t = ::dingo::detail::storage<
    typename Registration::scope_type::type,
    registration_registered_storage_type_t<Registration>,
    registration_stored_type_t<Registration>,
    typename Registration::factory_type::type,
    typename Registration::conversions_type::type>;

template <typename Registration>
static constexpr auto registration_materialization_kind_v =
    registration_stores_single_interface_v<Registration>
        ? ir::registration_materialization_kind::single_interface
        : ir::registration_materialization_kind::shared_factory_data;

template <typename Registration, typename PayloadType>
static constexpr auto registration_payload_kind_v =
    std::is_same_v<PayloadType, void>
        ? ir::registration_payload_kind::default_constructed
        : std::is_same_v<typename Registration::scope_type::type, external>
              ? ir::registration_payload_kind::external_instance
              : ir::registration_payload_kind::factory_payload;

template <typename Registration, typename PayloadType = void,
          typename IndexKeyType = void>
using registration_plan_t = ir::registration<
    registration_interface_types_t<Registration>,
    typename Registration::scope_type::type,
    registration_registered_storage_type_t<Registration>,
    registration_stored_type_t<Registration>,
    registration_runtime_storage_t<Registration>,
    typename Registration::factory_type::type,
    typename Registration::conversions_type::type,
    factory_invocation_ir_t<typename Registration::factory_type::type>,
    IndexKeyType,
    registration_payload_kind_v<Registration, PayloadType>,
    registration_materialization_kind_v<Registration>>;

} // namespace detail

// TODO:
// the default factory is hardcoded

template <typename... Args> struct type_registration {
  private:
    // Parse the explicit registration wrappers once, then derive any defaults
    // from that cached result instead of rescanning Args... for each role.
    using parsed_args = detail::parse_registration_args_t<Args...>;

  public:
    // Scope has to be scpecified as there is no way how to deduce it
    using scope_type = detail::registration_scope_t<parsed_args>;
    static_assert(!std::is_same_v<scope_type, scope<void>>,
                  "failed to deduce a scope type");

    // Storage can be deduced from Factory
    using storage_type = detail::registration_storage_t<parsed_args>;
    static_assert(!std::is_same_v<storage_type, storage<void>>,
                  "failed to deduce a storage type");

    // Factory can be deduced from Storage
    using factory_type = detail::registration_factory_t<parsed_args>;
    static_assert(!std::is_same_v<factory_type, factory<void>>,
                  "failed to deduce a factory type");

    // Interface can be deduced from Storage or Factory
    using interface_type = detail::registration_interface_t<parsed_args>;
    static_assert(!std::is_same_v<interface_type, interfaces<void>>,
                  "failed to deduce an interface type");

    // Conversions are deduced from Storage and Scope
    using conversions_type = detail::registration_conversions_t<parsed_args>;
    static_assert(!std::is_same_v<conversions_type, conversions<void>>,
                  "failed to deduce a conversions type");
};

} // namespace dingo
