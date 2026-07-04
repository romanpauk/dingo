//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/auto_constructible.h>
#include <dingo/core/binding_model.h>
#include <dingo/core/binding_resolution_policy.h>
#include <dingo/core/binding_selection.h>
#include <dingo/core/exceptions.h>
#include <dingo/core/key.h>
#include <dingo/core/none.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/invoke.h>
#include <dingo/lookup/lookup.h>
#include <dingo/lookup/operations.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/annotated.h>
#include <dingo/registration/collection_traits.h>
#include <dingo/registration/requirements.h>
#include <dingo/resolution/runtime_binding.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/lookup_index.h>
#include <dingo/static/local_resolution.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_list.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>
#include <vector>

namespace dingo {
namespace detail {

template <typename StaticRegistry, typename ParentContainer>
class container_with_static_bindings;

template <typename Interface> struct key_value_interface {
  using type = normalized_type_t<Interface>;
};

template <> struct key_value_interface<void> {
  using type = void;
};

template <typename Interface>
using key_value_interface_t = typename key_value_interface<Interface>::type;

template <typename Container, typename Request, typename IdType,
          typename = void>
struct has_key_value_lookup_definition : std::false_type {};

template <typename Container, typename Request, typename IdType>
struct has_key_value_lookup_definition<
    Container, Request, IdType,
    std::void_t<typename Container::lookup_definition_type>>
    : std::bool_constant<!std::is_void_v<selected_lookup_entry_t<
          normalized_type_t<Request>, std::decay_t<IdType>,
          normalize_lookup_definitions_t<
              typename Container::lookup_definition_type>>>> {};

template <typename Container, typename Request, typename IdType>
inline constexpr bool has_key_value_lookup_definition_v =
    has_key_value_lookup_definition<Container, Request, IdType>::value;

template <typename T>
inline constexpr bool is_runtime_auto_constructible_dependency_v =
    std::is_same_v<dependency_value_t<T>, std::decay_t<T>> &&
    (!std::is_reference_v<T> ||
     (std::is_lvalue_reference_v<T> &&
      std::is_const_v<std::remove_reference_t<T>> &&
      is_auto_constructible<std::decay_t<T>>::value));

template <typename T, typename = void>
struct has_static_registry_type : std::false_type {};

template <typename T>
struct has_static_registry_type<T,
                                std::void_t<typename T::static_registry_type>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_static_registry_type_v =
    has_static_registry_type<T>::value;

template <typename Container, typename Request, typename IdType,
          typename = void>
struct has_binding_status : std::false_type {};

template <typename Container, typename Request, typename IdType>
struct has_binding_status<
    Container, Request, IdType,
    std::void_t<
        decltype(std::declval<Container &>().template binding_status<Request>(
            std::declval<IdType &>()))>> : std::true_type {};

template <typename Container, typename Request, typename IdType>
inline constexpr bool has_binding_status_v =
    has_binding_status<Container, Request, IdType>::value;

template <typename Container, typename Request, typename IdType>
constexpr bool has_lookup() {
  if constexpr (is_none_v<std::decay_t<IdType>> || is_typed_key_v<IdType>) {
    return true;
  } else {
    return has_key_value_lookup_definition_v<Container, Request, IdType>;
  }
}

template <typename Request, typename ParentContainer, typename IdType>
bool should_resolve_missing_from_parent(ParentContainer &parent, IdType &id) {
  if constexpr (!has_lookup<ParentContainer, Request, IdType>()) {
    return false;
  } else if constexpr (is_runtime_auto_constructible_dependency_v<Request> &&
                       !has_static_registry_type_v<ParentContainer> &&
                       has_binding_status_v<ParentContainer, Request, IdType>) {
    return parent.template binding_status<Request>(id) !=
           binding_status::not_found;
  } else {
    return true;
  }
}

} // namespace detail

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot>
class runtime_registry : public allocator_base<Allocator> {
  friend class runtime_context;
  template <typename> friend class detail::runtime_registration_api;
  template <typename T, typename Context, typename Container>
  friend T detail::resolve_context_request(Context &, Container &);
  template <typename, typename> friend class detail::binding_resolution;
  template <typename, typename>
  friend class detail::container_with_static_bindings;
  template <typename, typename, typename> friend class runtime_container;
  template <typename ContainerTraitsT, typename AllocatorT,
            typename ParentRegistryT, typename ResolveRootT>
  friend class runtime_registry;
  template <typename ContainerTraitsT, typename AllocatorT,
            typename ParentRegistryT, typename ResolveRootT>
  using rebind_t = runtime_registry<ContainerTraitsT, AllocatorT,
                                    ParentRegistryT, ResolveRootT>;
  using registry_type =
      runtime_registry<ContainerTraits, Allocator, ParentRegistry, ResolveRoot>;
  using resolve_root_type =
      std::conditional_t<std::is_same_v<void, ResolveRoot>, registry_type,
                         ResolveRoot>;
  using container_type = resolve_root_type;
  using runtime_binding_interface_type =
      runtime_binding_interface<container_type>;
  using runtime_selection =
      detail::runtime_binding_selection<runtime_binding_interface_type>;
  template <typename Registration>
  using registration_container_type =
      runtime_registry<typename ContainerTraits::template rebind_t<
                           type_list<typename ContainerTraits::tag_type,
                                     typename Registration::interface_type>>,
                       Allocator, resolve_root_type, resolve_root_type>;
  using parent_registry_type =
      std::conditional_t<std::is_same_v<void, ParentRegistry>, registry_type,
                         ParentRegistry>;

public:
  using container_traits_type = ContainerTraits;
  using allocator_type = Allocator;
  using parent_container_type = resolve_root_type;
  using rtti_type = typename ContainerTraits::rtti_type;
  using lookup_definition_type =
      detail::container_lookup_definition_type_t<ContainerTraits>;
  runtime_registry()
      : allocator_base<allocator_type>(allocator_type()),
        runtime_bindings_(get_allocator()) {}

  runtime_registry(const allocator_type &alloc)
      : allocator_base<allocator_type>(alloc),
        runtime_bindings_(get_allocator()) {}

  runtime_registry(resolve_root_type *root,
                   const allocator_type &alloc = allocator_type())
      : allocator_base<allocator_type>(alloc), resolve_root_(root),
        runtime_bindings_(get_allocator()) {

    if constexpr (!std::is_same_v<void, ParentRegistry>) {
      static_assert(
          !detail::is_tagged_container_v<container_traits_type> ||
              !std::is_same_v<typename container_traits_type::tag_type,
                              typename parent_registry_type::
                                  container_traits_type::tag_type>,
          "static typemap based containers require parent and child "
          "container tags to be different");
    }
  }

  runtime_registry(const runtime_registry &) = delete;
  runtime_registry &operator=(const runtime_registry &) = delete;

  ~runtime_registry() = default;

  allocator_type &get_allocator() {
    return allocator_base<allocator_type>::get_allocator();
  }

protected:
  struct runtime_bindings_state;

