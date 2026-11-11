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
#include <dingo/binding/request_composition.h>
#include <dingo/binding/request_derivation.h>
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
struct local_binding_scope_slot {
  static constexpr bool has_local_bindings = false;
};

template <typename Registration, typename... LocalRegistrations>
struct local_binding_scope_slot<Registration,
                                static_registry<LocalRegistrations...>> {
  static constexpr bool has_local_bindings = true;
  static_bindings_state<LocalRegistrations...> scope;
};

// What a resolution of a binding that keeps nothing, or in a runtime
// transaction, orders: nothing.
struct no_resolution_order {};

// The order in which a binding that keeps what it resolves was first
// resolved, which a failed operation resets it in: the last first, so that
// what refers to an instance is reset before the instance. Zero while the
// binding is unresolved. Only a number is kept, no address, so that a
// resolution which cannot throw still folds.
template <bool Keeps> struct resolved_order {
  static constexpr bool is_tracked = false;
};

template <> struct resolved_order<true> {
  static constexpr bool is_tracked = true;
  std::size_t resolved_order_ = 0;
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

// What a static binding tells the derivation of a request it does not
// publish: whether it keeps its instance, whether a reference may refer to a
// value it makes, and whether it publishes the pointer it makes that the
// request refers into or takes a value out of.
template <typename Request, typename InterfaceBinding>
constexpr derivation_facts static_derivation_facts() {
  using storage = typename InterfaceBinding::binding_model_type::storage_type;
  derivation_facts facts{binding_cell<storage>::keeps_conversions,
                         places_made_values_v<storage>, false, false};
  if constexpr (std::is_lvalue_reference_v<Request> &&
                adopts_made_pointer_v<storage,
                                      std::remove_reference_t<Request>>) {
    facts.adopts = binding_supports_request_v<
        std::add_pointer_t<std::remove_reference_t<Request>>, InterfaceBinding>;
  }
  if constexpr (takes_shape<Request>() && takes_made_pointee_v<storage>) {
    facts.takes =
        binding_supports_request_v<remove_cvref_t<Request> *, InterfaceBinding>;
  }
  return facts;
}

// The derivation of a request a static binding does not publish.
template <typename Request, typename InterfaceBinding>
inline constexpr derivation static_derivation_v = derive_request<Request>(
    static_derivation_facts<Request, InterfaceBinding>());

// Whether a binding resolves a request: one it publishes, or one it derives
// from a request it resolves.
template <typename Request, typename InterfaceBinding>
constexpr bool resolves_request() {
  if constexpr (binding_supports_request_v<Request, InterfaceBinding>) {
    return true;
  } else {
    constexpr derivation how = static_derivation_v<Request, InterfaceBinding>;
    if constexpr (how == derivation::none) {
      return false;
    } else {
      return resolves_request<derived_request_t<how, Request>,
                              InterfaceBinding>();
    }
  }
}

template <typename Request, typename InterfaceBinding>
inline constexpr bool resolves_request_v =
    resolves_request<Request, InterfaceBinding>();

// How a binding resolves a request it does not publish, if it resolves it.
template <typename Request, typename InterfaceBinding>
inline constexpr derivation static_request_derivation_v =
    binding_supports_request_v<Request, InterfaceBinding> ||
            !resolves_request_v<Request, InterfaceBinding>
        ? derivation::none
        : static_derivation_v<Request, InterfaceBinding>;

// The resolution of a request selected at compile time.
template <typename Request, typename InterfaceBinding, typename Context>
struct static_request_resolution
    : binding_supports_request<Request, InterfaceBinding> {
private:
  static constexpr bool resolves =
      is_runtime_context_v<Context> ||
      resolves_request_v<Request, InterfaceBinding>;
  // A request that does not resolve because it would slice the registered
  // type is told so.
  static constexpr bool slices =
      slices_v<Request,
               leaf_type_t<typename InterfaceBinding::storage_type::type>>;

  static_assert(resolves || !slices,
                "a value of a base of the registered type would slice it; "
                "request the base by reference, by pointer or by a handle");
  static_assert(resolves || slices,
                "static resolution cannot satisfy a request the storage "
                "does not publish");
};

// One binding of a static container: the cell of its registration, the state
// of the local bindings the registration declares, and the resolution of a
// request through one of its interfaces.
template <typename Registration>
class static_binding
    : private local_binding_scope_slot<Registration>,
      private resolved_order<storage_can_reset<
          typename binding_model<Registration>::storage_type>::value> {
  using model = binding_model<Registration>;
  static constexpr bool keeps =
      storage_can_reset<typename model::storage_type>::value;

public:
  // Resets the binding the static container resolved as the order-th, and
  // those its registration binds locally.
  void reset_resolved([[maybe_unused]] std::size_t order) noexcept {
    if constexpr (keeps) {
      if (this->resolved_order_ == order) {
        this->resolved_order_ = 0;
        cell_.reset();
      }
    }
    if constexpr (local_binding_scope_slot<Registration>::has_local_bindings) {
      this->scope.reset_resolved(order);
    }
  }

  // Selects the resolution of a request at compile time and applies it
  // through a binding activation.
  template <typename InterfaceBinding, typename Request, typename Host,
            typename Context, typename Scope>
  Request resolve(Scope scope, Host &host, Context &context) {
    using resolution =
        typename static_request_resolution<Request, InterfaceBinding,
                                           Context>::type;
    constexpr derivation how =
        static_request_derivation_v<Request, InterfaceBinding>;
    if constexpr (how != derivation::none) {
      return derive<InterfaceBinding, Request, how>(scope, host, context);
    } else if constexpr (std::is_void_v<resolution>) {
      not_convertible<Request, InterfaceBinding>(context);
    } else if constexpr (!std::is_reference_v<Request> &&
                         !std::is_pointer_v<Request>) {
      [[maybe_unused]] auto order = ordered(host, context);
      return activation(scope, host)
          .template resolve_request_value<Request, resolution>(context);
    } else {
      [[maybe_unused]] auto order = ordered(host, context);
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
      [[maybe_unused]] auto order = ordered(host, context);
      return activation(scope, host)
          .template consume_request<Request, resolution>(context,
                                                         std::forward<Fn>(fn));
    }
  }

private:
  // Derives a request the binding does not publish from the request it
  // resolves through the binding, as the runtime container does.
  template <typename InterfaceBinding, typename Request, derivation How,
            typename Host, typename Context, typename Scope>
  Request derive(Scope scope, Host &host, Context &context) {
    if constexpr (How == derivation::compose && !std::is_reference_v<Request>) {
      return Request(compose<InterfaceBinding, Request>(scope, host, context));
    } else if constexpr (How == derivation::compose) {
      // What an rvalue request refers to is placed in the context.
      using wrapper = composed_type_t<Request>;
      return static_cast<Request>(
          context.template construct_from<wrapper>(scope, [&]() -> wrapper {
            return compose<InterfaceBinding, Request>(scope, host, context);
          }));
    } else {
      auto composes = composes_request<InterfaceBinding>();
      return convert_resolved_binding<Request>(apply_derivation<How, Request>(
          scope, context,
          described_type_v<typename InterfaceBinding::storage_type::type>,
          [&](auto tag) -> typename decltype(tag)::type {
            return resolve<InterfaceBinding, typename decltype(tag)::type>(
                scope, host, context);
          },
          composes, [&]() -> auto & { return cell_.retained(); },
          [](void *) {}));
    }
  }

  // Whether the binding composes a request, known as the code compiles.
  template <typename InterfaceBinding> static auto composes_request() {
    return [](auto tag) {
      return std::bool_constant<static_request_derivation_v<
                                    typename decltype(tag)::type,
                                    InterfaceBinding> == derivation::compose>{};
    };
  }

  // Composes a wrapper of what the binding resolves, as the runtime
  // container does, returned where it is requested: a value the binding
  // publishes is made into it while its source exists.
  template <typename InterfaceBinding, typename Request, typename Host,
            typename Context, typename Scope>
  composed_type_t<Request> compose(Scope scope, Host &host, Context &context) {
    using wrapper = composed_type_t<Request>;
    auto composes = composes_request<InterfaceBinding>();
    auto with = [&](auto tag, auto &make) -> wrapper {
      using candidate = typename decltype(tag)::type;
      if constexpr (static_request_derivation_v<candidate, InterfaceBinding> ==
                    derivation::none) {
        return consume<InterfaceBinding, candidate>(
            scope, host, context, [&](auto &&instance) -> wrapper {
              using instance_type = decltype(instance);
              if constexpr (std::is_same_v<remove_cvref_t<instance_type>,
                                           remove_cvref_t<candidate>>) {
                return make(std::forward<instance_type>(instance));
              } else {
                return make(static_cast<candidate>(
                    std::forward<instance_type>(instance)));
              }
            });
      } else {
        return make(resolve<InterfaceBinding, candidate>(scope, host, context));
      }
    };
    return compose_wrapper<wrapper,
                           derived_request_t<derivation::compose, Request>>(
        composes, with);
  }

  // A resolution of the cell, which takes the next order of the static
  // container when it resolves the cell first, once the cell is resolved:
  // after what the cell depends on. A runtime transaction resets what it
  // resolves itself.
  template <typename Host> class resolution_order {
  public:
    resolution_order(static_binding &binding, Host &host)
        : binding_(binding), host_(host),
          first_(!binding.cell_.storage().is_resolved()) {}
    ~resolution_order() {
      if (first_ && binding_.cell_.storage().is_resolved() &&
          binding_.resolved_order_ == 0) {
        binding_.resolved_order_ = host_.next_resolved_order();
      }
    }

    resolution_order(const resolution_order &) = delete;
    resolution_order &operator=(const resolution_order &) = delete;

  private:
    static_binding &binding_;
    Host &host_;
    bool first_;
  };

  template <typename Host, typename Context>
  auto ordered(Host &host, Context &) {
    if constexpr (keeps && !is_runtime_context_v<Context>) {
      return resolution_order<Host>(*this, host);
    } else {
      return no_resolution_order{};
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
  // Resets the binding resolved as the order-th, of these or of what they
  // bind locally.
  void reset_resolved([[maybe_unused]] std::size_t order) noexcept {
    (static_cast<static_binding<Registrations> &>(*this).reset_resolved(order),
     ...);
  }

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
