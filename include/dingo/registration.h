//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The registration declarations of the public API and its reduction to a
// binding type of the next implementation.

#include <dingo/base/mechanics.h>
#include <dingo/collection.h>
#include <dingo/conversion/convert.h>
#include <dingo/storage/scopes.gen.h>
#include <dingo/diagnostics/compile_diagnostics.gen.h>
#include <dingo/lookup/key_kinds.gen.h>

#include <type_traits>

namespace dingo {

template <typename Tag> struct scope {
  using type = Tag;
};

template <typename Type> struct storage {
  using type = Type;
};

template <typename... Interfaces> struct interfaces {
  using type = core::type_list<Interfaces...>;
};

// The factory a registration declares: a constructor, a function, or a
// callable object passed at registration.
template <typename Factory> struct factory {
  using type = Factory;
};

template <typename... Parts> struct bind {};

template <typename... Binds> struct bindings {};

// The dependencies of a binding: the argument types of its constructor.
template <typename... Types> struct dependencies {};

} // namespace dingo

namespace dingo::core {

namespace detail {
template <template <typename...> typename Part, typename Type>
struct is_part : std::false_type {};

template <template <typename...> typename Part, typename... Arguments>
struct is_part<Part, Part<Arguments...>> : std::true_type {};

template <template <typename...> typename Part, typename Default,
          typename... Parts>
struct find_part {
  using type = Default;
};

template <template <typename...> typename Part, typename Default,
          typename First, typename... Rest>
struct find_part<Part, Default, First, Rest...> {
  using type =
      std::conditional_t<is_part<Part, First>::value, First,
                         typename find_part<Part, Default, Rest...>::type>;
};

struct missing_part {
  using type = void;
};

template <typename... Parts> struct find_key_part {
  using type = no_key;
};

template <typename First, typename... Rest>
struct find_key_part<First, Rest...> {
  using type =
      typename std::conditional_t<is_key_type<First>::value, key_of<First>,
                                  find_key_part<Rest...>>::type;
};

template <typename Default, typename... Args> struct find_callable_factory {
  using type = Default;
};

template <typename Default, typename First, typename... Rest>
struct find_callable_factory<Default, First, Rest...> {
  using type = typename std::conditional_t<
      is_callable_factory<std::decay_t<First>>::value ||
          is_collection_factory<std::decay_t<First>>::value,
      std::enable_if<true, std::decay_t<First>>,
      find_callable_factory<Default, Rest...>>::type;
};

template <typename Default, typename... Args> struct find_runtime_key {
  using type = Default;
};

template <typename Default, typename First, typename... Rest>
struct find_runtime_key<Default, First, Rest...> {
  using type =
      typename std::conditional_t<is_key_value<std::decay_t<First>>::value,
                                  runtime_key_of<std::decay_t<First>>,
                                  find_runtime_key<Default, Rest...>>::type;
};

// The runtime keys of the key values of a registration of types other than
// `Primary`, each once: the keys of other domains its binding is also found
// by.
template <typename Primary, typename Found, typename... Args>
struct find_key_aliases {
  using type = Found;
};

template <typename Primary, typename... Found, typename First, typename... Rest>
struct find_key_aliases<Primary, type_list<Found...>, First, Rest...> {
  template <typename Key>
  static constexpr bool adds =
      !std::is_same_v<Key, Primary> && (!std::is_same_v<Key, Found> && ...);
  using key = typename runtime_key_of<std::decay_t<First>>::type;
  using type = typename std::conditional_t<
      is_key_value<std::decay_t<First>>::value && adds<key>,
      find_key_aliases<Primary, type_list<Found..., key>, Rest...>,
      find_key_aliases<Primary, type_list<Found...>, Rest...>>::type;
};
} // namespace detail

template <typename... Parts> struct registration;

template <typename Bind> struct bind_registration;

namespace detail {
template <typename Bindings> struct local_bindings_of;

template <typename... Binds>
struct local_bindings_of<::dingo::bindings<Binds...>> {
  using type = type_list<typename bind_registration<Binds>::type::binding...>;
};

template <typename... Types> constexpr bool distinct_types() {
  constexpr type_id ids[] = {type_id_v<Types>..., nullptr};
  for (std::size_t first = 0; first < sizeof...(Types); ++first) {
    for (std::size_t later = first + 1; later < sizeof...(Types); ++later) {
      if (ids[first] == ids[later]) {
        return false;
      }
    }
  }
  return true;
}

// The first variadic wrapper of a stored type tree that names an alternative
// twice, or void.
template <typename Type, typename = void> struct repeated_alternatives {
  using type = void;
};

template <typename... Types> struct first_repeated;

template <> struct first_repeated<> {
  using type = void;
};

template <typename First, typename... Rest>
struct first_repeated<First, Rest...> {
  using type = std::conditional_t<
      std::is_void_v<typename repeated_alternatives<First>::type>,
      typename first_repeated<Rest...>::type,
      typename repeated_alternatives<First>::type>;
};

template <typename List> struct first_repeated_in;

template <typename... Types> struct first_repeated_in<type_list<Types...>> {
  using type = typename first_repeated<Types...>::type;
  static constexpr bool distinct = distinct_types<Types...>();
};

template <typename Type>
struct repeated_alternatives<
    Type, std::enable_if_t<
              is_wrapper_v<std::remove_cv_t<std::remove_reference_t<Type>>>>> {
  using wrapper = std::remove_cv_t<std::remove_reference_t<Type>>;
  using arguments =
      first_repeated_in<typename wrapper_traits<wrapper>::arguments>;
  using type =
      std::conditional_t<has_properties_v<wrapper, property::variadic> &&
                             !arguments::distinct,
                         wrapper, typename arguments::type>;
};

// Whether the arguments of a registration take a key value of the runtime key
// `Key` for `Interface`: one that names no interface, or names it.
template <typename Interface, typename Key, typename... Args>
constexpr bool takes_key_value() {
  return (
      (is_key_value<std::decay_t<Args>>::value &&
       std::is_same_v<typename runtime_key_of<std::decay_t<Args>>::type, Key> &&
       (std::is_void_v<
            typename key_value_interface<std::decay_t<Args>>::type> ||
        std::is_same_v<typename key_value_interface<std::decay_t<Args>>::type,
                       Interface>)) ||
      ...);
}

// Whether `Declaration` declares a lookup of `Interface` by a runtime key.
template <typename Interface, typename Declaration>
constexpr bool declares_runtime_lookup() {
  return Declaration::key_type::descriptor.kind == key_kind::runtime &&
         type_list_index_of<Interface,
                            typename Declaration::interface_list>() !=
             Declaration::interface_list::size;
}

// Reports a registration that takes a key value of `Interface` for one of the
// lookups of it by a runtime key the declarations declare and none for
// another. spec: diagnostics.registration_key_value_missing
template <typename Stored, typename Interface, typename... Declarations>
struct interface_key_values {
  template <typename... Args> static constexpr bool complete() {
    constexpr bool takes_any =
        ((declares_runtime_lookup<Interface, Declarations>() &&
          takes_key_value<Interface, typename Declarations::key_type,
                          Args...>()) ||
         ...);
    if constexpr (takes_any) {
      (missing<Declarations, Args...>(), ...);
    }
    return true;
  }

