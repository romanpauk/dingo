//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/meta.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/binding/request_composition.h>
#include <dingo/type/request.h>

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {
// How a request a binding does not publish is derived from one it resolves:
// - adopt: a reference to what a pointer the binding makes points to,
//   resolved as the pointer, which an owner placed in the context adopts;
// - keep: a reference or a pointer to a wrapper that shares what the binding
//   keeps, resolved as the wrapper, which the binding keeps with it;
// - place: a reference or a pointer to a value made for the request,
//   resolved as the value, placed in the context;
// - take: a value of what a pointer the binding makes points to, resolved as
//   the pointer and moved out of what it points to;
// - compose: a wrapper of the value it holds, resolved as the value.
// A reference into the context is refused to the caller, which uses what a
// resolution returns after the context ends.
enum class derivation : unsigned char {
  none,
  adopt,
  keep,
  place,
  take,
  compose
};

// What a binding tells of itself, which the derivation of a request depends
// on: whether it keeps its instance, whether a reference may refer to a value
// it makes, and whether it publishes a pointer it makes that the request
// refers into or takes a value out of.
struct derivation_facts {
  bool keeps;
  bool places;
  bool adopts;
  bool takes;
};

class retention;

// What a runtime binding hands the derivation of a request it does not
// publish: what it keeps of its instance, the type it registers, the key the
// request is cached under, if it caches it, and, of a request, whether it
// publishes it and its facts.
struct derivation_site {
  retention *kept;
  type_descriptor registered;
  const void *cache_key;
  bool (*publishes)(type_descriptor);
  derivation_facts (*facts_of)(type_descriptor);
};

// The owner of a pointer handed over to a request, which a reference to what
// it points to is resolved through: the owner is placed in the context.
template <typename T> std::unique_ptr<T> adopt_pointer(T *pointer) {
  return std::unique_ptr<T>(pointer);
}

template <typename T, std::size_t N>
std::unique_ptr<T[]> adopt_pointer(T (*array)[N]) {
  return std::unique_ptr<T[]>(*array);
}

template <typename Pointer, typename Context, typename Scope>
void place_adopted_pointer(Scope scope, Context &context, Pointer pointer) {
  auto owner = adopt_pointer(pointer);
  context.template construct<decltype(owner)>(scope, std::move(owner));
}

// The shapes of request each derivation applies to.
template <typename Request>
inline constexpr bool refers_request_v =
    std::is_lvalue_reference_v<Request> || std::is_pointer_v<Request>;

template <typename Request> constexpr bool places_shape() {
  if constexpr (refers_request_v<Request>) {
    using value_type = referred_value_t<Request>;
    if constexpr (std::is_object_v<value_type> &&
                  is_complete<value_type>::value) {
      return !std::is_abstract_v<value_type> && !std::is_array_v<value_type>;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

template <typename Request> constexpr bool takes_shape() {
  if constexpr (!std::is_lvalue_reference_v<Request> &&
                !std::is_pointer_v<Request>) {
    using value_type = remove_cvref_t<Request>;
    if constexpr (std::is_object_v<value_type> &&
                  is_complete<value_type>::value) {
      return !std::is_array_v<value_type> &&
             std::is_constructible_v<value_type, value_type &&>;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

template <derivation Derivation, typename Request>
constexpr bool derivation_shape() {
  if constexpr (Derivation == derivation::adopt) {
    if constexpr (std::is_lvalue_reference_v<Request> &&
                  std::is_object_v<std::remove_reference_t<Request>>) {
      return is_complete<
          std::remove_cv_t<std::remove_reference_t<Request>>>::value;
    } else {
      return false;
    }
  } else if constexpr (Derivation == derivation::keep) {
    return refers_to_sharing_v<Request>;
  } else if constexpr (Derivation == derivation::place) {
    return places_shape<Request>();
  } else if constexpr (Derivation == derivation::take) {
    return takes_shape<Request>();
  } else if constexpr (Derivation == derivation::compose) {
    return !refers_request_v<Request> && composes_v<Request>;
  } else {
    return false;
  }
}

template <derivation Derivation, typename Request>
inline constexpr bool derivation_shape_v =
    derivation_shape<Derivation, Request>();

// The request a derivation resolves through the binding.
template <derivation Derivation, typename Request> struct derived_request {
  using type = void;
};

template <typename Request> struct derived_request<derivation::adopt, Request> {
  using type = std::add_pointer_t<std::remove_reference_t<Request>>;
};

template <typename Request> struct derived_request<derivation::keep, Request> {
  using type = composed_type_t<Request>;
};

template <typename Request> struct derived_request<derivation::place, Request> {
  using type = referred_value_t<Request>;
};

template <typename Request> struct derived_request<derivation::take, Request> {
  using type = std::add_pointer_t<remove_cvref_t<Request>>;
};

template <typename Request>
struct derived_request<derivation::compose, Request> {
  using type = composed_inner_t<Request>;
};

template <derivation Derivation, typename Request>
using derived_request_t = typename derived_request<Derivation, Request>::type;

// The derivation of a request a binding does not publish. A reference to
// what a pointer the binding makes points to refers to it; a wrapper that
// shares what the binding keeps is kept with it; another value a reference
// refers to is placed, when the binding makes it, or it is a wrapper made of
// what the binding resolves or sharing it. A value is taken out of a pointer
// the binding makes, or composed.
template <typename Request>
constexpr derivation derive_request(derivation_facts facts) {
  if constexpr (derivation_shape_v<derivation::adopt, Request>) {
    if (facts.adopts) {
      return derivation::adopt;
    }
  }
  if constexpr (derivation_shape_v<derivation::keep, Request>) {
    if (facts.keeps) {
      return derivation::keep;
    }
  }
  if constexpr (derivation_shape_v<derivation::place, Request>) {
    if (facts.places || composes_v<Request> || refers_to_sharing_v<Request>) {
      return derivation::place;
    }
  }
  if constexpr (derivation_shape_v<derivation::take, Request>) {
    if (facts.takes) {
      return derivation::take;
    }
  }
  if constexpr (derivation_shape_v<derivation::compose, Request>) {
    return derivation::compose;
  }
  return derivation::none;
}

// Whether a request may be derived at all, by any binding: by one of which
// every fact holds.
template <typename Request>
inline constexpr bool derivable_v =
    derive_request<Request>({true, true, true, true}) != derivation::none;
} // namespace detail
} // namespace dingo
