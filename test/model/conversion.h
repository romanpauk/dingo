//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// What a binding plans for a request, without resolving it: whether it plans
// the request for as long as the container, which the caller may take, only
// for as long as the request, or not at all. Both containers plan a request
// alike, so one answer is of both. Nothing of a container is instantiated,
// so many registrations are answered in one program.

#pragma once

#include "model/probe.h"

#include <dingo/static/binding.h>

namespace dingo::model {

// How C++ itself builds a request of an object the binding holds,
// independent of the library. The stored object is one, which a unique
// scope makes for the request alone; and so is what it dereferences to, down
// to the leaf, which the request owns when it owns the unique_ptr or the
// optional that holds it, or the raw pointer a unique scope made it. An array
// is not dereferenced: its first element is no object of its own.
template <typename T> struct owns_element : std::false_type {};
template <typename T> struct owns_element<T *> : std::true_type {};
template <typename T, typename Deleter>
struct owns_element<std::unique_ptr<T, Deleter>> : std::true_type {};
template <typename T> struct owns_element<std::optional<T>> : std::true_type {};

// The value an optional is made of.
template <typename T> struct optional_value_of {
  using type = void;
};
template <typename T> struct optional_value_of<std::optional<T>> {
  using type = T;
};

template <typename Source, typename = void> struct dereferenced {
  using type = void;
};
template <typename Source>
struct dereferenced<Source, std::void_t<decltype(*std::declval<Source &>())>> {
  using type = std::conditional_t<
      std::is_array_v<Source>, void,
      std::remove_reference_t<decltype(*std::declval<Source &>())>>;
};

// The ways C++ builds a request of an object: binding a reference or a
// pointer to it without a temporary, converting a value of it the request
// owns, copying it into a value, or copying it into a new optional.
enum route : unsigned { refers, moves, copies, wraps };

// A way of building a request of an object at a depth of dereferences, of an
// object that shares nothing with its copy or not, as a bit of a mask.
constexpr unsigned route_bit(route how, std::size_t depth, bool value) {
  return 1u << ((how * 4 + depth) * 2 + (value ? 1 : 0));
}

// The ways C++ builds Request of Source, at a depth: a value converted of it,
// moved when the request owns it and copied otherwise, without an explicit
// constructor; a reference or a pointer bound to it without a temporary, an
// rvalue reference only to what the request owns; or a pointer the source
// is, or an array decays to.
template <typename Request, typename Source, bool Owned, std::size_t Depth>
constexpr unsigned routes_at() {
  static_assert(Depth < 4, "a route_bit holds four depths");
  using object = std::remove_reference_t<Request>;
  using value = std::remove_cv_t<Request>;
  constexpr bool shares_nothing =
      std::is_void_v<typename dereferenced<Source>::type> ||
      owns_element<Source>::value;
  unsigned found = 0;
  if constexpr (std::is_pointer_v<Request>) {
    if (std::is_convertible_v<Source *, Request> ||
        (std::is_array_v<Source> && std::is_convertible_v<Source &, Request>)) {
      found |= route_bit(refers, Depth, shares_nothing);
    }
  } else if constexpr (std::is_reference_v<Request>) {
    if (std::is_convertible_v<Source *, object *> &&
        (Owned || !std::is_rvalue_reference_v<Request>)) {
      found |= route_bit(refers, Depth, shares_nothing);
    }
  }
  // An array is no value of its own: it decays.
  if constexpr (!std::is_array_v<Source> && !std::is_reference_v<Request>) {
    if constexpr (Owned) {
      if (std::is_convertible_v<Source &&, value>) {
        found |= route_bit(moves, Depth, shares_nothing);
      }
    } else if constexpr (std::is_convertible_v<Source &, value>) {
      using made_of = typename optional_value_of<value>::type;
      if constexpr (!std::is_void_v<made_of> &&
                    std::is_convertible_v<Source &, made_of>) {
        found |= route_bit(wraps, Depth, shares_nothing);
      } else {
        found |= route_bit(copies, Depth, shares_nothing);
      }
    }
  }
  return found;
}

// The ways C++ builds Request of Source or of anything Source dereferences
// to.
template <typename Request, typename Source, bool Owned, std::size_t Depth = 0>
constexpr unsigned routes_through() {
  using element = typename dereferenced<Source>::type;
  constexpr unsigned found = routes_at<Request, Source, Owned, Depth>();
  if constexpr (std::is_void_v<element>) {
    return found;
  } else {
    return found |
           routes_through<Request, element,
                          Owned && owns_element<Source>::value, Depth + 1>();
  }
}

// Records the outcome of a request of Interface the binding of a
// registration answers, as the code compiles: published when it plans the
// request for as long as the container, placed when only for as long as the
// request, unpublished when not at all. A request the lookup sends to the
// binding of another interface is answered there. Records too the ways C++
// itself builds the request of what the binding holds, which the request
// owns when its scope Makes it for the request alone.
template <typename Model, typename Interface, typename Request, typename Leaf,
          bool Makes>
DINGO_ALWAYS_INLINE void publication(const char *prefix, const char *name) {
  using type = unannotated_t<Interface>;
  using binding = detail::binding<type, Model>;
  using planning = detail::static_execution<Request, binding>;
  using stored = typename Model::registered_storage_type;
  constexpr bool owned = !std::is_reference_v<stored> && Makes;
  (void)sizeof(Leaf);
  if constexpr (!selects_v<Request, type>) {
    record(prefix, name, "selects_another", nullptr);
  } else {
    const char *outcome = planning::template planned<true>().found ? "published"
                          : planning::template planned<false>().found
                              ? "placed"
                              : "unpublished";
    std::printf("{\"cell\": \"");
    print_json_text(prefix);
    print_json_text(name);
    std::printf(
        "\", \"outcome\": \"%s\", \"cpp_routes\": %u}\n", outcome,
        routes_through<Request, std::remove_reference_t<stored>, owned>());
  }
}

} // namespace dingo::model
