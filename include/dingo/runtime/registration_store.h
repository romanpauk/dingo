//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The registration half of a runtime registry: the runtime key validation,
// the lookup commit, and the rollback of both. The public registration API
// stays in registry.h; this store owns the rules and the commit.

#include <dingo/base/exceptions.h>
#include <dingo/base/type_list.h>
#include <dingo/registration/requirements.h>
#include <dingo/runtime/selection.h>
#include <dingo/runtime/transaction.h>
#include <dingo/type/annotated.h>
#include <dingo/type/key.h>
#include <dingo/type/normalized_type.h>

#include <tuple>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename Interface> struct key_value_interface {
  using type = normalized_type_t<Interface>;
};

template <> struct key_value_interface<void> {
  using type = void;
};

template <typename Interface>
using key_value_interface_t = typename key_value_interface<Interface>::type;

template <typename ContainerTraits, typename Allocator>
class registration_store : public lookup_selection<ContainerTraits, Allocator> {
  using lookup_base = lookup_selection<ContainerTraits, Allocator>;

protected:
  using store_type = registration_store;
  using runtime_transaction_type = runtime_transaction<Allocator>;
  using typename lookup_base::allocator_type;
  using typename lookup_base::base_lookup_cardinality;
  using typename lookup_base::base_lookup_entry;
  using typename lookup_base::lookup_definition_type;
  using typename lookup_base::lookup_index_entries;
  using typename lookup_base::normalized_lookup_entries;
  using typename lookup_base::rtti_type;
  using typename lookup_base::runtime_binding_interface_type;
  using typename lookup_base::runtime_bindings_state;
  using typename lookup_base::runtime_lookup_binding_view;
  using typename lookup_base::runtime_lookup_value;
  template <typename LookupEntry>
  using lookup_entry_cardinality =
      typename lookup_base::template lookup_entry_cardinality<LookupEntry>;
  template <typename Interface, typename Key>
  using lookup_index_route =
      typename lookup_base::template lookup_index_route<Interface, Key>;
  template <typename Interface, typename KeyDomain, typename Cardinality>
  using static_key_lookup_entry_t =
      typename lookup_base::template static_key_lookup_entry_t<
          Interface, KeyDomain, Cardinality>;
  template <typename Interface, typename KeyDomain>
  using static_key_lookup_cardinality_t =
      typename lookup_base::template static_key_lookup_cardinality_t<Interface,
                                                                     KeyDomain>;
  template <typename LookupEntry>
  using runtime_lookup_entry_traits =
      typename lookup_base::template runtime_lookup_entry_traits<LookupEntry>;

  template <typename TypeInterface>
  using runtime_lookup_entries_for_interface_t =
      typename lookup_base::template runtime_lookup_entries_for_interface_t<
          TypeInterface>;

  template <typename Arg>
  using runtime_registration_arg_traits =
      detail::runtime_registration_key_arg_traits<std::decay_t<Arg>>;

  template <typename Entry, typename Arg>
  struct runtime_registration_arg_matches_entry {
  private:
    using entry_traits = runtime_lookup_entry_traits<Entry>;
    using arg_traits = runtime_registration_arg_traits<Arg>;
    using arg_interface =
        detail::key_value_interface_t<typename arg_traits::interface_type>;

  public:
    static constexpr bool value =
        std::is_same_v<typename entry_traits::key_type,
                       typename arg_traits::key_type> &&
        (std::is_void_v<arg_interface> ||
         std::is_same_v<typename entry_traits::interface_type, arg_interface>);
  };

  template <typename Entry, typename Arg>
  struct runtime_registration_arg_matches_qualified_entry {
  private:
    using entry_traits = runtime_lookup_entry_traits<Entry>;
    using arg_traits = runtime_registration_arg_traits<Arg>;
    using arg_interface =
        detail::key_value_interface_t<typename arg_traits::interface_type>;

  public:
    static constexpr bool value =
        !std::is_void_v<arg_interface> &&
        std::is_same_v<typename entry_traits::key_type,
                       typename arg_traits::key_type> &&
        std::is_same_v<typename entry_traits::interface_type, arg_interface>;
  };