  resolve_root_type *resolve_root() {
    if constexpr (std::is_same_v<void, ResolveRoot> ||
                  std::is_same_v<resolve_root_type, registry_type>) {
      return this;
    } else if constexpr (std::is_base_of_v<registry_type, resolve_root_type>) {
      return static_cast<resolve_root_type *>(this);
    } else {
      return resolve_root_;
    }
  }

  const resolve_root_type *resolve_root() const {
    if constexpr (std::is_same_v<void, ResolveRoot> ||
                  std::is_same_v<resolve_root_type, registry_type>) {
      return this;
    } else if constexpr (std::is_base_of_v<registry_type, resolve_root_type>) {
      return static_cast<const resolve_root_type *>(this);
    } else {
      return resolve_root_;
    }
  }

public:
  template <typename... TypeArgs> auto register_type() {
    return prepare_binding<TypeArgs...>(resolve_root(), none_t{});
  }

  template <typename... TypeArgs, typename Arg,
            std::enable_if_t<!detail::is_runtime_registration_key_arg_v<Arg>,
                             int> = 0>
  auto register_type(Arg &&arg) {
    return prepare_binding<TypeArgs...>(resolve_root(), std::forward<Arg>(arg));
  }

  template <typename... TypeArgs, typename... KeyArgs,
            std::enable_if_t<
                (sizeof...(KeyArgs) > 0 &&
                 detail::are_runtime_registration_key_args_v<KeyArgs...>),
                int> = 0>
  auto register_type(KeyArgs &&...keys) {
    return prepare_binding<TypeArgs...>(resolve_root(), none_t{},
                                        std::forward<KeyArgs>(keys)...);
  }

  template <typename... TypeArgs, typename Arg, typename... KeyArgs,
            std::enable_if_t<
                (sizeof...(KeyArgs) > 0 &&
                 !detail::is_runtime_registration_key_arg_v<Arg> &&
                 detail::are_runtime_registration_key_args_v<KeyArgs...>),
                int> = 0>
  auto register_type(Arg &&arg, KeyArgs &&...keys) {
    return prepare_binding<TypeArgs...>(resolve_root(), std::forward<Arg>(arg),
                                        std::forward<KeyArgs>(keys)...);
  }

protected:
  template <typename T, typename IdType = none_t,
            typename R = dependency_result_t<T>>
  R resolve(IdType &&id = IdType()) {
    return resolve_runtime_request<T>(std::forward<IdType>(id));
  }

  template <typename T> T construct_collection() {
    return construct_collection_runtime_request<T>();
  }

  template <typename T, typename Fn> T construct_collection(Fn &&fn) {
    return construct_collection_runtime_request<T>(std::forward<Fn>(fn));
  }

  template <typename T, typename Key> T construct_collection(key_type<Key>) {
    return construct_collection_runtime_request<T>(key_type<Key>{});
  }

  template <typename T, typename Fn, typename Key>
  T construct_collection(Fn &&fn, key_type<Key>) {
    return construct_collection_runtime_request<T>(std::forward<Fn>(fn),
                                                   key_type<Key>{});
  }

  template <typename Signature = void, typename Callable>
  auto invoke(Callable &&callable) {
    return invoke_runtime_request<Signature>(std::forward<Callable>(callable));
  }

  template <typename T, typename IdType = none_t,
            typename R = dependency_result_t<T>>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  R resolve_runtime_request(IdType &&id = IdType()) {
    if constexpr (detail::is_typed_key_v<IdType> &&
                  collection_traits<R>::is_collection) {
      return construct_collection_runtime_request<R>(std::decay_t<IdType>{});
    } else if constexpr (!is_none_v<std::decay_t<IdType>> &&
                         collection_traits<R>::is_collection) {
      return construct_collection_runtime_request<R>(
          detail::binding_collection_append{}, std::forward<IdType>(id));
    } else if constexpr (collection_traits<R>::is_collection) {
      return construct_collection_runtime_request<R>(
          detail::binding_collection_append{}, none_t{});
    } else {
      runtime_context context;
      return resolve_impl<T, true, false>(context, std::forward<IdType>(id));
    }
  }

  template <typename T> T construct_collection_runtime_request() {
    return construct_collection_runtime_request<T>(
        detail::binding_collection_append{});
  }

  template <typename T, typename Fn>
  T construct_collection_runtime_request(Fn &&fn) {
    return construct_collection_runtime_request<T>(std::forward<Fn>(fn),
                                                   none_t{});
  }

  template <typename T, typename Key>
  T construct_collection_runtime_request(key_type<Key>) {
    return construct_collection_runtime_request<T>(
        detail::binding_collection_append{}, key_type<Key>{});
  }

  template <typename T, typename Fn, typename Key>
  T construct_collection_runtime_request(Fn &&fn, key_type<Key>) {
    return construct_collection_runtime_request_impl<T, Key>(
        std::forward<Fn>(fn));
  }

  template <typename T, typename Fn>
  T construct_collection_runtime_request(Fn &&fn, none_t) {
    return construct_collection_runtime_request_impl<T, void>(
        std::forward<Fn>(fn));
  }

  template <typename T, typename Fn, typename IdType,
            std::enable_if_t<!is_none_v<std::decay_t<IdType>> &&
                                 !detail::is_typed_key_v<IdType>,
                             int> = 0>
  T construct_collection_runtime_request(Fn &&fn, IdType &&id) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");

    T results;
    runtime_context context;
    const std::size_t count = count_collection<T>(id);
    if (count == 0 &&
        !has_explicit_collection_lookup<T>(std::forward<IdType>(id))) {
      throw detail::make_collection_type_not_found_exception<T, resolve_type>();
    }

    collection_type::reserve(results, count);
    append_runtime_collection(results, context, std::forward<Fn>(fn),
                              std::forward<IdType>(id));
    return results;
  }

  template <typename T, typename Key, typename Fn>
  T construct_collection_runtime_request_impl(Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");

    T results;
    runtime_context context;
    const std::size_t count = count_collection<T>(collection_key<Key>());
    if (count == 0 &&
        !has_explicit_collection_lookup<T>(collection_key<Key>())) {
      throw detail::make_collection_type_not_found_exception<T, resolve_type>();
    }

    collection_type::reserve(results, count);
    append_runtime_collection(results, context, std::forward<Fn>(fn),
                              collection_key<Key>());
    return results;
  }

  template <typename Signature = void, typename Callable>
  auto invoke_runtime_request(Callable &&callable) {
    using callable_type = std::remove_cv_t<std::remove_reference_t<Callable>>;
    using dispatch_signature =
        detail::callable_dispatch_signature_t<Signature, callable_type>;

    runtime_context context;
    auto type_guard = context.template track_type<callable_type>();
    return detail::callable_invoke<dispatch_signature>::construct(
        std::forward<Callable>(callable), context, *resolve_root());
  }

  template <typename Request, typename Key = void>
  detail::binding_status binding_status() {
    return binding_status<Request>(collection_key<Key>());
  }

  template <typename Request, typename Key = void>
  runtime_selection select_binding() {
    return source_select<Request>(collection_key<Key>());
  }

