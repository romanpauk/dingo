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
#include <dingo/base/type_list.h>
#include <dingo/request/composition.h>
#include <dingo/request/decomposition.h>
#include <dingo/type/type_traits.h>

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
// - refer: a reference or a pointer to a value the binding makes for the
//   request, resolved as an rvalue reference to it;
// - address: a pointer, resolved as a reference to what it points to;
// - move: a value, resolved as an rvalue reference to one the binding makes;
// - copy: a value, resolved as a reference to one the binding keeps;
// - convert: a sharing handle, resolved as the sole owner that converts to
//   it;
// - compose: a wrapper of the value it holds, resolved as the value.
// A reference into the context is refused to the caller, which uses what a
// resolution returns after the context ends.
enum class derivation : unsigned char {
  none,
  adopt,
  keep,
  refer,
  address,
  place,
  take,
  move,
  copy,
  convert,
  compose
};

template <typename T> struct request_tag {
  using type = T;
};

// What a binding tells of itself, which the derivation of a request depends
// on: whether it keeps its instance, whether a reference may refer to a value
// it makes, whether it publishes a pointer it makes that the request refers
// into or takes a value out of, whether a value of the request would slice
// what it stores, as one of a base of it does, and whether it publishes what
// it resolves before it constructs it, so that nothing is copied or moved of
// what it publishes.
struct derivation_facts {
  bool keeps;
  bool places;
  bool adopts;
  bool takes;
  bool slices;
  bool publishes_early;
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

// A value a request asks for: an object, not a reference to one nor a
// pointer.
template <typename Request> constexpr bool value_shape() {
  if constexpr (!std::is_reference_v<Request> &&
                !std::is_pointer_v<remove_cvref_t<Request>>) {
    using value_type = remove_cvref_t<Request>;
    if constexpr (std::is_object_v<value_type> &&
                  is_complete<value_type>::value) {
      return !std::is_array_v<value_type> && !std::is_abstract_v<value_type>;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

// What a reference or a pointer request refers to: an object, not an array.
template <typename Request> constexpr bool refers_object_shape() {
  if constexpr (refers_request_v<Request>) {
    using value_type = referred_value_t<Request>;
    if constexpr (std::is_object_v<value_type> &&
                  is_complete<value_type>::value) {
      return !std::is_array_v<value_type>;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

// The sole owner a sharing handle a request asks for converts from: the
// owning handle of its element whose sharing handle it is, or void.
template <typename Shared, typename Handles> struct sole_owner_of {
  using type = void;
};

template <typename Shared, typename... Handles>
struct sole_owner_of<Shared, type_list<Handles...>> {
  template <typename Handle> static constexpr bool shares_as() {
    using handle = std::remove_reference_t<Handle>;
    if constexpr (std::is_base_of_v<semantics::sole_owner,
                                    type_traits<handle>>) {
      return std::is_same_v<typename type_traits<handle>::template share_t<
                                typename type_traits<Shared>::value_type>,
                            Shared>;
    } else {
      return false;
    }
  }

  using type =
      type_list_find_t<void, type_list<Handles...>, shares_as<Handles>()...>;
};

template <typename Request> struct converted_from {
private:
  using shared = remove_cvref_t<Request>;

  template <typename Shared = shared, bool = is_object_wrapper_v<Shared>>
  struct of {
    using type = void;
  };

  template <typename Shared> struct of<Shared, true> {
    using type = std::conditional_t<
        std::is_base_of_v<semantics::shared_owner, type_traits<Shared>> &&
            !is_array_like_type_v<Shared> &&
            is_complete<typename type_traits<Shared>::value_type>::value,
        typename sole_owner_of<Shared, owning_handles_of_t<typename type_traits<
                                           Shared>::value_type>>::type,
        void>;
  };

public:
  using type = typename of<>::type;
};

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
  } else if constexpr (Derivation == derivation::refer) {
    return refers_object_shape<Request>();
  } else if constexpr (Derivation == derivation::address) {
    // The address of a pointer is not taken as a pointer to a pointer of
    // another qualification, which would let it point to what it must not.
    if constexpr (std::is_pointer_v<Request> &&
                  refers_object_shape<Request>()) {
      return !std::is_pointer_v<referred_value_t<Request>>;
    } else {
      return false;
    }
  } else if constexpr (Derivation == derivation::move) {
    if constexpr (value_shape<Request>()) {
      return std::is_move_constructible_v<remove_cvref_t<Request>>;
    } else {
      return false;
    }
  } else if constexpr (Derivation == derivation::copy) {
    if constexpr (value_shape<Request>()) {
      return is_copy_constructible_v<remove_cvref_t<Request>>;
    } else {
      return false;
    }
  } else if constexpr (Derivation == derivation::convert) {
    if constexpr (!std::is_lvalue_reference_v<Request> &&
                  !std::is_pointer_v<Request>) {
      return !std::is_void_v<typename converted_from<Request>::type>;
    } else {
      return false;
    }
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

template <typename Request> struct derived_request<derivation::refer, Request> {
  using type = std::remove_cv_t<referred_value_t<Request>> &&;
};

template <typename Request>
struct derived_request<derivation::address, Request> {
  using type = std::remove_pointer_t<Request> &;
};

template <typename Request> struct derived_request<derivation::move, Request> {
  using type = remove_cvref_t<Request> &&;
};

template <typename Request> struct derived_request<derivation::copy, Request> {
  using type = const remove_cvref_t<Request> &;
};

template <typename Request>
struct derived_request<derivation::convert, Request> {
  using type = typename converted_from<Request>::type;
};

template <typename Request>
struct derived_request<derivation::compose, Request> {
  using type = composed_inner_t<Request>;
};

template <derivation Derivation, typename Request>
using derived_request_t = typename derived_request<Derivation, Request>::type;

// Whether a binding publishes every request, as no binding does: what may be
// derived at all is asked of it.
struct publishes_every_request {
  template <typename T> constexpr bool operator()(request_tag<T>) const {
    return true;
  }
};

// The derivation of a request a binding does not publish, from what the
// binding tells of itself and from what it publishes, which publishes(tag)
// says of the request of a tag. A reference to what a pointer the binding
// makes points to refers to it; a wrapper that shares what the binding keeps
// is kept with it; a reference or a pointer to a value the binding makes
// refers to it, a pointer to what the binding refers to is its address;
// another value a reference refers to is placed, when the binding makes it,
// or it is a wrapper made of what the binding resolves or sharing it. A value
// is taken out of a pointer the binding makes, moved out of a value it makes,
// copied of one it keeps, converted of a sole owner it makes, or composed.
template <typename Request, typename Publishes>
constexpr derivation derive_request(derivation_facts facts,
                                    Publishes publishes) {
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
  if constexpr (derivation_shape_v<derivation::refer, Request>) {
    if (publishes(
            request_tag<derived_request_t<derivation::refer, Request>>{})) {
      return derivation::refer;
    }
  }
  if constexpr (derivation_shape_v<derivation::address, Request>) {
    if (publishes(
            request_tag<derived_request_t<derivation::address, Request>>{})) {
      return derivation::address;
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
  if constexpr (derivation_shape_v<derivation::move, Request>) {
    if (!facts.slices && !facts.publishes_early &&
        publishes(
            request_tag<derived_request_t<derivation::move, Request>>{})) {
      return derivation::move;
    }
  }
  if constexpr (derivation_shape_v<derivation::copy, Request>) {
    if (!facts.slices && !facts.publishes_early &&
        publishes(
            request_tag<derived_request_t<derivation::copy, Request>>{})) {
      return derivation::copy;
    }
  }
  if constexpr (derivation_shape_v<derivation::convert, Request>) {
    if (publishes(
            request_tag<derived_request_t<derivation::convert, Request>>{})) {
      return derivation::convert;
    }
  }
  if constexpr (derivation_shape_v<derivation::compose, Request>) {
    return derivation::compose;
  }
  return derivation::none;
}

// Whether a request may be derived at all, by any binding: by one of which
// every fact holds, whose values do not slice what it stores, which
// constructs what it publishes first, and which publishes every request.
template <typename Request>
inline constexpr bool derivable_v =
    derive_request<Request>({true, true, true, true, false, false},
                            publishes_every_request{}) != derivation::none;
} // namespace detail
} // namespace dingo
