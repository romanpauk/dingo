//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// #include <dingo/base/config.h>

#include <dingo/base/none.h>
#include <dingo/base/type_list.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/traits.h>
#include <dingo/key/key.h>
#include <dingo/key/selected.h>
#include <dingo/registration/args.h>
#include <dingo/registration/deduction.h>
#include <dingo/registration/options.h>
#include <dingo/storage/storage.h>
#include <dingo/type/rebind.h>

#include <type_traits>

namespace dingo {
namespace detail {
template <typename ParsedArgs>
using registration_scope_t = typename ParsedArgs::scope_type;

template <typename ParsedArgs>
using registration_storage_t = std::conditional_t<
    !std::is_same_v<typename ParsedArgs::storage_type, ::dingo::storage<void>>,
    typename ParsedArgs::storage_type,
    ::dingo::storage<typename ParsedArgs::factory_type::type>>;

template <typename ParsedArgs,
          typename Dependencies = typename ParsedArgs::dependencies_type>
struct registration_constructor_default {
  using type = ::dingo::constructor<
      leaf_type_t<typename registration_storage_t<ParsedArgs>::type>>;
};

template <typename ParsedArgs>
struct registration_constructor_default<ParsedArgs,
                                        ::dingo::dependencies<void>> {
  using type = ::dingo::constructor<
      leaf_type_t<typename registration_storage_t<ParsedArgs>::type>>;
};

template <typename ParsedArgs, typename... Args>
struct registration_constructor_default<ParsedArgs,
                                        ::dingo::dependencies<Args...>> {
  using type = ::dingo::constructor<
      leaf_type_t<typename registration_storage_t<ParsedArgs>::type>(Args...)>;
};

template <typename ParsedArgs>
using registration_factory_default_t =
    ::dingo::factory<typename registration_constructor_default<
        ParsedArgs, typename ParsedArgs::dependencies_type>::type>;

template <typename ParsedArgs>
using registration_factory_t = std::conditional_t<
    !std::is_same_v<typename ParsedArgs::factory_type, ::dingo::factory<void>>,
    typename ParsedArgs::factory_type,
    registration_factory_default_t<ParsedArgs>>;

template <typename ParsedArgs>
using registration_interface_from_storage_t = typename deduced_interface_type<
    typename registration_storage_t<ParsedArgs>::type,
    registration_scope_t<ParsedArgs>>::type;

template <typename ParsedArgs>
using registration_interface_from_factory_t = ::dingo::interfaces<
    leaf_type_t<typename registration_factory_t<ParsedArgs>::type>>;

template <typename ParsedArgs> struct selected_registration_interface {
  using type = ParsedArgs;
};

template <typename T, typename Tag>
struct selected_registration_interface<
    ::dingo::detail::selected<T, ::dingo::detail::type_selector<Tag>>> {
  using type = T;
};

template <typename T> struct selected_registration_key {
  using type = ::dingo::key_type<::dingo::none_t>;
};

template <typename T, typename Tag>
struct selected_registration_key<
    ::dingo::detail::selected<T, ::dingo::detail::type_selector<Tag>>> {
  using type = ::dingo::key_type<Tag>;
};

template <typename Left, typename Right> struct merge_selected_registration_key;

template <>
struct merge_selected_registration_key<::dingo::key_type<::dingo::none_t>,
                                       ::dingo::key_type<::dingo::none_t>> {
  using type = ::dingo::key_type<::dingo::none_t>;
};

template <typename Right>
struct merge_selected_registration_key<::dingo::key_type<::dingo::none_t>,
                                       Right> {
  using type = Right;
};

template <typename Left>
struct merge_selected_registration_key<Left,
                                       ::dingo::key_type<::dingo::none_t>> {
  using type = Left;
};

template <typename Key> struct merge_selected_registration_key<Key, Key> {
  using type = Key;
};

template <typename Left, typename Right>
struct merge_selected_registration_key {
  static_assert(
      std::is_same_v<Left, Right>,
      "selected registration interfaces must use one compile-time selector");
  using type = void;
};

template <typename... Interfaces>
struct normalize_selected_registration_interfaces {
  using type = ::dingo::interfaces<
      typename selected_registration_interface<Interfaces>::type...>;
};

template <typename... Interfaces>
struct selected_registration_key_from_interfaces;

template <> struct selected_registration_key_from_interfaces<> {
  using type = ::dingo::key_type<::dingo::none_t>;
};

template <typename Head, typename... Tail>
struct selected_registration_key_from_interfaces<Head, Tail...> {
  using head_key = typename selected_registration_key<Head>::type;
  using tail_key =
      typename selected_registration_key_from_interfaces<Tail...>::type;
  using type =
      typename merge_selected_registration_key<head_key, tail_key>::type;
};

template <typename Interfaces>
struct normalize_selected_registration_interface_arg {
  using type = Interfaces;
  using key_type = ::dingo::key_type<::dingo::none_t>;
};

template <typename... Interfaces>
struct normalize_selected_registration_interface_arg<
    ::dingo::interfaces<Interfaces...>> {
  using type =
      typename normalize_selected_registration_interfaces<Interfaces...>::type;
  using key_type =
      typename selected_registration_key_from_interfaces<Interfaces...>::type;
};

template <typename ParsedArgs>
using registration_explicit_interface_t =
    typename normalize_selected_registration_interface_arg<
        typename ParsedArgs::interface_type>::type;

template <typename ParsedArgs>
using registration_selected_key_t =
    typename normalize_selected_registration_interface_arg<
        typename ParsedArgs::interface_type>::key_type;

template <typename ParsedArgs>
using registration_interface_t = std::conditional_t<
    !std::is_same_v<typename ParsedArgs::interface_type,
                    ::dingo::interfaces<void>>,
    registration_explicit_interface_t<ParsedArgs>,
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

template <typename T> struct normalize_dependencies {
  using type = ::dingo::dependencies<T>;
};

template <typename... Args> struct normalize_dependencies<type_list<Args...>> {
  using type = ::dingo::dependencies<Args...>;
};

template <typename T>
using normalize_dependencies_t = typename normalize_dependencies<T>::type;

template <typename ParsedArgs>
using registration_dependencies_t = std::conditional_t<
    !std::is_same_v<typename ParsedArgs::dependencies_type,
                    ::dingo::dependencies<void>>,
    typename ParsedArgs::dependencies_type,
    std::conditional_t<
        !std::is_same_v<
            typename ::dingo::factory_traits<typename registration_factory_t<
                ParsedArgs>::type>::dependencies,
            void>,
        normalize_dependencies_t<typename ::dingo::factory_traits<
            typename registration_factory_t<ParsedArgs>::type>::dependencies>,
        ::dingo::dependencies<void>>>;

template <typename ParsedArgs>
using registration_key_t =
    std::conditional_t<!std::is_same_v<typename ParsedArgs::key_type,
                                       ::dingo::key_type<::dingo::none_t>>,
                       typename ParsedArgs::key_type,
                       registration_selected_key_t<ParsedArgs>>;

template <typename Bindings> struct is_empty_bindings : std::false_type {};

template <> struct is_empty_bindings<::dingo::bindings<>> : std::true_type {};

template <>
struct is_empty_bindings<::dingo::bindings<static_bindings<>>>
    : std::true_type {};

template <typename Bindings>
using canonical_bindings_t =
    std::conditional_t<is_empty_bindings<Bindings>::value,
                       ::dingo::bindings<void>, Bindings>;

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
  static_assert((detail::is_supported_registration_arg_v<Args> && ...),
                "type_registration expects "
                "scope/storage/factory/interfaces/key/conversions/"
                "dependencies/bindings wrappers");
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

  using key_type = detail::registration_key_t<parsed_args>;

  // Conversions are deduced from Storage and Scope
  using conversions_type = detail::registration_conversions_t<parsed_args>;
  static_assert(!std::is_same_v<conversions_type, conversions<void>>,
                "failed to deduce a conversions type");

  using dependencies_type = detail::registration_dependencies_t<parsed_args>;
  using bindings_type =
      detail::canonical_bindings_t<typename parsed_args::bindings_type>;
};

} // namespace dingo
