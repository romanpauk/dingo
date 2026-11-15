//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/binding/interface.h>
#include <dingo/binding/selection.h>
#include <dingo/context/resolution.h>
#include <dingo/request/decomposition.h>
#include <dingo/request/execution.h>
#include <dingo/storage/retention.h>
#include <dingo/storage/scope_traits.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/shape.h>
#include <dingo/type/type_traits.h>

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

template <typename T> T resolved_value(resolved_address result) {
  using result_type = std::remove_reference_t<T>;
  using value_type = std::remove_cv_t<result_type>;

  if constexpr (std::is_lvalue_reference_v<T>) {
    return *static_cast<result_type *>(result.address);
  } else if constexpr (std::is_rvalue_reference_v<T>) {
    return std::move(*static_cast<result_type *>(result.address));
  } else if constexpr (std::is_pointer_v<T>) {
    return static_cast<T>(result.address);
  } else if constexpr (std::is_move_constructible_v<value_type> &&
                       is_copy_constructible_v<value_type>) {
    if (result.access == resolved_address::access_kind::consume) {
      return std::move(*static_cast<value_type *>(result.address));
    }
    return *static_cast<value_type *>(result.address);
  } else if constexpr (std::is_move_constructible_v<value_type>) {
    assert(result.access == resolved_address::access_kind::consume);
    return std::move(*static_cast<value_type *>(result.address));
  } else if constexpr (is_copy_constructible_v<value_type>) {
    return *static_cast<value_type *>(result.address);
  } else {
    throw_type_not_convertible(detail::described_type_v<T>,
                               detail::described_type_v<value_type>);
  }
}

template <typename T, typename Instance, typename Fn>
decltype(auto) consume_resolved(Instance &&instance, Fn &&fn) {
  if constexpr (std::is_lvalue_reference_v<T> || std::is_pointer_v<T> ||
                (!std::is_rvalue_reference_v<T> &&
                 !std::is_constructible_v<T, Instance &&>)) {
    return std::forward<Fn>(fn)(instance);
  } else {
    return std::forward<Fn>(fn)(std::forward<Instance>(instance));
  }
}

} // namespace detail

template <typename T, typename RTTI, typename Binding, typename Context>
T resolve_binding(construction_scope scope, Binding &binding, Context &context,
                  const void *cache_key = nullptr);