  template <typename Left, typename Right>
  struct runtime_registration_args_are_same_key {
  private:
    using left_traits = runtime_registration_arg_traits<Left>;
    using right_traits = runtime_registration_arg_traits<Right>;
    using left_interface =
        detail::key_value_interface_t<typename left_traits::interface_type>;
    using right_interface =
        detail::key_value_interface_t<typename right_traits::interface_type>;

  public:
    static constexpr bool value =
        std::is_same_v<typename left_traits::key_type,
                       typename right_traits::key_type> &&
        std::is_same_v<left_interface, right_interface>;
  };

  template <typename Arg, typename... KeyValueArgs>
  static constexpr std::size_t runtime_registration_key_arg_count_v =
      (std::size_t{0} + ... +
       (runtime_registration_args_are_same_key<Arg, KeyValueArgs>::value ? 1U
                                                                         : 0U));

  template <typename Key, typename... KeyValueArgs>
  static constexpr std::size_t
      unqualified_runtime_registration_key_arg_count_v =
          (std::size_t{0} + ... +
           ((std::is_same_v<typename runtime_registration_arg_traits<
                                KeyValueArgs>::key_type,
                            Key> &&
             std::is_void_v<typename runtime_registration_arg_traits<
                 KeyValueArgs>::interface_type>)
                ? 1U
                : 0U));

  template <typename Entry, typename... KeyValueArgs>
  static constexpr std::size_t qualified_runtime_registration_key_arg_count_v =
      (std::size_t{0} + ... +
       (runtime_registration_arg_matches_qualified_entry<Entry,
                                                         KeyValueArgs>::value
            ? 1U
            : 0U));

  template <typename Entries, typename Key>
  struct runtime_lookup_entry_key_count;

  template <typename Key>
  struct runtime_lookup_entry_key_count<type_list<>, Key>
      : std::integral_constant<std::size_t, 0> {};

  template <typename Head, typename... Tail, typename Key>
  struct runtime_lookup_entry_key_count<type_list<Head, Tail...>, Key>
      : std::integral_constant<
            std::size_t,
            (runtime_registration_arg_matches_entry<Head, Key>::value ? 1U
                                                                      : 0U) +
                runtime_lookup_entry_key_count<type_list<Tail...>,
                                               Key>::value> {};

  template <typename InterfaceList, typename Arg>
  struct runtime_registration_key_match_count;

  template <typename Arg>
  struct runtime_registration_key_match_count<type_list<>, Arg>
      : std::integral_constant<std::size_t, 0> {};

  template <typename Interface, typename... Tail, typename Arg>
  struct runtime_registration_key_match_count<type_list<Interface, Tail...>,
                                              Arg>
      : std::integral_constant<
            std::size_t,
            runtime_lookup_entry_key_count<
                runtime_lookup_entries_for_interface_t<Interface>, Arg>::value +
                runtime_registration_key_match_count<type_list<Tail...>,
                                                     Arg>::value> {};

  template <typename InterfaceList, typename... KeyValueArgs>
  static constexpr void validate_supplied_runtime_registration_keys() {
    static_assert(
        ((runtime_registration_key_arg_count_v<KeyValueArgs, KeyValueArgs...> ==
          1U) &&
         ...),
        "duplicate runtime key value arguments for runtime lookup "
        "registration");
    static_assert(
        ((runtime_registration_key_match_count<InterfaceList,
                                               KeyValueArgs>::value > 0U) &&
         ...),
        "supplied runtime key value has no matching runtime-key lookup "
        "definition for the registered interface");
  }

  template <typename Entries, typename... KeyValueArgs>
  static constexpr void validate_required_runtime_registration_keys() {
    static_assert(type_list_size_v<Entries> > 0,
                  "runtime dingo::key_value<K>{value} registration requires a "
                  "matching runtime-key lookup definition");
    for_each(Entries{}, [](auto element) {
      using entry_type = typename decltype(element)::type;
      using key_type =
          typename runtime_lookup_entry_traits<entry_type>::key_type;
      static_assert(
          qualified_runtime_registration_key_arg_count_v<entry_type,
                                                         KeyValueArgs...> +
                  unqualified_runtime_registration_key_arg_count_v<
                      key_type, KeyValueArgs...> >=
              1U,
          "runtime-key lookup registration requires a runtime key value for "
          "each declared runtime-key lookup");
    });
  }
  template <typename T> static T invalid_registration_return();

