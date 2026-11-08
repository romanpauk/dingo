//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/binding/binding_activation.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_model.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/context/resolution_context.h>
#include <dingo/factory/factory_traits.h>
#include <dingo/lookup/lookup_key.h>
#include <dingo/resolution/cache.h>
#include <dingo/static/graph.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

template <typename... Registrations> class static_bindings_state;

template <typename BindingModel>
struct binding_factory_is_default_constructible
    : std::bool_constant<std::is_default_constructible_v<
          typename BindingModel::factory_type>> {};

template <typename BindingModel>
struct binding_storage_is_default_constructible
    : std::bool_constant<std::is_default_constructible_v<
          typename BindingModel::storage_type>> {};

template <typename Registration,
          typename BindingsType =
              typename detail::binding_model<Registration>::bindings_type>
struct local_binding_scope_slot {};

template <typename Registration, typename... LocalRegistrations>
struct local_binding_scope_slot<Registration,
                                static_registry<LocalRegistrations...>> {
  static_bindings_state<LocalRegistrations...> scope;
};

template <typename Request, typename Resolutions>
struct matching_binding_resolution;

template <typename Request>
struct matching_binding_resolution<Request, type_list<>> {
  using type = void;
};

template <typename Request, typename Head, typename... Tail>
struct matching_binding_resolution<Request, type_list<Head, Tail...>> {
private:
  using unwrapped_request = unannotated_t<Request>;

public:
  using type = std::conditional_t<
      is_resolution_request_for_v<Head, unwrapped_request>, Head,
      typename matching_binding_resolution<Request, type_list<Tail...>>::type>;
};

// Resolution target shapes are disjoint, so static lookup only needs to form
// the category that can match the request.
template <typename RequestType>
inline constexpr request_category static_request_category_v =
    std::is_lvalue_reference_v<RequestType> ? request_category::lvalue_reference
    : std::is_rvalue_reference_v<RequestType>
        ? request_category::rvalue_reference
    : std::is_pointer_v<RequestType> ? request_category::pointer
                                     : request_category::value;

template <typename Request, typename Interface, typename Storage>
struct request_binding_resolutions {
  using type = category_resolutions_t<
      static_request_category_v<std::remove_cv_t<unannotated_t<Request>>>,
      Interface, storage_shape_t<Storage>>;
};

template <typename Request, typename InterfaceBinding>
struct binding_supports_request {
private:
  using binding_model_type = typename InterfaceBinding::binding_model_type;
  using interface_type = typename InterfaceBinding::interface_type;
  using raw_interface_type = unannotated_t<interface_type>;
  using storage_type = typename binding_model_type::storage_type;
  using resolutions =
      typename request_binding_resolutions<Request, raw_interface_type,
                                           storage_type>::type;

public:
  using type = typename matching_binding_resolution<Request, resolutions>::type;

  static constexpr bool value = !std::is_void_v<type>;
};

template <typename Request, typename InterfaceBinding>
inline constexpr bool binding_supports_request_v =
    binding_supports_request<Request, InterfaceBinding>::value;

// The dependencies of a static binding resolve through its local bindings if
// they bind the request, otherwise through the host.
template <typename LocalStorage, typename LocalBindings, typename Host>
struct local_binding_router {
  LocalStorage &local;
  Host &host;

  template <typename T, bool RemoveRvalueReferences, typename Context,
            typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0,
            typename Scope>
  decltype(auto) resolve(Scope scope, Context &context, LookupKey key) {
    using request = request_type<T, RemoveRvalueReferences>;
    using R = typename request::interface_type;
    constexpr bool collection = collection_traits<R>::is_collection;
    constexpr bool bound_locally = [] {
      if constexpr (collection) {
        return static_collection_binding_count<LocalBindings, R, LookupKey>() !=
               0;
      } else {
        return LocalBindings::template selection<R, LookupKey>::status ==
               binding_status::found;
      }
    }();
    if constexpr (!bound_locally) {
      return host.template resolve<typename request::user_type,
                                   request::removes_rvalue_references>(
          scope, context, std::move(key));
    } else if constexpr (collection) {
      return local.template construct_collection<R, LookupKey, LocalBindings>(
          scope, host, context);
    } else {
      return local.template resolve<typename LocalBindings::template selection<
                                        R, LookupKey>::binding_type,
                                    R>(scope, host, context);
    }
  }
};

template <typename Host, typename LocalBindings> struct static_dependencies {
  using type = Host &;
};

template <typename Host, typename... LocalRegistrations>
struct static_dependencies<Host, static_registry<LocalRegistrations...>> {
  using type =
      local_binding_router<static_bindings_state<LocalRegistrations...>,
                           static_registry<LocalRegistrations...>, Host>;
};