  template <typename Request, typename IdType>
  runtime_selection select_binding(IdType &&id) {
    return source_select<Request>(std::forward<IdType>(id));
  }

  template <typename Request, typename IdType>
  detail::binding_status binding_status(IdType &&id) {
    auto selection =
        select_binding<Request>(runtime_bindings_, std::forward<IdType>(id));
    return selection.status;
  }

  template <typename T, typename Key = void, typename Fn>
  std::size_t append_collection(T &results, runtime_context &context, Fn &&fn) {
    return append_runtime_collection(results, context, std::forward<Fn>(fn),
                                     collection_key<Key>());
  }

  template <typename T, typename Key = void> std::size_t count_collection() {
    return count_collection<T>(collection_key<Key>());
  }

  template <typename Request, typename Result = Request>
  Result resolve_binding(runtime_selection selection,
                         runtime_context &context) {
    return resolve<Request, Result>(*selection.binding, context);
  }

  template <typename Source> class container_proxy {
  public:
    using container_type = typename Source::container_type;

    explicit container_proxy(Source *source) : source_(source) {}

    container_type &get() const { return source_->container(); }
    container_type *operator->() const { return std::addressof(get()); }
    operator container_type &() const { return get(); }

  private:
    Source *source_;
  };

  template <typename T> static T invalid_registration_return();

  using normalized_lookup_entries =
      detail::normalize_lookup_definitions_t<lookup_definition_type>;
  using base_lookup_definition =
      detail::base_lookup_definition_t<lookup_definition_type>;
  using base_lookup_entry =
      detail::base_lookup_entry<rtti_type,
                                typename base_lookup_definition::cardinality,
                                typename base_lookup_definition::backend_type>;
  using base_lookup_cardinality = typename base_lookup_definition::cardinality;
  using lookup_index_entries = type_list_unique_t<
      type_list_cat_t<normalized_lookup_entries, type_list<base_lookup_entry>>>;
  template <typename LookupEntry>
  using lookup_entry_cardinality =
      detail::lookup_entry_cardinality<LookupEntry>;

  template <typename LookupEntry>
  static constexpr bool lookup_entry_indexed_v =
      detail::contains_lookup_definition<LookupEntry,
                                         lookup_index_entries>::value;

  struct runtime_lookup_binding_view {
    runtime_binding_interface<container_type> *binding = nullptr;
  };

  struct runtime_binding_value {
    runtime_binding_value(const runtime_binding_value &) = delete;
    runtime_binding_value &operator=(const runtime_binding_value &) = delete;
    runtime_binding_value(runtime_binding_value &&) = delete;
    runtime_binding_value &operator=(runtime_binding_value &&) = delete;

    virtual ~runtime_binding_value() = default;

    virtual void destroy(allocator_type &allocator) = 0;

  protected:
    runtime_binding_value() = default;
  };

  template <std::size_t Index, typename Binding>
  struct runtime_binding_member : Binding {
    template <typename... Args>
    explicit runtime_binding_member(Args &&...args)
        : Binding(std::forward<Args>(args)...) {}

    typename Binding::container_type &container() {
      return this->get_container();
    }
  };

  template <typename Derived, typename Indexes, typename... Bindings>
  struct runtime_binding_value_impl_base;