  struct runtime_binding_value {
    runtime_binding_value(const runtime_binding_value &) = delete;
    runtime_binding_value &operator=(const runtime_binding_value &) = delete;
    runtime_binding_value(runtime_binding_value &&) = delete;
    runtime_binding_value &operator=(runtime_binding_value &&) = delete;

  protected:
    runtime_binding_value() = default;
    ~runtime_binding_value() = default;
  };

  template <std::size_t Index, typename Binding>
  struct runtime_binding_member : Binding {
    template <typename... Args>
    explicit runtime_binding_member(Args &&...args)
        : Binding(std::forward<Args>(args)...) {}
  };

  template <typename Derived, typename Indexes, typename... Bindings>
  struct runtime_binding_value_impl_base;

  template <typename Derived, std::size_t... Indexes, typename... Bindings>
  struct runtime_binding_value_impl_base<
      Derived, std::index_sequence<Indexes...>, Bindings...>
      : runtime_binding_value, runtime_binding_member<Indexes, Bindings>... {
    using container_type =
        typename type_list_head_t<type_list<Bindings...>>::container_type;

    // NOLINTBEGIN(bugprone-use-after-move)
    template <typename... Args>
    explicit runtime_binding_value_impl_base(Args &&...args)
        : runtime_binding_member<Indexes, Bindings>(
              std::forward<Args>(args)...)... {}
    // NOLINTEND(bugprone-use-after-move)

    template <typename Interface> runtime_lookup_binding_view view() {
      return view_impl<Interface, 0, Bindings...>();
    }

    container_type &get_container() {
      return static_cast<typename store_type::template runtime_binding_member<
          0, type_list_head_t<type_list<Bindings...>>> &>(*this)
          .get_container();
    }

  private:
    template <typename Interface, std::size_t Index, typename Head,
              typename... Tail>
    runtime_lookup_binding_view view_impl() {
      if constexpr (std::is_same_v<
                        typename Head::interface_type,
                        typename annotated_traits<Interface>::type>) {
        auto &binding =
            static_cast<typename store_type::template runtime_binding_member<
                Index, Head> &>(*this);
        return {std::addressof(binding)};
      } else if constexpr (sizeof...(Tail) > 0) {
        return view_impl<Interface, Index + 1, Tail...>();
      } else {
        static_assert(sizeof...(Tail) > 0,
                      "lookup interface is not present in runtime binding "
                      "value");
      }
    }
  };

  template <typename... Bindings>
  struct runtime_binding_value_impl final
      : runtime_binding_value_impl_base<runtime_binding_value_impl<Bindings...>,
                                        std::index_sequence_for<Bindings...>,
                                        Bindings...> {
    using base_type =
        runtime_binding_value_impl_base<runtime_binding_value_impl<Bindings...>,
                                        std::index_sequence_for<Bindings...>,
                                        Bindings...>;

    using base_type::base_type;
  };

  template <typename Entry, typename Tuple, std::size_t Index = 0>
  static decltype(auto) qualified_registration_key_value(Tuple &keys) {
    using tuple_type = std::remove_reference_t<Tuple>;
    if constexpr (Index >= std::tuple_size_v<tuple_type>) {
      return unqualified_registration_key_value<Entry>(keys);
    } else if constexpr (runtime_registration_arg_matches_qualified_entry<
                             Entry, std::decay_t<std::tuple_element_t<
                                        Index, tuple_type>>>::value) {
      return std::get<Index>(keys).value();
    } else {
      return qualified_registration_key_value<Entry, Tuple, Index + 1>(keys);
    }
  }

