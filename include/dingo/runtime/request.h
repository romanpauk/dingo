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
// keeps, the conversions of a request type for each binding, and the value the
// request takes of what a conversion reached.

namespace dingo {
namespace detail {
// What a runtime resolution makes: kept with the binding when it lives as
// long as the container, in the context in the scope of the request
// otherwise.
template <typename Context>
class context_store final : public execution::runtime_store {
public:
  // The types a failure names are those of the request and of the binding,
  // which live as long as the program.
  context_store(Context &context, construction_scope scope, retention *retained,
                const type_descriptor &requested,
                const type_descriptor &registered) noexcept
      : context_(context), scope_(scope), retained_(retained),
        requested_(&requested), registered_(&registered) {}

  // Rejects a request of what an empty layer does not hold.
  [[noreturn]] void absent() override {
    throw_type_not_convertible(*requested_, *registered_, context_);
  }

  // Keeps an object of a layout, which construct constructs of make: with
  // the binding when it is stable, in the context in the scope of the
  // request otherwise.
  void *keep(bool stable, const object_layout &layout,
             execution::construct_function construct, void *make) override {
    auto place_made = [&](void *place) { construct(place, make); };
    if (stable) {
      // A binding that keeps no instance is converted to keep nothing made of
      // it.
      assert(retained_ != nullptr);
      return retained_->keep(&layout, context_, layout, place_made);
    }
    return context_.place(scope_, layout, place_made);
  }

  // The same, of a type a typed route names: what lives as long as the
  // request, in the context, of this store; what lives as long as the
  // container, with the binding, of at<true>(). Each is placed by the layout
  // of its type, as an erased one is, so that a type a route keeps adds no
  // code of the context of its own.
  template <typename T, typename Make> T &keep(Make &&make) {
    return *static_cast<T *>(
        context_.place(scope_, object_layout_v<T>, [&](void *place) {
          ::new (place) T(std::forward<Make>(make)());
        }));
  }

  struct stable_store {
    context_store &store;
    [[noreturn]] void absent() { store.absent(); }
    template <typename T, typename Make> T &keep(Make &&make) {
      assert(store.retained_ != nullptr);
      return *static_cast<T *>(store.retained_->keep(
          &object_layout_v<T>, store.context_, object_layout_v<T>,
          [&](void *place) { ::new (place) T(std::forward<Make>(make)()); }));
    }
  };

  template <bool Stable> auto part() {
    if constexpr (Stable) {
      return stable_store{*this};
    } else {
      return transient_store{*this};
    }
  }

  struct transient_store {
    context_store &store;
    [[noreturn]] void absent() { store.absent(); }
    template <typename T, typename Make> T &keep(Make &&make) {
      return store.template keep<T>(std::forward<Make>(make));
    }
  };

private:
  Context &context_;
  construction_scope scope_;
  retention *retained_;
  const type_descriptor *requested_;
  const type_descriptor *registered_;
};

// The conversions of a request type, made once for each binding that resolves
// it: for a request that needs what it refers to to live as long as the
// container, and as long as the request. A conversion depends only on the types
// of the binding and of the request, so that the conversions are shared by
// every container, and a conversion made twice at once is the same.
class conversion_cache {
public:
  using conversion_type = execution::runtime_conversion;

  struct entry {
    const void *binding;
    conversion_type stable;
    conversion_type transient;
    entry *next;
  };

  conversion_cache() = default;
  conversion_cache(const conversion_cache &) = delete;
  conversion_cache &operator=(const conversion_cache &) = delete;
  ~conversion_cache() {
    entry *current = head_.load(std::memory_order_acquire);
    while (current != nullptr) {
      entry *next = current->next;
      delete current;
      current = next;
    }
  }