  template <typename Derived, std::size_t... Indexes, typename... Bindings>
  struct runtime_binding_value_impl_base<
      Derived, std::index_sequence<Indexes...>, Bindings...>
      : runtime_binding_value, runtime_binding_member<Indexes, Bindings>... {
    using container_type =
        typename type_list_head_t<type_list<Bindings...>>::container_type;

    template <typename... Args>
    explicit runtime_binding_value_impl_base(Args &&...args)
        : runtime_binding_member<Indexes, Bindings>(
              std::forward<Args>(args)...)... {}

    template <typename Interface> runtime_lookup_binding_view view() {
      return view_impl<Interface, 0, Bindings...>();
    }

    container_type &container() {
      return static_cast<
                 typename registry_type::template runtime_binding_member<
                     0, type_list_head_t<type_list<Bindings...>>> &>(*this)
          .container();
    }

    void destroy(allocator_type &allocator) override {
      auto alloc = allocator_traits::rebind<Derived>(allocator);
      auto *derived = static_cast<Derived *>(this);
      allocator_traits::destroy(alloc, derived);
      allocator_traits::deallocate(alloc, derived, 1);
    }

  private:
    template <typename Interface, std::size_t Index, typename Head,
              typename... Tail>
    runtime_lookup_binding_view view_impl() {
      if constexpr (std::is_same_v<
                        typename Head::interface_type,
                        typename annotated_traits<Interface>::type>) {
        auto &binding =
            static_cast<typename registry_type::template runtime_binding_member<
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

  struct runtime_binding_value_deleter {
    allocator_type *allocator = nullptr;

    void operator()(runtime_binding_value *value) const {
      if (value != nullptr) {
        value->destroy(*allocator);
      }
    }
  };

  struct runtime_lookup_value {
    using unique_owner =
        std::unique_ptr<runtime_binding_value, runtime_binding_value_deleter>;
    using shared_owner = std::shared_ptr<runtime_binding_value>;

    runtime_lookup_value(unique_owner &&value, runtime_lookup_binding_view view)
        : value_(std::move(value)), view_(view) {}
    runtime_lookup_value(const shared_owner &value,
                         runtime_lookup_binding_view view)
        : value_(value), view_(view) {}

    runtime_lookup_value(runtime_lookup_value &&) noexcept = default;
    runtime_lookup_value &operator=(runtime_lookup_value &&) noexcept = default;
    runtime_lookup_value(const runtime_lookup_value &) = delete;
    runtime_lookup_value &operator=(const runtime_lookup_value &) = delete;

    runtime_binding_interface<container_type> &binding() {
      return *view_.binding;
    }

  private:
    std::variant<unique_owner, shared_owner> value_;
    runtime_lookup_binding_view view_;
  };

  struct runtime_bindings_state {
    explicit runtime_bindings_state(allocator_type &allocator)
        : lookup_indexes(allocator) {
      validate_lookup_definitions();
    }

    detail::lookup_index<lookup_index_entries, runtime_lookup_value,
                         allocator_type>
        lookup_indexes;

  private:
    static void validate_lookup_definitions() {
      using entries_type = normalized_lookup_entries;
      static_assert(!detail::has_duplicate_base_lookup_definition_v<
                        lookup_definition_type>,
                    "duplicate dingo base lookup definition");
      static_assert(!detail::has_duplicate_lookup_definition_v<entries_type>,
                    "duplicate dingo lookup definition for interface/key "
                    "domain/cardinality");
      static_assert(!detail::has_duplicate_lookup_key_type_v<entries_type>,
                    "conflicting dingo lookup definitions for interface/key "
                    "domain");
    }
  };

  template <typename Key>
  using collection_key_t =
      std::conditional_t<std::is_void_v<Key>, none_t, key_type<Key>>;

protected:
  template <typename Key> static collection_key_t<Key> collection_key() {
    return {};
  }

  template <typename Interface>
  static detail::base_lookup_key<rtti_type> base_no_key_lookup_key() {
    return {rtti_type::template get_type_index<Interface>(),
            rtti_type::template get_type_index<none_t>()};
  }

  template <typename Interface, typename Key>
  static detail::base_lookup_key<rtti_type> base_typed_key_lookup_key() {
    return {rtti_type::template get_type_index<Interface>(),
            rtti_type::template get_type_index<
                detail::normalized_lookup_key_t<Key>>()};
  }

  template <typename Interface, typename Cardinality>
  using no_key_lookup_entry_t =
      detail::selected_no_key_lookup_entry_t<Interface, Cardinality,
                                             normalized_lookup_entries>;

  template <typename Interface>
  static constexpr bool has_explicit_no_key_one_lookup_v =
      !std::is_void_v<no_key_lookup_entry_t<Interface, ::dingo::one>>;

  template <typename Interface>
  static constexpr bool has_explicit_no_key_many_lookup_v =
      !std::is_void_v<no_key_lookup_entry_t<Interface, ::dingo::many>>;

  template <typename Interface>
  using no_key_lookup_cardinality_t = std::conditional_t<
      has_explicit_no_key_many_lookup_v<Interface>, ::dingo::many,
      std::conditional_t<has_explicit_no_key_one_lookup_v<Interface>,
                         ::dingo::one, base_lookup_cardinality>>;

  template <typename Interface, typename Cardinality>
  using no_key_request_lookup_entry_t = std::conditional_t<
      std::is_void_v<no_key_lookup_entry_t<Interface, Cardinality>>,
      base_lookup_entry, no_key_lookup_entry_t<Interface, Cardinality>>;

  template <typename Interface, typename Key, typename Cardinality>
  using typed_key_lookup_entry_t =
      detail::selected_typed_key_lookup_entry_t<Interface, Key, Cardinality,
                                                normalized_lookup_entries>;

  template <typename Interface, typename Key>
  static constexpr bool has_explicit_typed_key_one_lookup_v =
      !std::is_void_v<typed_key_lookup_entry_t<Interface, Key, ::dingo::one>>;

  template <typename Interface, typename Key>
  static constexpr bool has_explicit_typed_key_many_lookup_v =
      !std::is_void_v<typed_key_lookup_entry_t<Interface, Key, ::dingo::many>>;

  template <typename Interface, typename Key>
  using typed_key_lookup_cardinality_t = std::conditional_t<
      !has_explicit_typed_key_one_lookup_v<Interface, Key> &&
          has_explicit_typed_key_many_lookup_v<Interface, Key>,
      ::dingo::many,
      std::conditional_t<has_explicit_typed_key_one_lookup_v<Interface, Key>,
                         ::dingo::one, base_lookup_cardinality>>;

  template <typename Interface, typename Key, typename Cardinality>
  using typed_key_request_lookup_entry_t = std::conditional_t<
      std::is_void_v<typed_key_lookup_entry_t<Interface, Key, Cardinality>>,
      base_lookup_entry, typed_key_lookup_entry_t<Interface, Key, Cardinality>>;

  template <typename Interface, typename KeyDomain, typename Cardinality>
  struct explicit_static_key_lookup_entry;

  template <typename Interface, typename Cardinality>
  struct explicit_static_key_lookup_entry<Interface, ::dingo::none_t,
                                          Cardinality> {
    using type = no_key_lookup_entry_t<Interface, Cardinality>;
  };

  template <typename Interface, typename Key, typename Cardinality>
  struct explicit_static_key_lookup_entry<Interface, ::dingo::key_type<Key>,
                                          Cardinality> {
    using type = typed_key_lookup_entry_t<Interface, Key, Cardinality>;
  };

  template <typename Interface, typename KeyDomain, typename Cardinality>
  using explicit_static_key_lookup_entry_t =
      typename explicit_static_key_lookup_entry<Interface, KeyDomain,
                                                Cardinality>::type;

  template <typename KeyDomain> struct static_key_route_id {
    using type = none_t;
  };

  template <typename Key> struct static_key_route_id<::dingo::key_type<Key>> {
    using type = ::dingo::key_type<Key>;
  };

  template <typename Interface, typename Key>
  using key_value_lookup_entry_t =
      detail::selected_lookup_entry_t<Interface, Key,
                                      normalized_lookup_entries>;

  template <typename Interface, typename Key>
  using key_value_lookup_cardinality_t = typename lookup_entry_cardinality<
      key_value_lookup_entry_t<Interface, Key>>::type;

  template <typename LookupEntry> struct runtime_lookup_entry_traits {
    static constexpr bool is_key_value = false;
    using interface_type = void;
    using key_type = void;
  };

  template <typename Interface, typename Key, typename Cardinality,
            typename Backend>
  struct runtime_lookup_entry_traits<detail::lookup_entry<
      Interface, detail::key_value_domain<Key>, Cardinality, Backend>> {
    static constexpr bool is_key_value = true;
    using interface_type = Interface;
    using key_type = Key;
  };

  template <typename TypeInterface, typename Entries>
  struct runtime_lookup_entries_for_interface;

  template <typename TypeInterface>
  struct runtime_lookup_entries_for_interface<TypeInterface, type_list<>> {
    using type = type_list<>;
  };

  template <typename TypeInterface, typename Head, typename... Tail>
  struct runtime_lookup_entries_for_interface<TypeInterface,
                                              type_list<Head, Tail...>> {
  private:
    using head_traits = runtime_lookup_entry_traits<Head>;
    using tail_type =
        typename runtime_lookup_entries_for_interface<TypeInterface,
                                                      type_list<Tail...>>::type;
    static constexpr bool matches =
        head_traits::is_key_value &&
        std::is_same_v<normalized_type_t<TypeInterface>,
                       typename head_traits::interface_type>;

  public:
    using type =
        std::conditional_t<matches, type_list_cat_t<type_list<Head>, tail_type>,
                           tail_type>;
  };

  template <typename TypeInterface>
  using runtime_lookup_entries_for_interface_t =
      typename runtime_lookup_entries_for_interface<
          TypeInterface, normalized_lookup_entries>::type;

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

  template <typename Interface, typename IdType> struct request_lookup_traits;

  template <typename Interface>
  struct request_lookup_traits<Interface, ::dingo::none_t> {
    using interface_type = Interface;
    using normalized_interface = normalized_type_t<Interface>;
    using key_domain = ::dingo::none_t;
    using key_value_type = void;
    using selected_cardinality =
        no_key_lookup_cardinality_t<normalized_interface>;
    using lookup_entry = no_key_request_lookup_entry_t<normalized_interface,
                                                       selected_cardinality>;
    static constexpr bool has_explicit_key_value_lookup = false;
    static constexpr bool has_explicit_collection_lookup =
        std::is_same_v<selected_cardinality, ::dingo::many>;
  };

  template <typename Interface, typename Key>
  struct request_lookup_traits<Interface, ::dingo::key_type<Key>> {
    using interface_type = Interface;
    using normalized_interface = normalized_type_t<Interface>;
    using key_type = Key;
    using key_domain = ::dingo::key_type<key_type>;
    using key_value_type = void;
    using selected_cardinality =
        typed_key_lookup_cardinality_t<normalized_interface, key_type>;
    using lookup_entry =
        typed_key_request_lookup_entry_t<normalized_interface, key_type,
                                         selected_cardinality>;
    static constexpr bool has_explicit_key_value_lookup = false;
    static constexpr bool has_explicit_collection_lookup =
        std::is_same_v<selected_cardinality, ::dingo::many>;
  };

  template <typename Interface, typename IdType> struct request_lookup_traits {
    using normalized_interface = normalized_type_t<Interface>;
    using key_value_type = std::decay_t<IdType>;
    using key_domain = detail::key_value_domain<key_value_type>;
    using lookup_entry =
        key_value_lookup_entry_t<normalized_interface, key_value_type>;
    using selected_cardinality =
        typename lookup_entry_cardinality<lookup_entry>::type;
    static constexpr bool has_explicit_key_value_lookup =
        !std::is_void_v<lookup_entry>;
    static constexpr bool has_explicit_collection_lookup =
        has_explicit_key_value_lookup &&
        std::is_same_v<selected_cardinality, ::dingo::many>;
  };

  template <typename Interface, typename IdType> struct lookup_index_route {
    using traits = request_lookup_traits<Interface, std::decay_t<IdType>>;
    using entry = typename traits::lookup_entry;

    static decltype(auto) key(IdType &id) { return id; }
  };

  template <typename Interface> struct lookup_index_route<Interface, none_t> {
    using traits = request_lookup_traits<Interface, none_t>;
    using entry = typename traits::lookup_entry;

    static auto key(none_t) {
      if constexpr (std::is_same_v<entry, base_lookup_entry>) {
        return base_no_key_lookup_key<typename traits::interface_type>();
      } else {
        return none_t{};
      }
    }
  };

  template <typename Interface, typename Key>
  struct lookup_index_route<Interface, ::dingo::key_type<Key>> {
    using traits = request_lookup_traits<Interface, ::dingo::key_type<Key>>;
    using entry = typename traits::lookup_entry;

    static auto key(::dingo::key_type<Key>) {
      if constexpr (std::is_same_v<entry, base_lookup_entry>) {
        return base_typed_key_lookup_key<typename traits::interface_type,
                                         typename traits::key_type>();
      } else {
        return none_t{};
      }
    }
  };

  template <typename T, typename IdType>
  static constexpr bool has_explicit_collection_lookup(IdType &&) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using traits = request_lookup_traits<resolve_type, std::decay_t<IdType>>;
    return traits::has_explicit_collection_lookup;
  }

  template <typename T, typename IdType> struct selected_runtime_binding {
    registry_type &registry;
    IdType &id;

    decltype(auto) select() {
      return registry.template source_select<T>(std::forward<IdType>(id));
    }

    template <typename Request, typename Selection>
    decltype(auto) resolve(runtime_context &context, Selection selection) {
      return registry.template source_resolve<T>(selection, context,
                                                 std::forward<IdType>(id));
    }
  };

  template <typename T, bool RemoveRvalueReferences, bool MayAutoConstruct,
            typename IdType>
  struct missing_runtime_binding {
    registry_type &registry;
    IdType &id;

    template <typename Request>
    dependency_interface_t<Request> resolve(runtime_context &context) {
      return registry
          .template source_missing<T, RemoveRvalueReferences, MayAutoConstruct,
                                   IdType, dependency_interface_t<Request>>(
              context, std::forward<IdType>(id));
    }
  };

  template <typename Request, typename IdType = none_t>
  runtime_selection source_select(IdType &&id = IdType()) {
    return select_binding<Request>(runtime_bindings_, std::forward<IdType>(id));
  }

  template <typename T, typename IdType>
  auto source_resolve(runtime_selection selection, runtime_context &context,
                      IdType &&id) -> dependency_interface_t<T> {
    (void)id;
    return resolve<T, dependency_interface_t<T>>(*selection.binding, context);
  }

  template <typename T, bool RemoveRvalueReferences, bool MayAutoConstruct,
            typename IdType,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R source_missing(runtime_context &context, IdType &&id) {
    using Type = normalized_type_t<T>;
    (void)id;

    if constexpr (!std::is_same_v<void, ParentRegistry> &&
                  !std::is_same_v<void *, decltype(resolve_root_)> &&
                  !std::is_base_of_v<registry_type, resolve_root_type>) {
      if (resolve_root_) {
        if constexpr (is_none_v<std::decay_t<IdType>>) {
          return resolve_root()->template resolve<T, RemoveRvalueReferences>(
              context, none_t{});
        } else if constexpr (detail::is_typed_key_v<IdType>) {
          return resolve_root()->template resolve<T, RemoveRvalueReferences>(
              context, std::decay_t<IdType>{});
        } else {
          return resolve_root()->template resolve<T>(std::forward<IdType>(id));
        }
      }
    }

    if constexpr (MayAutoConstruct && detail::is_typed_key_v<IdType> &&
                  collection_traits<R>::is_collection) {
      return this->template construct_collection_runtime_request<R>(
          detail::binding_collection_append{}, std::decay_t<IdType>{});
    } else if constexpr (MayAutoConstruct &&
                         is_auto_constructible<std::decay_t<T>>::value) {
      if constexpr (constructor<Type>::kind ==
                    detail::constructor_kind::concrete) {
        return auto_construct<T>(context);
      } else if constexpr (is_none_v<std::decay_t<IdType>>) {
        throw detail::make_type_not_found_exception<T>(context);
      } else {
        throw detail::make_type_not_found_exception<T, std::decay_t<IdType>>(
            context);
      }
    } else if constexpr (is_none_v<std::decay_t<IdType>>) {
      throw detail::make_type_not_found_exception<T>(context);
    } else {
      throw detail::make_type_not_found_exception<T, std::decay_t<IdType>>(
          context);
    }
  }

  template <typename T, typename IdType, typename Fn>
  std::size_t for_each_collection_entry(runtime_bindings_state &state,
                                        IdType &&id, Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using lookup_type = normalized_type_t<resolve_type>;
    using traits = request_lookup_traits<lookup_type, std::decay_t<IdType>>;
    using route = lookup_index_route<lookup_type, std::decay_t<IdType>>;

    if constexpr (traits::has_explicit_collection_lookup ||
                  std::is_same_v<typename route::entry, base_lookup_entry>) {
      return for_each_lookup<typename route::entry>(state, route::key(id),
                                                    std::forward<Fn>(fn));
    } else {
      (void)id;
      (void)fn;
      return 0;
    }
  }

  template <typename T, typename Fn, typename IdType>
  std::size_t append_runtime_collection(T &results, runtime_context &context,
                                        Fn &&fn, IdType id) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    return for_each_collection_entry<T>(
        runtime_bindings_, id, [&](auto &entry) {
          fn(results,
             resolve_collection_type<resolve_type>(entry.binding(), context));
        });
  }

  template <typename T, typename IdType>
  std::size_t count_collection(IdType id) {
    std::size_t result = 0;
    for_each_collection_entry<T>(runtime_bindings_, id,
                                 [&](auto &) { ++result; });
    return result;
  }

private:
  template <typename Request, typename IdType>
  runtime_selection select_binding(runtime_bindings_state &state, IdType &&id) {
    using lookup_type = normalized_type_t<Request>;
    using exact_type = std::remove_cv_t<
        std::remove_reference_t<dependency_interface_t<Request>>>;
    if constexpr (!is_none_v<std::decay_t<IdType>> &&
                  !detail::is_typed_key_v<IdType>) {
      using lookup_traits =
          request_lookup_traits<lookup_type, std::decay_t<IdType>>;
      if constexpr (!lookup_traits::has_explicit_key_value_lookup &&
                    !std::is_same_v<lookup_type, exact_type>) {
        using exact_traits =
            request_lookup_traits<exact_type, std::decay_t<IdType>>;
        if constexpr (exact_traits::has_explicit_key_value_lookup) {
          return select_binding_at_interface<Request, IdType, exact_type>(state,
                                                                          id);
        }
      }
      return select_binding_at_interface<Request, IdType, lookup_type>(state,
                                                                       id);
    } else {
      auto selection =
          select_binding_at_interface<Request, IdType, exact_type>(state, id);
      if constexpr (!std::is_same_v<lookup_type, exact_type>) {
        if (!selection.found()) {
          selection = select_binding_at_interface<Request, IdType, lookup_type>(
              state, id);
        }
      }
      return selection;
    }
  }

  template <typename Request, typename IdType, typename Interface>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  runtime_selection select_binding_at_interface(runtime_bindings_state &state,
                                                IdType &id) {
    using route = lookup_index_route<Interface, std::decay_t<IdType>>;
    using lookup_entry = typename route::entry;
    if constexpr (std::is_void_v<lookup_entry>) {
      if constexpr (!std::is_same_v<void, ParentRegistry> &&
                    !std::is_base_of_v<registry_type, resolve_root_type>) {
        return runtime_selection::miss();
      } else {
        static_assert(!std::is_void_v<lookup_entry>,
                      "keyed registration or request has no matching "
                      "dingo lookup definition for interface/key");
      }
    } else {
      return select_lookup<lookup_entry>(state, route::key(id));
    }
  }

  runtime_selection make_selection(runtime_lookup_value *selected,
                                   bool ambiguous) {
    if (ambiguous) {
      return runtime_selection::ambiguity();
    }
    return detail::make_runtime_selection<runtime_binding_interface_type>(
        selected ? &selected->binding() : nullptr);
  }

  template <typename LookupEntry, typename Key>
  runtime_selection select_lookup(runtime_bindings_state &state,
                                  const Key &key) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();
    auto match =
        detail::lookup_find_singular<LookupEntry, runtime_lookup_value>(index,
                                                                        key);
    return make_selection(match.value, match.ambiguous);
  }