// A reference or a pointer request of a value the storage makes for the
// request, which it publishes no address of: the value is placed in the
// context. A copy is not the instance a reference refers to, so the storage
// moves its instance into the value, as one that does not share it does.
template <typename Request, typename InterfaceBinding>
inline constexpr bool places_request_value_v = [] {
  using storage = typename InterfaceBinding::binding_model_type::storage_type;
  if constexpr ((std::is_lvalue_reference_v<Request> ||
                 std::is_pointer_v<Request>) &&
                places_made_values_v<storage>) {
    return !binding_supports_request_v<Request, InterfaceBinding> &&
           binding_supports_request_v<referred_value_t<Request>,
                                      InterfaceBinding>;
  } else {
    return false;
  }
}();

// A reference request of a storage that hands over a pointer it makes for the
// request: the request is resolved as the pointer, which an owner placed in
// the context adopts.
template <typename Request, typename InterfaceBinding>
inline constexpr bool adopts_request_pointer_v = [] {
  using storage = typename InterfaceBinding::binding_model_type::storage_type;
  if constexpr (std::is_lvalue_reference_v<Request> &&
                adopts_made_pointer_v<storage,
                                      std::remove_reference_t<Request>>) {
    return !binding_supports_request_v<Request, InterfaceBinding> &&
           !places_request_value_v<Request, InterfaceBinding> &&
           binding_supports_request_v<
               std::add_pointer_t<std::remove_reference_t<Request>>,
               InterfaceBinding>;
  } else {
    return false;
  }
}();

// The resolution of a request selected at compile time.
template <typename Request, typename InterfaceBinding, typename Context>
struct static_request_resolution
    : binding_supports_request<Request, InterfaceBinding> {
  static_assert(is_runtime_context_v<Context> ||
                    binding_supports_request_v<Request, InterfaceBinding> ||
                    places_request_value_v<Request, InterfaceBinding> ||
                    adopts_request_pointer_v<Request, InterfaceBinding>,
                "static resolution cannot satisfy a request the storage "
                "does not publish");
};

// One binding of a static container: the cell of its registration, the state
// of the local bindings the registration declares, and the resolution of a
// request through one of its interfaces.
template <typename Registration>
class static_binding : private local_binding_scope_slot<Registration> {
  using model = binding_model<Registration>;

public:
  // Selects the resolution of a request at compile time and applies it
  // through a binding activation.
  template <typename InterfaceBinding, typename Request, typename Host,
            typename Context, typename Scope>
  Request resolve(Scope scope, Host &host, Context &context) {
    using resolution =
        typename static_request_resolution<Request, InterfaceBinding,
                                           Context>::type;
    if constexpr (places_request_value_v<Request, InterfaceBinding>) {
      return place_value<InterfaceBinding, Request>(scope, host, context);
    } else if constexpr (adopts_request_pointer_v<Request, InterfaceBinding>) {
      return adopt_pointer<InterfaceBinding, Request>(scope, host, context);
    } else if constexpr (std::is_void_v<resolution>) {
      not_convertible<Request, InterfaceBinding>(context);
    } else if constexpr (!std::is_reference_v<Request> &&
                         !std::is_pointer_v<Request>) {
      return consume<InterfaceBinding, Request>(
          scope, host, context, [](auto &&instance) -> Request {
            return Request(std::forward<decltype(instance)>(instance));
          });
    } else {
      return convert_resolved_binding<Request>(
          activation(scope, host)
              .template resolve_request_address<Request, resolution>(context));
    }
  }

  template <typename InterfaceBinding, typename Request, typename Host,
            typename Context, typename Fn, typename Scope>
  decltype(auto) consume(Scope scope, Host &host, Context &context, Fn &&fn) {
    using resolution =
        typename static_request_resolution<Request, InterfaceBinding,
                                           Context>::type;
    if constexpr (std::is_void_v<resolution>) {
      not_convertible<Request, InterfaceBinding>(context);
    } else {
      return activation(scope, host)
          .template consume_request<Request, resolution>(context,
                                                         std::forward<Fn>(fn));
    }
  }

private:
  // Places the value a reference or a pointer request refers to in the
  // context, which ends before the caller uses what a resolution returns.
  template <typename InterfaceBinding, typename Request, typename Host,
            typename Context, typename Scope>
  Request place_value(Scope scope, Host &host, Context &context) {
    using value_type = referred_value_t<Request>;
    refuse_caller<InterfaceBinding, Request>(scope, context);
    auto &value =
        context.template construct_from<value_type>(scope, [&]() -> value_type {
          return resolve<InterfaceBinding, value_type>(scope, host, context);
        });
    if constexpr (std::is_pointer_v<Request>) {
      return &value;
    } else {
      return value;
    }
  }

  // Resolves a reference request as the pointer the storage hands over, and
  // places an owner of the pointer in the context.
  template <typename InterfaceBinding, typename Request, typename Host,
            typename Context, typename Scope>
  Request adopt_pointer(Scope scope, Host &host, Context &context) {
    using pointer_type = std::add_pointer_t<std::remove_reference_t<Request>>;
    refuse_caller<InterfaceBinding, Request>(scope, context);
    pointer_type pointer =
        resolve<InterfaceBinding, pointer_type>(scope, host, context);
    place_adopted_pointer(scope, context, pointer);
    return *pointer;
  }