namespace detail {
// A reference into the context ends before the caller uses what a resolution
// returns: the caller is refused it, as a static container's compiles.
template <typename Request, typename Scope, typename Context>
void reject_escape(Scope scope, type_descriptor registered, Context &context) {
  if constexpr (is_fixed_caller_scope_v<Scope>) {
    static_assert(!is_fixed_caller_scope_v<Scope>,
                  "a reference or a pointer to an instance the resolution "
                  "makes would dangle in what is returned to the caller; "
                  "take it by value, or let a shared scope keep what refers "
                  "to it");
  } else if (scope.is_caller()) {
    throw_type_not_convertible(described_type_v<Request>, registered, context);
  }
}

// The leaf a binding of an interface stores: the leaf of its stored type, or,
// when that is an alternative type, the leaf of the alternative of it whose
// leaf the interface's is.
template <typename Leaf, typename Interface, typename = void>
struct bound_leaf {
  using type = Leaf;
};

template <typename Leaf, typename Interface>
struct bound_leaf<Leaf, Interface,
                  std::enable_if_t<is_alternative_type_v<Leaf>>> {
  template <typename... Alternatives>
  static constexpr bool holds(type_list<Alternatives...>) {
    return (std::is_same_v<leaf_type_t<Interface>, leaf_type_t<Alternatives>> ||
            ...);
  }
  using type =
      std::conditional_t<holds(alternative_type_alternatives_t<Leaf>{}),
                         leaf_type_t<Interface>, Leaf>;
};

// What a binding of an interface and a storage plans its requests by, in
// both containers: the leaf it stores, the element of its interface, the
// stored object its storage gives, made for the request by a scope that
// makes its instance or kept, of the qualifiers it is kept of, and whether a
// request of the interface asks for the leaf as its abstract interface.
template <typename Storage, typename Interface> struct binding_execution {
  // The leaf, and the element of the interface, of no qualifiers: those of
  // what the binding stores are of each level of it.
  using leaf_type =
      std::remove_cv_t<typename bound_leaf<leaf_type_t<typename Storage::type>,
                                           Interface>::type>;
  using interface_type = execution::innermost_t<Interface, leaf_type>;
  static constexpr semantics::scope_traits scope =
      scope_semantics_v<typename Storage::tag_type>;
  // A bounded array a scope makes is given as the pointer to its first
  // element: the stored object is the array.
  using stored_type = std::conditional_t<
      scope.makes && std::is_array_v<typename Storage::type>,
      typename Storage::type,
      std::conditional_t<scope.makes,
                         remove_cvref_t<typename storage_source_traits<
                             Storage>::consumed_type>,
                         std::remove_reference_t<typename storage_source_traits<
                             Storage>::borrowed_type>>>;
  using plan = execution::binding_plan<stored_type, leaf_type, scope.makes,
                                       scope.publishes_early>;
  static constexpr bool abstract = !std::is_same_v<leaf_type, interface_type>;
  static constexpr bool modeled =
      plan::stored_stack::element != stack_element::other;
};

// The stored object a source gives, where a plan starts: a value made for
// the request, which it owns, or the object or the pointer the storage
// keeps, which it borrows.
template <typename Stored, bool Makes, typename Source, typename Keeper>
auto plan_start(Source &&source, Keeper &keeper) {
  using source_type = remove_cvref_t<Source>;
  if constexpr (std::is_array_v<Stored> &&
                std::is_pointer_v<remove_cvref_t<decltype(*source)>>) {
    // The array a scope made, given as the pointer to its first element.
    return execution::carry<Makes>(*reinterpret_cast<Stored *>(*source));
  } else if constexpr (is_pointer_source_v<source_type> &&
                       std::is_pointer_v<Stored>) {
    // The pointer the storage keeps, as an object of its own.
    auto &kept = keeper.template keep<Stored>([&] { return source.get(); });
    return execution::carry<Makes>(kept);
  } else if constexpr (is_pointer_source_v<source_type>) {
    return execution::carry<Makes>(*source.get());
  } else {
    return execution::carry<Makes>(*source);
  }
}

// What a runtime resolution makes: kept with the binding when it lives as
// long as the container, in the context in the scope of the request
// otherwise.
template <typename Context> class runtime_keeper {
public:
  runtime_keeper(Context &context, construction_scope scope,
                 retention *retained, type_descriptor requested,
                 type_descriptor registered) noexcept
      : context_(context), scope_(scope), retained_(retained),
        requested_(requested), registered_(registered) {}

  // Rejects a request of what an empty layer does not hold.
  [[noreturn]] void absent() {
    throw_type_not_convertible(requested_, registered_, context_);
  }

  template <typename T, typename Make> T &keep_transient(Make &&make) {
    return context_.template construct_made<T>(scope_,
                                               std::forward<Make>(make));
  }

  template <typename T, typename Make> T &keep_stable(Make &&make) {
    // A binding that keeps no instance is planned to keep nothing made of
    // it.
    assert(retained_ != nullptr);
    return retained_->template keep<T>(&execution::retained_key<T>, context_,
                                       std::forward<Make>(make));
  }

  template <typename T, typename Make> T &keep(Make &&make) {
    return keep_transient<T>(std::forward<Make>(make));
  }

private:
  Context &context_;
  construction_scope scope_;
  retention *retained_;
  type_descriptor requested_;
  type_descriptor registered_;
};

// The plans of a request type, made once for each binding that resolves it:
// for a request that needs what it refers to to live as long as the
// container, and as long as the request. A plan depends only on the types
// of the binding and of the request, so that the plans are shared by every
// container, and a plan made twice at once is the same.
template <typename Keeper> class plan_cache {
public:
  using plan_type = execution::runtime_plan<Keeper>;

  struct entry {
    const void *binding;
    plan_type stable;
    plan_type transient;
    entry *next;
  };

  plan_cache() = default;
  plan_cache(const plan_cache &) = delete;
  plan_cache &operator=(const plan_cache &) = delete;
  ~plan_cache() {
    entry *at = head_.load(std::memory_order_acquire);
    while (at != nullptr) {
      entry *next = at->next;
      delete at;
      at = next;
    }
  }

  template <typename Make> const entry &find(const void *binding, Make &&make) {
    entry *head = head_.load(std::memory_order_acquire);
    for (entry *at = head; at != nullptr; at = at->next) {
      if (at->binding == binding) {
        return *at;
      }
    }
    auto *made = new entry{binding, make(semantics::lifetime::stable),
                           make(semantics::lifetime::request), head};
    while (!head_.compare_exchange_weak(made->next, made,
                                        std::memory_order_release,
                                        std::memory_order_acquire)) {
    }
    return *made;
  }

private:
  std::atomic<entry *> head_{nullptr};
};

// A request of type T as a runtime resolution plans it: the interface it
// names, its layers, what it asks for and the tables of the types it spells,
// and the address it is satisfied with of what a plan reaches.
template <typename Context> struct runtime_request {
  using keeper_type = runtime_keeper<Context>;
  type_descriptor requested;
  // The interface the request names, which only a binding of it resolves.
  type_descriptor interface;
  bool modeled;
  execution::request_layout<keeper_type> layout;
  plan_cache<keeper_type> *plans;
};

// The address of what satisfies a request of the shape Shape, of what a plan
// reaches: the object a request refers to, or what a pointer it reached
// points to; what a request takes or moves, which it consumes; a copy is
// made of the object. A value whose last step the request makes is made of
// what the plan reached before it.
template <typename Keeper>
resolved_address finish_request(execution::erased_carrier reached,
                                const execution::runtime_plan<Keeper> &plan) {
  using access = resolved_address::access_kind;
  using made_kind = resolved_address::made_kind;
  switch (plan.last) {
  case execution::last_step::copy:
    return {reached.object, access::borrow, made_kind::copy, 0, plan.quals};
  case execution::last_step::run:
    return {reached.object, access::consume, made_kind::run, plan.depth, 0};
  default:
    break;
  }
  switch (plan.how) {
  case semantics::satisfaction::point:
    return {plan.reached->pointee(reached.object), access::borrow};
  case semantics::satisfaction::take:
  case semantics::satisfaction::move:
    return {reached.object, access::consume};
  default:
    return {reached.object, access::borrow};
  }
}

// What C++ says of a request type, as a runtime resolution plans it.
template <typename T> struct request_execution {
  using category = requested_category<T>;
  using shape = typename category::shape;
  // The innermost type of the request: what a binding of its interface
  // stores the leaf of, or is the leaf as its interface.
  using element_type = execution::innermost_t<shape, void>;
  using stack = stack_of_t<shape, element_type>;
  using universe_type = layer_universe<typename stack::layers>;
  using layers = execution::runtime_layers<typename universe_type::identities>;
  static constexpr bool modeled =
      category::modeled && stack::element != stack_element::other;

  static constexpr semantics::request goal() {
    semantics::request made;
    execution::place_layers<universe_type>(made.layers,
                                           typename stack::layers{});
    made.depth =
        static_cast<std::uint8_t>(type_list_size_v<typename stack::layers>);
    quals_of<shape, element_type>::fill(made.quals);
    return made;
  }
};

template <typename T, typename Context>
inline plan_cache<runtime_keeper<Context>> runtime_plans;

template <typename T, typename Context>
constexpr runtime_request<Context> make_runtime_request() {
  using of = request_execution<T>;
  using keeper = runtime_keeper<Context>;
  if constexpr (!of::modeled) {
    return {described_type_v<T>, {}, false, {}, nullptr};
  } else {
    using tables =
        execution::request_tables<T, typename of::element_type, keeper>;
    return {described_type_v<T>,
            described_type_v<execution::interface_of_t<typename of::shape>>,
            true,
            {of::layers::value, of::layers::count, of::goal(),
             of::category::value, tables::value.value},
            &runtime_plans<T, Context>};
  }
}

template <typename T, typename Context>
inline constexpr runtime_request<Context> runtime_request_v =
    make_runtime_request<T, Context>();
} // namespace detail

