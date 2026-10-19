//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/none.h>
#include <dingo/base/type_list.h>
#include <dingo/key/key.h>
#include <dingo/registration/options.h>

#include <type_traits>

namespace dingo {
namespace detail {
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

template <typename Current, typename Expected, typename Candidate>
using registration_arg_t = std::conditional_t<
    !std::is_same_v<Current, Expected>, Current,
    std::conditional_t<registration_arg_matches<Expected, Candidate>::value,
                       Candidate, Current>>;

template <typename ScopeType, typename StorageType, typename FactoryType,
          typename InterfaceType, typename KeyType, typename ConversionsType,
          typename DependenciesType, typename BindingsType>
struct registration_args {
  using scope_type = ScopeType;
  using storage_type = StorageType;
  using factory_type = FactoryType;
  using interface_type = InterfaceType;
  using key_type = KeyType;
  using conversions_type = ConversionsType;
  using dependencies_type = DependenciesType;
  using bindings_type = BindingsType;
};

template <typename ParsedArgs, typename... Args> struct parse_registration_args;

template <typename ParsedArgs> struct parse_registration_args<ParsedArgs> {
  using type = ParsedArgs;
};

template <typename ScopeType, typename StorageType, typename FactoryType,
          typename InterfaceType, typename KeyType, typename ConversionsType,
          typename DependenciesType, typename BindingsType, typename Head,
          typename... Tail>
struct parse_registration_args<
    registration_args<ScopeType, StorageType, FactoryType, InterfaceType,
                      KeyType, ConversionsType, DependenciesType, BindingsType>,
    Head, Tail...> {
private:
  using parsed_head = registration_args<
      registration_arg_t<ScopeType, ::dingo::scope<void>, Head>,
      registration_arg_t<StorageType, ::dingo::storage<void>, Head>,
      registration_arg_t<FactoryType, ::dingo::factory<void>, Head>,
      registration_arg_t<InterfaceType, ::dingo::interfaces<void>, Head>,
      registration_arg_t<KeyType, ::dingo::key_type<::dingo::none_t>, Head>,
      registration_arg_t<ConversionsType, ::dingo::conversions<void>, Head>,
      registration_arg_t<DependenciesType, ::dingo::dependencies<void>, Head>,
      registration_arg_t<BindingsType, ::dingo::bindings<void>, Head>>;

public:
  using type = typename parse_registration_args<parsed_head, Tail...>::type;
};

template <typename... Args>
using parse_registration_args_t = typename parse_registration_args<
    registration_args<::dingo::scope<void>, ::dingo::storage<void>,
                      ::dingo::factory<void>, ::dingo::interfaces<void>,
                      ::dingo::key_type<::dingo::none_t>,
                      ::dingo::conversions<void>, ::dingo::dependencies<void>,
                      ::dingo::bindings<void>>,
    Args...>::type;

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

template <typename T, typename...> struct get_type;
template <typename T> struct get_type<T, type_list<>> {
  using type = T;
};

template <typename T, typename Head, typename... Tail>
struct get_type<T, type_list<Head, Tail...>> {
  using type = std::conditional_t<
      std::is_same_v<T, typename Head::template rebind_t<void>> &&
          !std::is_same_v<Head, T>, /* &&... is needed for interface deduction
                                     */
      Head, typename get_type<T, type_list<Tail...>>::type>;
};

template <typename T, typename... Args>
using get_type_t = typename get_type<T, Args...>::type;
} // namespace detail
} // namespace dingo
