//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/binding/activation.h>
#include <dingo/binding/cache.h>
#include <dingo/binding/model.h>
#include <dingo/binding/resolution.h>
#include <dingo/context/resolution.h>
#include <dingo/factory/factory_traits.h>
#include <dingo/lookup/lookup_key.h>
#include <dingo/request/collection.h>
#include <dingo/request/execution.h>
#include <dingo/static/graph.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {
// Whether a value of Target made of Source would slice it: Target is a base
// of Source, other than Source itself.
template <typename Target, typename Source>
inline constexpr bool slices_v =
    !std::is_reference_v<Target> &&
    std::is_base_of_v<std::remove_cv_t<Target>,
                      std::remove_cv_t<std::remove_reference_t<Source>>> &&
    !std::is_same_v<std::remove_cv_t<Target>,
                    std::remove_cv_t<std::remove_reference_t<Source>>>;

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
  static_bindings_state<LocalRegistrations...> local_bindings;
};

// What a resolution of a binding that keeps nothing orders: nothing.
struct no_resolution_order {};

// The order in which a binding that keeps what it resolves was first
// resolved, which a failed operation resets it in: the last first, so that
// what refers to an instance is reset before the instance. Zero while the
// binding is unresolved. Only a number is kept, no address, so that a
// resolution which cannot throw still folds.
template <bool Keeps> struct resolved_order {};

template <> struct resolved_order<true> {
  std::size_t resolved_order_ = 0;
};

// The dependencies of a static binding resolve through its local bindings if
// they bind the request, otherwise through the host.
template <typename LocalStorage, typename Host> struct local_binding_router {
  using LocalBindings = typename LocalStorage::registry_type;
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
      if constexpr (collection_traits<R>::is_collection) {
        return static_collection_binding_count<LocalBindings, R, LookupKey>() !=
               0;
      } else {
        // A level of local bindings answers what it binds, also ambiguously,
        // by the rule of the runtime container.
        return !falls_back(
            LocalBindings::template selection<R, LookupKey>::status);
      }
    }();
    if constexpr (!bound_locally) {
      return host.template resolve<typename request::user_type,
                                   request::removes_rvalue_references>(
          scope, context, std::move(key));
    } else if constexpr (collection) {
      return local.template construct_collection<R, LookupKey>(scope, host,
                                                               context);
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
      local_binding_router<static_bindings_state<LocalRegistrations...>, Host>;
};

// The plan of a request of a static binding, as the code compiles: of the
// stored object its storage gives, borrowed or made for the request, and of
// the request. A request needs what it refers to to live as long as the
// container when it is passed to the caller.
template <typename Request, typename InterfaceBinding> struct static_execution {
  using storage_type = typename InterfaceBinding::storage_type;
  using binding = binding_execution<
      storage_type, unannotated_t<typename InterfaceBinding::interface_type>>;
  using leaf_type = typename binding::leaf_type;
  static constexpr semantics::scope_traits scope = binding::scope;
  using stored_type = typename binding::stored_type;
  using plan = execution::request_plan<stored_type, Request, leaf_type,
                                       scope.makes, scope.publishes_early>;
  using interface_type = typename plan::interface_type;

  template <bool Stable> static constexpr const semantics::plan &planned() {
    if constexpr (Stable) {
      return plan::stable;
    } else {
      return plan::transient;
    }
  }

  // Whether the binding resolves the request at all, for some lifetime.
  static constexpr bool resolves = plan::transient.found || plan::stable.found;
};

// Whether a request of a binding refers to the stored object itself, as
// the code compiles: no step makes anything of it.
template <typename Request, typename InterfaceBinding>
constexpr bool refers_stored() {
  constexpr const semantics::plan &plan =
      static_execution<Request, InterfaceBinding>::plan::stable;
  return plan.found && plan.count == 0 &&
         plan.how == semantics::satisfaction::refer;
}

// Whether an rvalue request of a binding is of a value the binding makes
// for the request, which the value request moves out of where it returns
// it.
template <typename Request, typename InterfaceBinding>
constexpr bool returns_made_value() {
  using value_plan = typename static_execution<std::remove_reference_t<Request>,
                                               InterfaceBinding>::plan;
  return static_execution<Request, InterfaceBinding>::resolves &&
         value_plan::stable.found &&
         value_plan::stable.how == semantics::satisfaction::move;
}