  // What is placed in the context ends before the caller uses what a
  // resolution returns: a request of the caller is refused.
  template <typename InterfaceBinding, typename Request, typename Context,
            typename Scope>
  static void refuse_caller(Scope scope, Context &context) {
    if constexpr (is_fixed_caller_scope_v<Scope>) {
      static_assert(!is_fixed_caller_scope_v<Scope>,
                    "a reference or a pointer to an instance the "
                    "resolution makes would dangle in what is returned to "
                    "the caller; take it by value, or let a shared scope "
                    "keep what refers to it");
    } else if (scope.is_caller()) {
      not_convertible<Request, InterfaceBinding>(context);
    }
  }

  template <typename Host, typename Scope>
  auto activation(Scope scope, Host &host) {
    using dependencies =
        typename static_dependencies<Host, typename model::bindings_type>::type;
    if constexpr (std::is_reference_v<dependencies>) {
      return binding_activation<binding_cell_t<model>, dependencies, Scope>{
          cell_, host, scope};
    } else {
      return binding_activation<binding_cell_t<model>, dependencies, Scope>{
          cell_, {this->scope, host}, scope};
    }
  }

  template <typename Request, typename InterfaceBinding, typename Context>
  [[noreturn]] static void not_convertible(Context &context) {
    throw_type_not_convertible(
        detail::described_type_v<Request>,
        detail::described_type_v<typename InterfaceBinding::storage_type::type>,
        context);
  }

  binding_cell_t<model> cell_;
};

// The bindings of a static container, one per registration, and the
// collections they make up.
template <typename... Registrations>
class static_bindings_state : private static_binding<Registrations>... {
public:
  template <typename InterfaceBinding, typename Request, typename Host,
            typename Context, typename Scope>
  Request resolve(Scope scope, Host &host, Context &context) {
    return binding<InterfaceBinding>()
        .template resolve<InterfaceBinding, Request>(scope, host, context);
  }

  template <typename InterfaceBinding, typename Request, typename Host,
            typename Context, typename Fn, typename Scope>
  decltype(auto) consume(Scope scope, Host &host, Context &context, Fn &&fn) {
    return binding<InterfaceBinding>()
        .template consume<InterfaceBinding, Request>(scope, host, context,
                                                     std::forward<Fn>(fn));
  }

  template <typename T, typename LookupKey, typename Bindings, typename Host,
            typename Context, typename Fn, typename Scope>
  std::size_t append_collection(Scope scope, T &results, Host &host,
                                Context &context, Fn &&fn) {
    static_assert(is_lookup_key_v<LookupKey>);
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using interface_bindings =
        typename Bindings::template bindings<normalized_type_t<resolve_type>,
                                             LookupKey>;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");

    dingo::for_each(interface_bindings{}, [&](auto binding_iterator) {
      using binding = typename decltype(binding_iterator)::type;
      if constexpr (is_copy_constructible_v<resolve_type> &&
                    !std::is_reference_v<resolve_type>) {
        consume<binding, resolve_type>(scope, host, context, [&](auto &&value) {
          fn(results, std::forward<decltype(value)>(value));
        });
      } else {
        fn(results, resolve<binding, resolve_type>(scope, host, context));
      }
    });
    return type_list_size_v<interface_bindings>;
  }

  template <typename T, typename LookupKey, typename Bindings, typename Host,
            typename Context, typename Fn, typename Scope>
  T construct_collection(Scope scope, Host &host, Context &context, Fn &&fn) {
    constexpr std::size_t count =
        static_collection_binding_count<Bindings, T, LookupKey>();
    static_assert(
        count != 0,
        "static container cannot construct a collection for an unbound type");

    T results;
    collection_traits<T>::reserve(results, count);
    append_collection<T, LookupKey, Bindings>(scope, results, host, context,
                                              std::forward<Fn>(fn));
    return results;
  }

  template <typename T, typename LookupKey, typename Bindings, typename Host,
            typename Context, typename Scope>
  T construct_collection(Scope scope, Host &host, Context &context) {
    using collection_type = collection_traits<T>;
    constexpr std::size_t count =
        static_collection_binding_count<Bindings, T, LookupKey>();
    static_assert(
        count != 0,
        "static container cannot construct a collection for an unbound type");

    if constexpr (collection_type::has_fixed_size_construct) {
      T results = collection_type::make_fixed_size(count);
      std::size_t index = 0;
      append_collection<T, LookupKey, Bindings>(
          scope, results, host, context, [&](auto &, auto &&value) {
            collection_type::set(results, index,
                                 std::forward<decltype(value)>(value));
            ++index;
          });
      return results;
    } else {
      return construct_collection<T, LookupKey, Bindings>(
          scope, host, context, [](auto &collection, auto &&value) {
            collection_type::add(collection,
                                 std::forward<decltype(value)>(value));
          });
    }
  }

private:
  template <typename InterfaceBinding> auto &binding() {
    return static_cast<static_binding<
        typename InterfaceBinding::binding_model_type::registration_type> &>(
        *this);
  }
};
} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