namespace detail {
// A value of T made of the value at an address, of the type the request
// spells at a depth K, moved into all the layers above it at once.
template <typename T, typename Element, typename Types, std::size_t K>
T made_of_nested(void *address) {
  // The value is moved into what holds it of the qualifiers it needs.
  using of = std::remove_cv_t<typename Types::template nested<K>::type>;
  return execution::made_of<Element, Element, T>(
      std::move(*static_cast<of *>(address)));
}

// A value of T made of the run of wraps the plan left to it: the makers of
// the depths a run of wraps makes T of.
template <typename T, typename Element, typename Types, std::size_t K>
constexpr T (*maker_of())(void *) {
  if constexpr (execution::layer_types<T, Element,
                                       Element>::template runs<K>()) {
    return &made_of_nested<T, Element, Types, K>;
  } else {
    return nullptr;
  }
}

template <typename T, typename Element, typename Types, std::size_t... K>
T made_by_run(resolved_address result, std::index_sequence<K...>) {
  using maker = T (*)(void *);
  constexpr maker makers[] = {maker_of<T, Element, Types, K + 1>()...};
  return makers[result.depth - 1](result.address);
}

// The value a request of T takes, of what a plan reached: made where it is
// returned, of a copy or of a run of wraps the plan left to it, or as
// resolved_value gives it.
template <typename T> T returned_value(resolved_address result) {
  using made_kind = resolved_address::made_kind;
  if constexpr (std::is_reference_v<T> || std::is_pointer_v<T>) {
    return resolved_value<T>(result);
  } else {
    using value_type = std::remove_cv_t<T>;
    if (result.made == made_kind::copy) {
      if constexpr (std::is_constructible_v<value_type,
                                            const volatile value_type &>) {
        if (result.quals & semantics::volatile_qual) {
          return value_type(
              *static_cast<const volatile value_type *>(result.address));
        }
      }
      if constexpr (is_copy_constructible_v<value_type>) {
        return value_type(*static_cast<const value_type *>(result.address));
      }
    }
    using of = request_execution<T>;
    using types = execution::request_types<T, typename of::element_type>;
    if constexpr (types::depth != 0) {
      if (result.made == made_kind::run) {
        return made_by_run<value_type, typename of::element_type, types>(
            result, std::make_index_sequence<types::depth>{});
      }
    }
    return resolved_value<T>(result);
  }
}
} // namespace detail

