//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/construction_scope.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/binding/interface.h>
#include <dingo/binding/resolution.h>
#include <dingo/request/execution.h>
#include <dingo/runtime/execution.h>
#include <dingo/storage/retention.h>

#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

// A request as the runtime container resolves it: what a resolution makes and
// keeps, the plans of a request type for each binding, and the value the
// request takes of what a plan reached.

namespace dingo {
template <typename T, typename RTTI, typename Binding, typename Context>
T resolve_binding(construction_scope scope, Binding &binding, Context &context,
                  const void *cache_key = nullptr);

namespace detail {
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

} // namespace dingo