  template <typename LookupEntry, typename Key, typename Fn>
  std::size_t for_each_lookup(runtime_bindings_state &state, const Key &key,
                              Fn &&fn) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();
    return detail::lookup_for_each<LookupEntry>(index, key,
                                                std::forward<Fn>(fn));
  }

public:
  template <typename TypeInterface, typename TypeStorage, typename BindingState,
            typename KeyIdType>
  using runtime_registration_binding_t = std::conditional_t<
      is_none_v<KeyIdType>,
      runtime_binding<container_type,
                      typename annotated_traits<TypeInterface>::type,
                      TypeStorage, BindingState>,
      detail::keyed_binding_identity<
          KeyIdType,
          runtime_binding<container_type,
                          typename annotated_traits<TypeInterface>::type,
                          TypeStorage, BindingState>>>;

  template <typename InterfaceList, typename TypeStorage, typename BindingState,
            typename KeyIdType>
  struct runtime_binding_value_owner;

  template <typename... TypeInterfaces, typename TypeStorage,
            typename BindingState, typename KeyIdType>
  struct runtime_binding_value_owner<type_list<TypeInterfaces...>, TypeStorage,
                                     BindingState, KeyIdType> {
    using type = runtime_binding_value_impl<runtime_registration_binding_t<
        TypeInterfaces, TypeStorage, BindingState, KeyIdType>...>;
  };

  template <typename InterfaceList, typename TypeStorage, typename BindingState,
            typename KeyIdType>
  using runtime_binding_value_owner_t =
      typename runtime_binding_value_owner<InterfaceList, TypeStorage,
                                           BindingState, KeyIdType>::type;

