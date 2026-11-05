//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// #include <dingo/base/config.h>

#include <dingo/base/type_list.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/factory_traits.h>
#include <dingo/storage/storage.h>
#include <dingo/type/key.h>
#include <dingo/type/rebind_type.h>

#include <type_traits>
#include <utility>

namespace dingo {
template <typename... Registrations> struct static_registry;
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

template <typename... Args> struct dependencies {
  using type = type_list<Args...>;
  template <typename U> using rebind_t = dependencies<U>;
};

template <typename... Args> struct bindings {
  using type = static_registry<Args...>;
  template <typename U> using rebind_t = bindings<U>;
};

template <> struct bindings<void> {
  using type = void;
  template <typename U> using rebind_t = bindings<U>;
};

template <typename... Args> struct bindings<static_registry<Args...>> {
  using type = static_registry<Args...>;
  template <typename U> using rebind_t = bindings<U>;
};

template <typename T> struct static_bindings_source;

template <typename... Args> struct static_bindings_source<bindings<Args...>> {
  using type = typename bindings<Args...>::type;
};

template <typename... Args>
struct static_bindings_source<static_registry<Args...>> {
  using type = static_registry<Args...>;
};

template <typename T>
using static_bindings_source_t = typename static_bindings_source<T>::type;

template <> struct dependencies<void> {
  using type = void;
  template <typename U> using rebind_t = dependencies<U>;
};

template <typename... Args> struct dependencies<type_list<Args...>> {
  using type = type_list<Args...>;
  template <typename U> using rebind_t = dependencies<U>;
};

namespace detail {
template <typename Type, typename = void> struct deduced_interface_types {
  using type = type_list<remove_cvref_t<Type>>;
};

template <typename Type>
struct deduced_interface_types<
    Type, std::enable_if_t<type_traits<remove_cvref_t<Type>>::enabled &&
                           is_value_borrowable_v<remove_cvref_t<Type>>>> {
private:
  using value_type = remove_cvref_t<Type>;

public:
  using type =
      type_list_cat_t<type_list<value_type>,
                      typename deduced_interface_types<
                          typename type_traits<value_type>::value_type>::type>;
};

template <typename Type>
struct deduced_interface_types<
    Type, std::enable_if_t<is_alternative_type_v<remove_cvref_t<Type>>>> {
private:
  using value_type = remove_cvref_t<Type>;

  template <typename Alternatives> struct alternatives_interface_types;

  template <typename... Alternatives>
  struct alternatives_interface_types<type_list<Alternatives...>> {
    using type = type_list_cat_t<
        typename deduced_interface_types<Alternatives>::type...>;
  };

public:
  using type = type_list_unique_t<
      type_list_cat_t<type_list<value_type>,
                      typename alternatives_interface_types<
                          alternative_type_alternatives_t<value_type>>::type>>;
};

template <typename Expected, typename Candidate, typename = void>
struct registration_arg_matches : std::false_type {};

template <typename T> struct is_registration_key_arg : std::false_type {};

template <typename T, auto... Values>
struct is_registration_key_arg<::dingo::key_type<T, Values...>>
    : std::bool_constant<!std::is_void_v<T>> {};

template <typename Expected, typename Candidate>
struct registration_arg_matches<
    Expected, Candidate,
    std::enable_if_t<!is_registration_key_arg<Candidate>::value,
                     std::void_t<typename Candidate::template rebind_t<void>>>>
    : std::bool_constant<
          std::is_same_v<Expected,
                         typename Candidate::template rebind_t<void>> &&
          !std::is_same_v<Candidate, Expected>> {};

template <typename Candidate>
struct registration_arg_matches<
    ::dingo::key_type<::dingo::none_t>, Candidate,
    std::enable_if_t<is_registration_key_arg<Candidate>::value>>
    : std::true_type {};

// The first argument that fills the role of Expected, or Expected itself.
template <typename Expected, typename... Args> struct registration_arg {
  using type = Expected;
};

template <typename Expected, typename Head, typename... Tail>
struct registration_arg<Expected, Head, Tail...> {
  using type = typename std::conditional_t<
      registration_arg_matches<Expected, Head>::value,
      std::enable_if<true, Head>, registration_arg<Expected, Tail...>>::type;
};

template <typename Expected, typename... Args>
using registration_arg_t = typename registration_arg<Expected, Args...>::type;

template <typename... Args> struct registration_args {
  using scope_type = registration_arg_t<::dingo::scope<void>, Args...>;
  using storage_type = registration_arg_t<::dingo::storage<void>, Args...>;
  using factory_type = registration_arg_t<::dingo::factory<void>, Args...>;
  using interface_type = registration_arg_t<::dingo::interfaces<void>, Args...>;
  using key_type =
      registration_arg_t<::dingo::key_type<::dingo::none_t>, Args...>;
  using conversions_type =
      registration_arg_t<::dingo::conversions<void>, Args...>;
  using dependencies_type =
      registration_arg_t<::dingo::dependencies<void>, Args...>;
  using bindings_type = registration_arg_t<::dingo::bindings<void>, Args...>;
};

template <typename T>
inline constexpr bool is_supported_registration_arg_v =
    registration_arg_matches<::dingo::scope<void>, T>::value ||
    registration_arg_matches<::dingo::storage<void>, T>::value ||
    registration_arg_matches<::dingo::factory<void>, T>::value ||
    registration_arg_matches<::dingo::interfaces<void>, T>::value ||
    registration_arg_matches<::dingo::key_type<::dingo::none_t>, T>::value ||
    registration_arg_matches<::dingo::conversions<void>, T>::value ||
    registration_arg_matches<::dingo::dependencies<void>, T>::value ||
    registration_arg_matches<::dingo::bindings<void>, T>::value;

template <typename ParsedArgs>
using registration_scope_t = typename ParsedArgs::scope_type;

template <typename ParsedArgs>
using registration_storage_t = std::conditional_t<
    !std::is_same_v<typename ParsedArgs::storage_type, ::dingo::storage<void>>,
    typename ParsedArgs::storage_type,
    ::dingo::storage<typename ParsedArgs::factory_type::type>>;

template <typename ParsedArgs,
          typename Dependencies = typename ParsedArgs::dependencies_type>
struct registration_constructor_default;

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
using registration_factory_default_t = ::dingo::factory<
    typename registration_constructor_default<ParsedArgs>::type>;

template <typename ParsedArgs>
using registration_factory_t = std::conditional_t<
    !std::is_same_v<typename ParsedArgs::factory_type, ::dingo::factory<void>>,
    typename ParsedArgs::factory_type,
    registration_factory_default_t<ParsedArgs>>;

template <typename StorageType, typename ScopeType, typename = void>
struct deduced_interface_type {
  using type = ::dingo::interfaces<std::remove_cv_t<leaf_type_t<StorageType>>>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::enable_if_t<is_alternative_type_v<remove_cvref_t<StorageType>>>> {
  using type =
      ::dingo::interfaces<typename ::dingo::detail::deduced_interface_types<
          remove_cvref_t<StorageType>>::type>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::enable_if_t<
        !scope_consumes_v<typename ScopeType::type> &&
        type_traits<remove_cvref_t<StorageType>>::enabled &&
        is_value_borrowable_v<remove_cvref_t<StorageType>> &&
        is_alternative_type_v<std::remove_cv_t<leaf_type_t<StorageType>>>>> {
  using type =
      ::dingo::interfaces<typename ::dingo::detail::deduced_interface_types<
          std::remove_cv_t<leaf_type_t<StorageType>>>::type>;
};

template <typename StorageType, typename ScopeType>
struct deduced_interface_type<
    StorageType, ScopeType,
    std::enable_if_t<
        !scope_consumes_v<typename ScopeType::type> &&
        type_traits<StorageType>::enabled && !std::is_pointer_v<StorageType> &&
        std::is_array_v<typename type_traits<StorageType>::value_type>>> {
  using type =
      ::dingo::interfaces<typename type_traits<StorageType>::value_type,
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

// The selector key of the selected interfaces; they must agree on one.
template <typename... Keys> struct merge_selected_registration_key {
  using type = ::dingo::key_type<::dingo::none_t>;
};

template <typename Head, typename... Tail>
struct merge_selected_registration_key<Head, Tail...> {
private:
  using none = ::dingo::key_type<::dingo::none_t>;
  using tail = typename merge_selected_registration_key<Tail...>::type;

public:
  static_assert(
      std::is_same_v<Head, none> || std::is_same_v<tail, none> ||
          std::is_same_v<Head, tail>,
      "selected registration interfaces must use one compile-time selector");
  using type = std::conditional_t<std::is_same_v<Head, none>, tail, Head>;
};

template <typename Interfaces>
struct normalize_selected_registration_interface_arg {
  using type = Interfaces;
  using key_type = ::dingo::key_type<::dingo::none_t>;
};

template <typename... Interfaces>
struct normalize_selected_registration_interface_arg<
    ::dingo::interfaces<Interfaces...>> {
  using type = ::dingo::interfaces<
      typename selected_registration_interface<Interfaces>::type...>;
  using key_type = typename merge_selected_registration_key<
      typename selected_registration_key<Interfaces>::type...>::type;
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
struct is_empty_bindings<::dingo::bindings<static_registry<>>>
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
  // The explicit registration wrappers, from which the defaults are derived.
  using parsed_args = detail::registration_args<Args...>;

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