  template <typename Entry, typename Tuple, std::size_t Index = 0>
  static decltype(auto) unqualified_registration_key_value(Tuple &keys) {
    using tuple_type = std::remove_reference_t<Tuple>;
    using key_type = typename runtime_lookup_entry_traits<Entry>::key_type;
    if constexpr (Index >= std::tuple_size_v<tuple_type>) {
      static_assert(Index < std::tuple_size_v<tuple_type>,
                    "missing runtime key value for runtime-key lookup "
                    "registration");
    } else {
      using element_type =
          std::decay_t<std::tuple_element_t<Index, tuple_type>>;
      using element_traits = runtime_registration_arg_traits<element_type>;
      if constexpr (std::is_same_v<typename element_traits::key_type,
                                   key_type> &&
                    std::is_void_v<typename element_traits::interface_type>) {
        return std::get<Index>(keys).value();
      } else {
        return unqualified_registration_key_value<Entry, Tuple, Index + 1>(
            keys);
      }
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename LookupKey,
            typename LookupEntry, typename ValueFactory, typename KeyArg>
  auto insert_lookup(runtime_bindings_state &state, ValueFactory &value_factory,
                     const LookupKey &lookup_key, const KeyArg &key_arg) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();

    if constexpr (std::is_same_v<
                      typename lookup_entry_cardinality<LookupEntry>::type,
                      ::dingo::one>) {
      auto [handle, inserted] = index.try_emplace(
          key_arg, value_factory.template make<TypeInterface>());
      if (!inserted) {
        throw detail::make_lookup_already_registered_exception<
            TypeInterface, typename TypeStorage::type>(lookup_key, key_arg);
      }
      return handle;
    } else {
      return index.emplace(key_arg,
                           value_factory.template make<TypeInterface>());
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename LookupKey,
            typename ValueFactory, typename KeyResolver>
  void commit_lookup(runtime_bindings_state &, ValueFactory &,
                     runtime_transaction_type &, KeyResolver &&,
                     const LookupKey &, type_list<>) {}

  template <typename TypeInterface, typename TypeStorage, typename LookupKey,
            typename Head, typename... Tail, typename ValueFactory,
            typename KeyResolver>
  void commit_lookup(runtime_bindings_state &state, ValueFactory &value_factory,
                     runtime_transaction_type &transaction,
                     KeyResolver &&key_resolver, const LookupKey &lookup_key,
                     type_list<Head, Tail...>) {
    static_assert(ValueFactory::shared_owner || sizeof...(Tail) == 0,
                  "unique runtime binding owner can only be inserted into one "
                  "lookup");
    decltype(auto) key_arg = key_resolver(type_list_iterator<Head>{});
    auto handle = insert_lookup<TypeInterface, TypeStorage, LookupKey, Head>(
        state, value_factory, lookup_key, key_arg);
    record_lookup_rollback<Head>(transaction, state, handle, key_arg);
    commit_lookup<TypeInterface, TypeStorage, LookupKey>(
        state, value_factory, transaction,
        std::forward<KeyResolver>(key_resolver), lookup_key,
        type_list<Tail...>{});
  }

  template <typename LookupEntry, typename Key, typename Binding>
  static void erase_lookup_binding(runtime_bindings_state &state,
                                   const Key &key, Binding *binding) noexcept {
    auto &index = state.lookup_indexes.template get<LookupEntry>();
    if constexpr (std::is_same_v<
                      typename lookup_entry_cardinality<LookupEntry>::type,
                      ::dingo::one>) {
      auto it = index.find(key);
      if (it != index.end()) {
        auto &value = detail::lookup_iterator_value(it);
        if (std::addressof(value.binding()) == binding) {
          index.erase(it);
          return;
        }
      }
    } else {
      auto [first, last] = index.equal_range(key);
      for (auto it = first; it != last; ++it) {
        auto &value = detail::lookup_iterator_value(it);
        if (std::addressof(value.binding()) == binding) {
          index.erase(it);
          return;
        }
      }
    }
    assert(false && "runtime lookup binding was not found");
  }

  template <typename LookupEntry, typename Handle, typename Key>
  static void record_lookup_rollback(runtime_transaction_type &transaction,
                                     runtime_bindings_state &state,
                                     Handle handle, const Key &key) {
    auto &value = detail::lookup_iterator_value(handle);
    auto *binding = std::addressof(value.binding());
    try {
      transaction.on_rollback(
          [&state, key = std::decay_t<Key>(key), binding]() noexcept {
            erase_lookup_binding<LookupEntry>(state, key, binding);
          });
    } catch (...) {
      erase_lookup_binding<LookupEntry>(state, key, binding);
      throw;
    }
  }

  bool commit_singular_base_lookup(runtime_bindings_state &state,
                                   runtime_transaction_type &transaction,
                                   detail::base_lookup_key<rtti_type> key,
                                   runtime_lookup_value value) {
    auto &index = state.lookup_indexes.template get<base_lookup_entry>();
    auto [handle, inserted] = index.try_emplace(key, std::move(value));
    if (inserted) {
      record_lookup_rollback<base_lookup_entry>(transaction, state, handle,
                                                key);
    }
    return inserted;
  }

  template <typename Owner> struct shared_lookup_value_factory {
    static constexpr bool shared_owner = true;
    Owner *owner;

    template <typename TypeInterface> runtime_lookup_value make() {
      return runtime_lookup_value(owner->template view<TypeInterface>());
    }
  };

  template <bool SharedOwner, typename Binding>
  struct direct_lookup_value_factory {
    static constexpr bool shared_owner = SharedOwner;
    Binding *binding;

    template <typename TypeInterface> runtime_lookup_value make() {
      static_assert(
          std::is_same_v<typename Binding::interface_type,
                         typename annotated_traits<TypeInterface>::type>);
      return runtime_lookup_value(runtime_lookup_binding_view{binding});
    }
  };

  template <typename TypeInterface, typename TypeStorage, typename LookupKey,
            typename Cardinality, typename ValueFactory>
  void commit_static_lookup(runtime_bindings_state &state,
                            ValueFactory &value_factory,
                            runtime_transaction_type &transaction,
                            LookupKey lookup_key) {
    using key_domain = typename LookupKey::definition_type;
    using explicit_lookup_entry =
        static_key_lookup_entry_t<TypeInterface, key_domain, Cardinality>;
    using route = lookup_index_route<TypeInterface, LookupKey>;
    using routed_lookup_entry = typename route::entry;
    using routed_cardinality =
        typename lookup_entry_cardinality<routed_lookup_entry>::type;
    if constexpr (!std::is_void_v<explicit_lookup_entry> ||
                  std::is_same_v<Cardinality, ::dingo::one> ||
                  std::is_same_v<routed_cardinality, Cardinality>) {
      static_assert(!std::is_void_v<routed_lookup_entry>);
      auto key_resolver = [&](auto) { return route::key(lookup_key); };
      commit_lookup<TypeInterface, TypeStorage, LookupKey>(
          state, value_factory, transaction, key_resolver, lookup_key,
          type_list<routed_lookup_entry>{});
    } else {
      static_assert(std::is_same_v<Cardinality, ::dingo::one>,
                    "static-key many lookup registrations require an explicit "
                    "dingo lookup definition");
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename LookupKey,
            typename ValueFactory, typename... KeyValueArgs>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  void commit_registration(runtime_bindings_state &state,
                           ValueFactory &value_factory,
                           runtime_transaction_type &transaction,
                           LookupKey lookup_key, KeyValueArgs &&...key_values) {
    check_interface_requirements<TypeStorage,
                                 typename annotated_traits<TypeInterface>::type,
                                 typename TypeStorage::type>();
    ((void)key_values, ...);
    using key_domain = typename LookupKey::definition_type;
    using lookup_cardinality_type =
        static_key_lookup_cardinality_t<TypeInterface, key_domain>;

    if constexpr (sizeof...(KeyValueArgs) > 0) {
      using lookup_entries =
          runtime_lookup_entries_for_interface_t<TypeInterface>;
      if constexpr (type_list_size_v<lookup_entries> > 0) {
        validate_required_runtime_registration_keys<lookup_entries,
                                                    KeyValueArgs...>();
        auto key_value_tuple = std::forward_as_tuple(key_values...);
        auto key_resolver = [&](auto element) -> decltype(auto) {
          using entry_type = typename decltype(element)::type;
          return qualified_registration_key_value<entry_type>(key_value_tuple);
        };
        commit_lookup<TypeInterface, TypeStorage, LookupKey>(
            state, value_factory, transaction, key_resolver, lookup_key,
            lookup_entries{});
      } else {
        commit_static_lookup<TypeInterface, TypeStorage, LookupKey,
                             lookup_cardinality_type>(state, value_factory,
                                                      transaction, lookup_key);
      }
    } else {
      commit_static_lookup<TypeInterface, TypeStorage, LookupKey,
                           lookup_cardinality_type>(state, value_factory,
                                                    transaction, lookup_key);
    }
  }

  template <class Storage, class TypeInterface, class Type>
  void check_interface_requirements() {
    detail::interface_registration_requirements<Storage, TypeInterface,
                                                Type>::assert_valid();
  }
};

} // namespace detail
} // namespace dingo