// What a static resolution makes for a request: kept with the binding when
// it lives as long as the container, in the context in the scope of the
// request otherwise.
template <typename Context, typename Scope, typename Retained>
class static_keeper {
public:
  static_keeper(Context &context, Scope scope, Retained &retained,
                void (*reject)())
      : transient_{{context, reject}, scope},
        stable_{{context, reject}, retained} {}

  [[noreturn]] void absent() { transient_.absent(); }

  template <bool Stable> auto &at() {
    if constexpr (Stable) {
      return stable_;
    } else {
      return transient_;
    }
  }

  template <typename T, typename Make> T &keep(Make &&make) {
    return transient_.template keep<T>(std::forward<Make>(make));
  }

private:
  // Rejects a request of what an empty layer does not hold.
  struct rejecting {
    Context &context;
    void (*reject)();
    [[noreturn]] void absent() {
      reject();
      std::abort();
    }
  };
  struct transient : rejecting {
    Scope scope;
    template <typename T, typename Make> T &keep(Make &&make) {
      return this->context.template construct_made<T>(scope,
                                                      std::forward<Make>(make));
    }
  };
  struct stable : rejecting {
    Retained &retained;
    template <typename T, typename Make> T &keep(Make &&make) {
      if constexpr (std::is_same_v<Retained, no_retention>) {
        static_assert(!std::is_same_v<Retained, no_retention>,
                      "a binding that keeps no instance keeps nothing made of "
                      "it");
        return this->context.template construct_made<T>(
            caller_scope, std::forward<Make>(make));
      } else {
        return retained.template keep<T>(&execution::retained_key<T>,
                                         std::forward<Make>(make));
      }
    }
  };
  transient transient_;
  stable stable_;
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
      this->local_bindings.reset_resolved(order);
    }
  }

  // Resolves a request by its plan, from the stored object the activation
  // of the binding materializes. A request the binding does not resolve for
  // the lifetime it needs is rejected: as the code compiles where the
  // lifetime is known then.
  template <typename InterfaceBinding, typename Request, typename Host,
            typename Context, typename Scope>
  Request resolve(Scope scope, Host &host, Context &context) {
    using planning = static_execution<Request, InterfaceBinding>;
    if constexpr (is_fixed_construction_scope<Scope>::value) {
      constexpr bool stable =
          Scope::lifetime_value == construction_lifetime::caller;
      if constexpr (planning::template planned<stable>().found) {
        return run<InterfaceBinding, Request, stable>(scope, host, context);
      } else {
        check_planned<Request, InterfaceBinding, Context, Scope, stable>();
        return not_convertible<Request, InterfaceBinding, Request>(context);
      }
    } else if constexpr (!planning::resolves) {
      check_planned<Request, InterfaceBinding, Context, Scope, false>();
      return not_convertible<Request, InterfaceBinding, Request>(context);
    } else {
      // The lifetime a request needs is known as it runs.
      if constexpr (planning::template planned<true>().found) {
        if (scope.is_caller()) {
          return run<InterfaceBinding, Request, true>(scope, host, context);
        }
      } else {
        if (scope.is_caller()) {
          not_convertible<Request, InterfaceBinding>(context);
        }
      }
      if constexpr (planning::template planned<false>().found) {
        return run<InterfaceBinding, Request, false>(scope, host, context);
      } else {
        return not_convertible<Request, InterfaceBinding, Request>(context);
      }
    }
  }