template <typename T, typename RTTI, typename Binding, typename Context>
T resolve_binding(construction_scope scope, Binding &binding, Context &context,
                  const void *cache_key) {
  return detail::returned_value<T>(binding.resolve_request(
      scope, context, detail::runtime_request_v<T, Context>, cache_key));
}

// A wrapper made of the normalized value is a new object, so a reference
// request is not made of it.
template <typename Request>
inline constexpr bool constructs_normalized_v =
    !std::is_reference_v<Request> &&
    type_traits<std::decay_t<Request>>::enabled &&
    !std::is_pointer_v<std::decay_t<Request>> &&
    !std::is_same_v<normalized_type_t<Request>, std::decay_t<Request>>;

// Whether the normalized value of a request is an object that can be built.
// An rvalue request is not one: it asks the routed storage to publish a
// movable result, and is rejected rather than degraded to the normalized value.
// Nor is a bounded array: a pointer to a new one cannot be made with new, and
// a handle of one only decays it.
//
template <typename Request>
inline constexpr bool constructible_normalized_v =
    !std::is_rvalue_reference_v<Request> &&
    std::is_object_v<request_constructed_t<Request>> &&
    !std::is_abstract_v<request_constructed_t<Request>> &&
    std::extent_v<request_constructed_t<Request>> == 0;
// A wrapper is made of the normalized value the request resolves: moved into
// all its layers at once, or into a handle of an array of it.
template <typename Request>
inline constexpr bool construct_normalized_request_v = [] {
  if constexpr (constructs_normalized_v<Request> &&
                constructible_normalized_v<Request>) {
    return detail::execution::makes_as<std::decay_t<Request>,
                                       request_constructed_t<Request>>();
  } else {
    return false;
  }
}();
// Whether the factory constructs an unbound request. A handle of a dynamic
// array is not constructed: a default factory has no size to make it with.
template <typename Request,
          typename ResolvedRequest = request_result_t<Request, true>>
inline constexpr bool construct_factory_request_v =
    constructible_normalized_v<Request> &&
    !std::is_lvalue_reference_v<ResolvedRequest> &&
    !detail::is_array_like_type_v<std::decay_t<ResolvedRequest>> &&
    (std::is_pointer_v<ResolvedRequest> ||
     (type_traits<ResolvedRequest>::enabled &&
      !std::is_reference_v<ResolvedRequest>) ||
     std::is_constructible_v<ResolvedRequest, normalized_type_t<Request>>);

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