  template <typename Make> const entry &find(const void *binding, Make &&make) {
    entry *head = head_.load(std::memory_order_acquire);
    for (entry *current = head; current != nullptr; current = current->next) {
      if (current->binding == binding) {
        return *current;
      }
    }
    execution::runtime_conversions conversions = make();
    auto *made =
        new entry{binding, conversions.stable, conversions.transient, head};
    while (!head_.compare_exchange_weak(made->next, made,
                                        std::memory_order_release,
                                        std::memory_order_acquire)) {
    }
    return *made;
  }

private:
  std::atomic<entry *> head_{nullptr};
};

// A request of type T as a runtime resolution converts it: the interface it
// names, its layers, what it asks for and the tables of the types it spells,
// and the address it is satisfied with of what a conversion reaches.
struct runtime_request {
  type_descriptor requested;
  // The key of the interface the request names, which only a binding of it
  // resolves.
  const void *interface_key;
  bool modeled;
  // The key of the type the request is of, without its own qualifiers.
  const void *shape;
  execution::request_layout layout;
  conversion_cache *conversions;
};

// The address of what satisfies a request of the shape Shape, of what a
// conversion reaches: the object a request refers to, or what a pointer it
// reached points to; what a request takes or moves, which it consumes; a copy
// is made of the object. A value whose last step the request makes is made of
// what the conversion reached before it.
inline resolved_address
finish_request(execution::erased_carrier reached,
               const execution::runtime_conversion &conversion) {
  using access = resolved_address::access_kind;
  using made_kind = resolved_address::made_kind;
  switch (conversion.last) {
  case execution::finish::copy:
    return {reached.object, access::borrow, made_kind::copy, 0,
            conversion.quals};
  case execution::finish::run:
    return {reached.object, access::consume, made_kind::run, conversion.depth,
            0};
  default:
    break;
  }
  switch (conversion.how) {
  case semantics::satisfaction::point:
    return {conversion.reached->pointee(reached.object), access::borrow};
  case semantics::satisfaction::take:
  case semantics::satisfaction::move:
    return {reached.object, access::consume};
  default:
    return {reached.object, access::borrow};
  }
}

// What C++ says of a request type, as a runtime resolution converts it.
template <typename T> struct request_execution {
  using category = requested_category<T>;
  using shape = typename category::shape;
  // The innermost type of the request: what a binding of its interface
  // stores the leaf of, or is the leaf as its interface.
  using element_type = execution::innermost_t<shape, void>;
  using stack = stack_t<shape, element_type>;
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
    stack_quals<shape, element_type>::fill(made.quals);
    return made;
  }
};

template <typename T> inline conversion_cache request_conversions;

template <typename T> constexpr runtime_request make_runtime_request() {
  using executed = request_execution<T>;
  if constexpr (!executed::modeled) {
    return {type_descriptor_v<T>, nullptr, false, nullptr, {}, nullptr};
  } else {
    using tables =
        execution::request_tables<T, typename executed::element_type>;
    return {type_descriptor_v<T>,
            &execution::shape_key<
                execution::request_interface_t<typename executed::shape>>,
            true,
            &execution::shape_key<std::remove_cv_t<typename executed::shape>>,
            {executed::layers::value, executed::layers::count, executed::goal(),
             executed::category::value, tables::value.value},
            &request_conversions<T>};
  }
}

template <typename T>
inline constexpr runtime_request runtime_request_v = make_runtime_request<T>();
} // namespace detail

namespace detail {
// A value of T made of the value at an address, of the type the request
// spells at a depth K, moved into all the layers above it at once.
template <typename T, typename Element, typename Types, std::size_t K>
T wrap_nested(void *address) {
  // The value is moved into what holds it of the qualifiers it needs.
  using nested_type =
      std::remove_cv_t<typename Types::template nested<K>::type>;
  return execution::wrap_value<Element, Element, T>(
      std::move(*static_cast<nested_type *>(address)));
}

// A value of T made of the run of wraps the conversion left to it: the makers
// of the depths a run of wraps makes T of.
template <typename T, typename Element, typename Types, std::size_t K>
constexpr T (*nested_maker())(void *) {
  if constexpr (execution::layer_types<T, Element,
                                       Element>::template runs<K>()) {
    return &wrap_nested<T, Element, Types, K>;
  } else {
    return nullptr;
  }
}

template <typename T, typename Element, typename Types, std::size_t... K>
T wrap_result(resolved_address result, std::index_sequence<K...>) {
  using maker = T (*)(void *);
  constexpr maker makers[] = {nested_maker<T, Element, Types, K + 1>()...};
  return makers[result.depth - 1](result.address);
}

// The value a request of T takes, of what a conversion reached: made where it
// is returned, of a copy or of a run of wraps the conversion left to it, or as
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
    using executed = request_execution<T>;
    using types = execution::request_types<T, typename executed::element_type>;
    if constexpr (types::depth != 0) {
      if (result.made == made_kind::run) {
        return wrap_result<value_type, typename executed::element_type, types>(
            result, std::make_index_sequence<types::depth>{});
      }
    }
    return resolved_value<T>(result);
  }
}
} // namespace detail

} // namespace dingo