private:
  // Rejects, as the code compiles, a request a static binding cannot
  // resolve for the lifetime it needs: a reference into what the
  // resolution makes, which the caller would outlive; a value of a base,
  // which would slice the registered type; or another. A context that
  // rejects late rejects it as it runs.
  template <typename Request, typename InterfaceBinding, typename Context,
            typename Scope, bool Stable>
  static constexpr void check_planned() {
    using planning = static_execution<Request, InterfaceBinding>;
    if constexpr (!rejects_late_v<Context> &&
                  !planning::template planned<Stable>().found) {
      if constexpr (Stable && planning::plan::transient.found) {
        static_assert(!is_fixed_caller_scope_v<Scope>,
                      "a reference or a pointer to an instance the resolution "
                      "makes would dangle in what is returned to the caller; "
                      "take it by value, or let a shared scope keep what "
                      "refers to it");
      } else {
        constexpr bool slices = slices_v<
            Request,
            leaf_type_t<typename InterfaceBinding::storage_type::type>>;
        static_assert(!slices,
                      "a value of a base of the registered type would slice "
                      "it; request the base by reference, by pointer or by a "
                      "handle");
        static_assert(slices,
                      "static resolution cannot satisfy a request the storage "
                      "does not publish");
      }
    }
  }

  // Runs the plan of a request of the stored object.
  template <typename InterfaceBinding, typename Request, bool Stable,
            typename Host, typename Context, typename Scope>
  Request run(Scope scope, Host &host, Context &context) {
    using planning = static_execution<Request, InterfaceBinding>;
    [[maybe_unused]] auto order = ordered(host, context);
    static_keeper<Context, Scope,
                  std::remove_reference_t<decltype(cell_.retained())>>
        keeper(context, scope, cell_.retained(),
               &reject_absent<Request, InterfaceBinding>);
    return activation(scope, host)
        .template use_source<Request>(context, [&](auto &&source) -> Request {
          return execution::planned_run<typename planning::plan, Stable>::
              template run<Request>(
                  plan_start<typename planning::stored_type,
                             planning::scope.makes>(
                      std::forward<decltype(source)>(source), keeper),
                  keeper);
        });
  }

  // A resolution of the cell, which takes the next order of the static
  // container when it resolves the cell first, once the cell is resolved:
  // after what the cell depends on.
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

  // A static container resolves its bindings in a static context, also
  // what a runtime child asks it for, so that it resets them itself.
  template <typename Host, typename Context>
  auto ordered(Host &host, Context &) {
    static_assert(!is_runtime_context_v<Context>);
    if constexpr (keeps) {
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
          cell_, {this->local_bindings, host}, scope};
    }
  }

  // Rejects a request of what an empty layer does not hold, as it runs.
  template <typename Request, typename InterfaceBinding>
  [[noreturn]] static void reject_absent() {
    throw_type_not_convertible(
        detail::described_type_v<Request>,
        detail::described_type_v<
            typename InterfaceBinding::storage_type::type>);
  }

  // Rejects a request as it runs; T is the type of what the branch that
  // rejects it does not return.
  template <typename Request, typename InterfaceBinding, typename T = void,
            typename Context>
  [[noreturn]] static T not_convertible(Context &context) {
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
class static_bindings_state : public static_registry<Registrations...>,
                              private static_binding<Registrations>... {
public:
  // The bindings of the state: it is the registry of its registrations, as
  // the selection of a request among them and as the storage of their cells.
  using registry_type = static_registry<Registrations...>;

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

  template <typename T, typename LookupKey, typename Host, typename Context,
            typename Fn, typename Scope>
  std::size_t append_collection(Scope scope, T &results, Host &host,
                                Context &context, Fn &&fn) {
    static_assert(is_lookup_key_v<LookupKey>);
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using interface_bindings = typename registry_type::template bindings<
        normalized_type_t<resolve_type>, LookupKey>;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");

    dingo::for_each(interface_bindings{}, [&](auto binding_iterator) {
      using binding = typename decltype(binding_iterator)::type;
      fn(results, resolve<binding, resolve_type>(scope, host, context));
    });
    return type_list_size_v<interface_bindings>;
  }

  template <typename T, typename LookupKey, typename Host, typename Context,
            typename Fn, typename Scope>
  T construct_collection(Scope scope, Host &host, Context &context, Fn &&fn) {
    constexpr std::size_t count =
        static_collection_binding_count<registry_type, T, LookupKey>();
    static_assert(
        count != 0,
        "static container cannot construct a collection for an unbound type");

    T results;
    collection_traits<T>::reserve(results, count);
    append_collection<T, LookupKey>(scope, results, host, context,
                                    std::forward<Fn>(fn));
    return results;
  }

  template <typename T, typename LookupKey, typename Host, typename Context,
            typename Scope>
  T construct_collection(Scope scope, Host &host, Context &context) {
    using collection_type = collection_traits<T>;
    constexpr std::size_t count =
        static_collection_binding_count<registry_type, T, LookupKey>();
    static_assert(
        count != 0,
        "static container cannot construct a collection for an unbound type");

    if constexpr (collection_type::has_fixed_size_construct) {
      T results = collection_type::make_fixed_size(count);
      std::size_t index = 0;
      append_collection<T, LookupKey>(
          scope, results, host, context, [&](auto &, auto &&value) {
            collection_type::set(results, index,
                                 std::forward<decltype(value)>(value));
            ++index;
          });
      return results;
    } else {
      return construct_collection<T, LookupKey>(
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