  template <typename Declaration, typename... Args>
  static constexpr void missing() {
    if constexpr (declares_runtime_lookup<Interface, Declaration>() &&
                  !takes_key_value<Interface, typename Declaration::key_type,
                                   Args...>()) {
      fail<diag::registration_key_value_missing, Stored,
           typename Declaration::key_type::value_type, Interface>();
    }
  }
};

template <typename Stored, typename Interfaces, typename Declarations>
struct key_values_of;

template <typename Stored, typename... Interfaces, typename... Declarations>
struct key_values_of<Stored, type_list<Interfaces...>,
                     type_list<Declarations...>> {
  template <typename... Args> static constexpr bool complete() {
    return (
        interface_key_values<Stored, Interfaces,
                             Declarations...>::template complete<Args...>() &&
        ...);
  }
};

// Whether a registration of `Binding` by `Args` takes a key value for every
// lookup of an interface by a runtime key that `Declarations`, a type_list of
// lookup declarations, declare, when it takes one for any.
template <typename Declarations, typename Binding, typename... Args>
constexpr bool key_values_complete() {
  using interfaces =
      std::conditional_t<Binding::interfaces::size == 0,
                         type_list<leaf_of_t<typename Binding::stored_type>>,
                         typename Binding::interfaces>;
  return key_values_of<typename Binding::stored_type, interfaces,
                       Declarations>::template complete<Args...>();
}

template <typename Scope, typename = void>
struct has_scope_traits : std::false_type {};

template <typename Scope>
struct has_scope_traits<
    Scope, std::void_t<decltype(::dingo::scope_traits<Scope>::descriptor)>>
    : std::true_type {};

template <typename Dependencies> struct declared_dependencies {
  using type = void;
};

template <typename... Types>
struct declared_dependencies<::dingo::dependencies<Types...>> {
  using type = constructor_factory<declared_leaf<Types...>>;
};
} // namespace detail

template <typename... Parts> struct bind_registration<::dingo::bind<Parts...>> {
  using type = registration<Parts...>;
};

namespace detail {
// The interfaces a registration declares, without the stored type: declaring
// only the stored type, its references and qualifiers removed, names the
// binding by its own type, as declaring no interface does.
template <typename Stored, typename Interfaces> struct declared_interfaces {
  using type = Interfaces;
};

template <typename Stored, typename Interface>
struct declared_interfaces<Stored, type_list<Interface>> {
  using type =
      std::conditional_t<std::is_same_v<remove_cvref_t<Stored>, Interface>,
                         type_list<>, type_list<Interface>>;
};
} // namespace detail

// The binding type a registration declares. A registration whose only
// argument is a callable factory makes its object by it.
template <typename... Parts> struct registration {
  using scope_part =
      typename detail::find_part<::dingo::scope, detail::missing_part,
                                 Parts...>::type;
  using storage_part =
      typename detail::find_part<::dingo::storage, detail::missing_part,
                                 Parts...>::type;
  using interfaces_part =
      typename detail::find_part<::dingo::interfaces, ::dingo::interfaces<>,
                                 Parts...>::type;
  static_assert(core::require<!std::is_same_v<scope_part, detail::missing_part>,
                              core::diag::registration_without_scope>());
  static_assert(
      core::require<!std::is_same_v<storage_part, detail::missing_part>,
                    core::diag::registration_without_storage>());
  static_assert(core::require<
                std::is_same_v<scope_part, detail::missing_part> ||
                    detail::has_scope_traits<typename scope_part::type>::value,
                core::diag::scope_undeclared, typename scope_part::type>());

  using repeated_wrapper =
      typename detail::repeated_alternatives<typename storage_part::type>::type;
  static_assert(
      core::require<std::is_void_v<repeated_wrapper>,
                    core::diag::alternatives_repeated, repeated_wrapper>());

  using factory_part =
      typename detail::find_part<::dingo::factory, detail::missing_part,
                                 Parts...>::type;
  using dependencies_part =
      typename detail::find_part<::dingo::dependencies, detail::missing_part,
                                 Parts...>::type;
  static constexpr bool declares_dependencies =
      !std::is_same_v<dependencies_part, detail::missing_part>;
  static_assert(
      core::require<!declares_dependencies ||
                        std::is_same_v<factory_part, detail::missing_part>,
                    core::diag::registration_dependencies_twice,
                    typename storage_part::type>());
  // The factory a factory<> part declares, or the constructor of the types a
  // dependencies<> part declares, or the detected constructor.
  using declared_factory = typename std::conditional_t<
      std::is_same_v<factory_part, detail::missing_part>,
      std::conditional_t<
          std::is_same_v<dependencies_part, detail::missing_part>,
          std::enable_if<true, detected_factory>,
          detail::declared_dependencies<dependencies_part>>,
      factory_of<typename factory_part::type>>::type;
  using locals_part =
      typename detail::find_part<::dingo::bindings, ::dingo::bindings<>,
                                 Parts...>::type;

  using key_part = typename detail::find_key_part<Parts...>::type;

  // The factory and the key a registration takes from its arguments: a
  // callable factory, and a key value, which a key part must not also name.
  template <typename... Args> struct with_arguments {
    using factory =
        typename detail::find_callable_factory<declared_factory, Args...>::type;
    static_assert(core::require<!declares_dependencies ||
                                    std::is_same_v<factory, declared_factory>,
                                core::diag::registration_dependencies_twice,
                                typename storage_part::type>());
    using runtime = typename detail::find_runtime_key<void, Args...>::type;
    static_assert(
        core::require<
            std::is_void_v<runtime> || std::is_same_v<key_part, no_key>,
            core::diag::registration_key_twice, typename storage_part::type>());
    using key = std::conditional_t<std::is_void_v<runtime>, key_part, runtime>;
    using aliases =
        typename detail::find_key_aliases<key, type_list<>, Args...>::type;
  };

  template <typename... Args>
  using binding_for = binding_type<
      typename scope_part::type, typename storage_part::type,
      typename detail::declared_interfaces<
          typename storage_part::type, typename interfaces_part::type>::type,
      typename with_arguments<Args...>::factory,
      typename with_arguments<Args...>::key,
      typename detail::local_bindings_of<locals_part>::type,
      typename with_arguments<Args...>::aliases>;

  using binding = binding_for<>;
};

} // namespace dingo::core