private:
  template <typename... TypeArgs, typename Parent, typename Arg,
            typename... KeyValueArgs>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  auto prepare_binding(Parent *parent, Arg &&arg,
                       KeyValueArgs &&...key_values) {
    static_assert(!detail::has_explicit_void_interface_v<TypeArgs...>,
                  "interfaces<void> is not a valid registration target");
    using registration =
        std::conditional_t<!is_none_v<std::decay_t<Arg>>,
                           type_registration<TypeArgs..., factory<Arg>>,
                           type_registration<TypeArgs...>>;
    static_assert(
        !detail::is_key_value_v<typename registration::key_type>,
        "dingo::key_type<T, V> registration keys require a static fixed "
        "runtime-key request");
    using binding_model = detail::binding_model<registration>;
    using bindings_type = typename binding_model::bindings_type;
    using instance_container_type = registration_container_type<registration>;
    (void)arg;
    using interface_types = typename binding_model::interface_types;
    static constexpr bool storage_tag_is_complete =
        binding_model::storage_tag_is_complete;
    static_assert(storage_tag_is_complete,
                  "registered storage tag must be complete; include the "
                  "corresponding dingo/storage header");
    if constexpr (storage_tag_is_complete) {
      using storage_type = typename binding_model::storage_type;
      using registration_requirements = typename binding_model::requirements;
      using key_id_type =
          std::conditional_t<std::is_void_v<typename binding_model::key_type>,
                             none_t,
                             key_type<typename binding_model::key_type>>;

      registration_requirements::assert_valid();

      using runtime_binding_state_type =
          detail::runtime_binding_state_t<resolve_root_type,
                                          instance_container_type, storage_type,
                                          bindings_type>;

      if constexpr (registration_requirements::valid &&
                    type_list_size_v<interface_types> == 1) {
        if constexpr (sizeof...(KeyValueArgs) > 0) {
          validate_supplied_runtime_registration_keys<interface_types,
                                                      KeyValueArgs...>();
        }
        using interface_type = type_list_head_t<interface_types>;

        using runtime_binding_type =
            runtime_binding<container_type,
                            typename annotated_traits<interface_type>::type,
                            storage_type, runtime_binding_state_type>;
        using registered_binding_type = std::conditional_t<
            is_none_v<key_id_type>, runtime_binding_type,
            detail::keyed_binding_identity<key_id_type, runtime_binding_type>>;
        using owner_type = runtime_binding_value_impl<registered_binding_type>;
        static constexpr bool shared_owner =
            sizeof...(KeyValueArgs) > 0 &&
            type_list_size_v<
                runtime_lookup_entries_for_interface_t<interface_type>> > 1;

        if constexpr (!is_none_v<std::decay_t<Arg>>) {
          return commit_binding<shared_owner, interface_type, storage_type,
                                owner_type>(
              parent, key_id_type{},
              std::forward_as_tuple(std::forward<KeyValueArgs>(key_values)...),
              std::forward<Arg>(arg));
        } else {
          return commit_binding<shared_owner, interface_type, storage_type,
                                owner_type>(
              parent, key_id_type{},
              std::forward_as_tuple(std::forward<KeyValueArgs>(key_values)...));
        }
      } else {
        if constexpr (registration_requirements::valid) {
          if constexpr (sizeof...(KeyValueArgs) > 0) {
            validate_supplied_runtime_registration_keys<interface_types,
                                                        KeyValueArgs...>();
          }
          std::shared_ptr<runtime_binding_state_type> data;
          if constexpr (!is_none_v<std::decay_t<Arg>>) {
            data = std::allocate_shared<runtime_binding_state_type>(
                allocator_traits::rebind<runtime_binding_state_type>(
                    get_allocator()),
                parent, std::forward<Arg>(arg));
          } else {
            data = std::allocate_shared<runtime_binding_state_type>(
                allocator_traits::rebind<runtime_binding_state_type>(
                    get_allocator()),
                parent);
          }

          using owner_type = runtime_binding_value_owner_t<
              interface_types, storage_type,
              std::shared_ptr<runtime_binding_state_type>, key_id_type>;
          auto &state = runtime_bindings_;
          auto binding_owner = make_binding_owner<true, owner_type>(data);
          auto *binding_result = binding_owner.get();
          shared_lookup_value_factory<owner_type> value_factory{
              std::move(binding_owner)};

          for_each(interface_types{}, [&](auto element) {
            using interface_type = typename decltype(element)::type;
            commit_registration<interface_type, storage_type>(
                state, value_factory, key_id_type{}, key_values...);
          });
          return container_proxy<owner_type>(binding_result);
        } else {
          return invalid_registration_return<instance_container_type>();
        }
      }
    } else {
      return invalid_registration_return<instance_container_type>();
    }
  }

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

  template <typename TypeInterface, typename TypeStorage, typename KeyIdType,
            typename LookupEntry>
  static auto make_registration_duplicate_exception() {
    using runtime_lookup_traits = runtime_lookup_entry_traits<LookupEntry>;
    if constexpr (runtime_lookup_traits::is_key_value) {
      return detail::make_lookup_already_registered_exception<
          TypeInterface, typename TypeStorage::type,
          typename runtime_lookup_traits::key_type>();
    } else {
      return detail::make_lookup_already_registered_exception<
          TypeInterface, typename TypeStorage::type, std::decay_t<KeyIdType>>();
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename KeyIdType,
            typename LookupEntry, typename ValueFactory, typename KeyArg>
  auto insert_lookup(runtime_bindings_state &state, ValueFactory &value_factory,
                     const KeyArg &key_arg) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();

    if constexpr (std::is_same_v<
                      typename lookup_entry_cardinality<LookupEntry>::type,
                      ::dingo::one>) {
      auto [it, inserted] = index.try_emplace(
          key_arg, value_factory.template make<TypeInterface>());
      if (!inserted) {
        throw make_registration_duplicate_exception<TypeInterface, TypeStorage,
                                                    KeyIdType, LookupEntry>();
      }
      return it;
    } else {
      return index.emplace(key_arg,
                           value_factory.template make<TypeInterface>());
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename KeyIdType,
            typename ValueFactory, typename KeyResolver>
  void commit_lookup(runtime_bindings_state &, ValueFactory &, KeyResolver &&,
                     type_list<>) {}

  template <typename TypeInterface, typename TypeStorage, typename KeyIdType,
            typename Head, typename... Tail, typename ValueFactory,
            typename KeyResolver>
  void commit_lookup(runtime_bindings_state &state, ValueFactory &value_factory,
                     KeyResolver &&key_resolver, type_list<Head, Tail...>) {
    static_assert(ValueFactory::shared_owner || sizeof...(Tail) == 0,
                  "unique runtime binding owner can only be inserted into one "
                  "lookup");
    decltype(auto) key_arg = key_resolver(type_list_iterator<Head>{});
    auto handle = insert_lookup<TypeInterface, TypeStorage, KeyIdType, Head>(
        state, value_factory, key_arg);
    try {
      commit_lookup<TypeInterface, TypeStorage, KeyIdType>(
          state, value_factory, std::forward<KeyResolver>(key_resolver),
          type_list<Tail...>{});
    } catch (...) {
      state.lookup_indexes.template get<Head>().erase(handle);
      throw;
    }
  }

  template <typename Owner> struct unique_lookup_value_factory {
    static constexpr bool shared_owner = false;
    std::unique_ptr<Owner, runtime_binding_value_deleter> owner;

    template <typename TypeInterface> runtime_lookup_value make() {
      auto view = owner->template view<TypeInterface>();
      typename runtime_lookup_value::unique_owner erased(std::move(owner));
      return runtime_lookup_value(std::move(erased), view);
    }
  };

  template <typename Owner> struct shared_lookup_value_factory {
    static constexpr bool shared_owner = true;
    std::shared_ptr<Owner> owner;

    template <typename TypeInterface> runtime_lookup_value make() {
      typename runtime_lookup_value::shared_owner erased = owner;
      return runtime_lookup_value(erased,
                                  owner->template view<TypeInterface>());
    }
  };

  template <typename Owner, typename... Args>
  std::unique_ptr<Owner, runtime_binding_value_deleter>
  make_allocated_binding_value(Args &&...args) {
    using owner_type = Owner;
    auto alloc = allocator_traits::rebind<owner_type>(get_allocator());
    auto *instance = allocator_traits::allocate(alloc, 1);
    try {
      allocator_traits::construct(alloc, instance, std::forward<Args>(args)...);
    } catch (...) {
      allocator_traits::deallocate(alloc, instance, 1);
      throw;
    }
    return std::unique_ptr<Owner, runtime_binding_value_deleter>(
        instance,
        runtime_binding_value_deleter{std::addressof(get_allocator())});
  }

  template <bool SharedOwner, typename Owner, typename... Args>
  auto make_binding_owner(Args &&...args) {
    auto owner =
        make_allocated_binding_value<Owner>(std::forward<Args>(args)...);
    if constexpr (SharedOwner) {
      return std::shared_ptr<Owner>(std::move(owner));
    } else {
      return owner;
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename KeyDomain,
            typename Cardinality, typename KeyIdType, typename ValueFactory>
  void commit_static_lookup(runtime_bindings_state &state,
                            ValueFactory &value_factory) {
    using explicit_lookup_entry =
        explicit_static_key_lookup_entry_t<TypeInterface, KeyDomain,
                                           Cardinality>;
    using route_id = typename static_key_route_id<KeyDomain>::type;
    using route = lookup_index_route<TypeInterface, route_id>;
    using routed_lookup_entry = typename route::entry;
    using routed_cardinality =
        typename lookup_entry_cardinality<routed_lookup_entry>::type;
    if constexpr (!std::is_void_v<explicit_lookup_entry> ||
                  std::is_same_v<Cardinality, ::dingo::one> ||
                  std::is_same_v<routed_cardinality, Cardinality>) {
      static_assert(!std::is_void_v<routed_lookup_entry>);
      static_assert(lookup_entry_indexed_v<routed_lookup_entry>);
      auto route_key = route_id{};
      auto key_resolver = [&](auto) { return route::key(route_key); };
      commit_lookup<TypeInterface, TypeStorage, KeyIdType>(
          state, value_factory, key_resolver, type_list<routed_lookup_entry>{});
    } else {
      static_assert(std::is_same_v<Cardinality, ::dingo::one>,
                    "static-key many lookup registrations require an explicit "
                    "dingo lookup definition");
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename KeyIdType,
            typename ValueFactory, typename... KeyValueArgs>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  void commit_registration(runtime_bindings_state &state,
                           ValueFactory &value_factory, KeyIdType,
                           KeyValueArgs &&...key_values) {
    check_interface_requirements<TypeStorage,
                                 typename annotated_traits<TypeInterface>::type,
                                 typename TypeStorage::type>();
    ((void)key_values, ...);

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
        commit_lookup<TypeInterface, TypeStorage, KeyIdType>(
            state, value_factory, key_resolver, lookup_entries{});
      } else if constexpr (is_none_v<std::decay_t<KeyIdType>>) {
        using lookup_cardinality_type =
            no_key_lookup_cardinality_t<TypeInterface>;
        commit_static_lookup<TypeInterface, TypeStorage, ::dingo::none_t,
                             lookup_cardinality_type, KeyIdType>(state,
                                                                 value_factory);
      } else {
        using typed_key_type = typename std::decay_t<KeyIdType>::type;
        using lookup_cardinality_type =
            typed_key_lookup_cardinality_t<TypeInterface, typed_key_type>;
        commit_static_lookup<TypeInterface, TypeStorage,
                             ::dingo::key_type<typed_key_type>,
                             lookup_cardinality_type, KeyIdType>(state,
                                                                 value_factory);
      }
    } else if constexpr (is_none_v<std::decay_t<KeyIdType>>) {
      using lookup_cardinality_type =
          no_key_lookup_cardinality_t<TypeInterface>;
      commit_static_lookup<TypeInterface, TypeStorage, ::dingo::none_t,
                           lookup_cardinality_type, KeyIdType>(state,
                                                               value_factory);
    } else {
      using typed_key_type = typename std::decay_t<KeyIdType>::type;
      using lookup_cardinality_type =
          typed_key_lookup_cardinality_t<TypeInterface, typed_key_type>;
      commit_static_lookup<TypeInterface, TypeStorage,
                           ::dingo::key_type<typed_key_type>,
                           lookup_cardinality_type, KeyIdType>(state,
                                                               value_factory);
    }
  }

  template <bool SharedOwner, typename TypeInterface, typename TypeStorage,
            typename Owner, typename Parent, typename KeyIdType,
            typename KeyValueTuple, typename... Args>
  container_proxy<Owner> commit_binding(Parent *parent, KeyIdType,
                                        KeyValueTuple &&key_values,
                                        Args &&...args) {
    auto &state = runtime_bindings_;
    auto binding_owner = make_binding_owner<SharedOwner, Owner>(
        parent, std::forward<Args>(args)...);
    auto *binding_result = binding_owner.get();
    if constexpr (SharedOwner) {
      shared_lookup_value_factory<Owner> value_factory{
          std::move(binding_owner)};
      std::apply(
          [&](auto &&...key_value_args) {
            commit_registration<TypeInterface, TypeStorage, KeyIdType>(
                state, value_factory, KeyIdType{},
                std::forward<decltype(key_value_args)>(key_value_args)...);
          },
          std::forward<KeyValueTuple>(key_values));
    } else {
      unique_lookup_value_factory<Owner> value_factory{
          std::move(binding_owner)};
      std::apply(
          [&](auto &&...key_value_args) {
            commit_registration<TypeInterface, TypeStorage, KeyIdType>(
                state, value_factory, KeyIdType{},
                std::forward<decltype(key_value_args)>(key_value_args)...);
          },
          std::forward<KeyValueTuple>(key_values));
    }
    return container_proxy<Owner>(binding_result);
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  template <typename T, bool RemoveRvalueReferences, bool MayAutoConstruct,
            typename IdType = none_t,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve_impl(runtime_context &context, IdType &&id = IdType()) {
    using Type = normalized_type_t<T>;
    static_assert(!std::is_const_v<Type>);

    selected_runtime_binding<T, IdType> selected{*this, id};
    missing_runtime_binding<T, RemoveRvalueReferences, MayAutoConstruct, IdType>
        missing{*this, id};
    auto sources = detail::make_selected_binding_sources(selected, missing);
    return detail::resolve_from_binding_sources<T, R>(context, sources);
  }

  template <typename T>
  decltype(auto) auto_construct(runtime_context &context) {
    using Type = normalized_type_t<T>;

    static_assert(is_complete<Type>::value,
                  "auto-construction requires a complete type");

    using type_detection = detail::automatic;
    return context.template construct_temporary<dependency_interface_t<T>,
                                                type_detection>(
        *resolve_root());
  }

  template <typename T, bool RemoveRvalueReferences, typename IdType = none_t,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve(runtime_context &context, IdType &&id = IdType()) {
    return resolve_impl < T, RemoveRvalueReferences,
           std::is_same_v<dependency_value_t<T>, std::decay_t<T>> &&
               (!std::is_reference_v<T> ||
                (std::is_lvalue_reference_v<T> &&
                 std::is_const_v<std::remove_reference_t<T>> &&
                 is_auto_constructible<std::decay_t<T>>::value)) >
                   (context, std::forward<IdType>(id));
  }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

  template <typename Request, typename Result, typename Binding,
            typename Context>
  Result resolve(Binding &binding, Context &context) {
    return ::dingo::resolve_binding_request<Request, rtti_type>(binding,
                                                                context);
  }

  template <typename T, typename Binding, typename Context>
  T resolve_collection_type(Binding &binding, Context &context) {
    return ::dingo::resolve_binding_request<T, rtti_type>(binding, context);
  }

  template <class Storage, class TypeInterface, class Type>
  void check_interface_requirements() {
    detail::interface_registration_requirements<Storage, TypeInterface,
                                                Type>::assert_valid();
  }

  resolve_root_type *resolve_root_ = nullptr;

  runtime_bindings_state runtime_bindings_;
};

} // namespace dingo
