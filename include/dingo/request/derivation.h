//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/base/flags.h>

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

// What a binding is, which the derivation of a request depends on: it keeps
// its instance; a reference may refer to a value it makes; a value of the
// request would slice what it stores, as one of a base of it does; it
// publishes what it resolves before it constructs it, so that nothing is
// copied or moved of what it publishes.
enum class binding_trait : unsigned char {
  keeps,
  places,
  slices,
  publishes_early,
};

// What a binding tells of itself for a request: what it is, and the
// derivations whose source it publishes, the request each resolves through.
struct derivation_facts {
  flags<binding_trait> traits;
  flags<derivation> sources;
};

class retention;

// A resolution a runtime binding publishes: the type it resolves to, and,
// when it resolves to a pointer the storage hands over, the reference to what
// the pointer points to, which a reference request adopts the pointer for,
// and whether a value request takes the value out of it.
struct published_resolution {
  type_descriptor target;
  type_descriptor adopted;
  bool adopts;
  bool takes;
};

// What a runtime binding tells of itself, as constant data: the resolutions
// it publishes, what it is, whether a value of its interface slices what it
// stores, and its interface.
struct binding_facts {
  const published_resolution *published;
  std::size_t published_count;
  flags<binding_trait> traits;
  bool slices;
  type_descriptor exposed;
};

// The index of the resolution a binding publishes for a request, or the
// number of its resolutions when it publishes none.
constexpr std::size_t published_index(const binding_facts &binding,
                                      type_descriptor requested) {
  for (std::size_t i = 0; i < binding.published_count; ++i) {
    if (matches_qualification_conversion(binding.published[i].target,
                                         requested)) {
      return i;
    }
  }
  return binding.published_count;
}

constexpr bool publishes(const binding_facts &binding,
                         type_descriptor requested) {
  return published_index(binding, requested) != binding.published_count;
}

// Whether a binding resolves a reference request as a pointer it hands over,
// which an owner placed in the context adopts; or, with take, a value request
// as the value moved out of what the pointer points to.
constexpr bool hands_over_request(const binding_facts &binding,
                                  type_descriptor requested, bool take) {
  for (std::size_t i = 0; i < binding.published_count; ++i) {
    const auto &resolution = binding.published[i];
    if ((take ? resolution.takes : resolution.adopts) &&
        matches_qualification_conversion(resolution.adopted, requested)) {
      return true;
    }
  }
  return false;
}

// What a runtime binding publishes of what the derivations of a request
// resolve through, of the descriptor of the request: a value is moved out of
// an rvalue reference to it or copied of a const reference to it, and a
// reference or a pointer refers to what an rvalue reference refers to; a
// pointer is the address of what an lvalue reference refers to. A sole owner
// a sharing handle converts of is known only to the request.
constexpr flags<derivation> runtime_published(const binding_facts &binding,
                                              type_descriptor requested) {
  flags<derivation> sources;
  auto publish = [&](derivation how, type_descriptor source,
                     type_reference_kind reference, type_cv_flags cv) {
    source.reference = reference;
    source.cv = cv;
    sources.set(how, publishes(binding, source));
  };
  if (requested.pointee != nullptr &&
      requested.reference == type_reference_kind::none) {
    const type_descriptor pointee = *requested.pointee;
    publish(derivation::refer, pointee, type_reference_kind::rvalue,
            type_cv_flags::none);
    publish(derivation::address, pointee, type_reference_kind::lvalue,
            pointee.cv);
  } else if (requested.reference == type_reference_kind::lvalue) {
    publish(derivation::refer, requested, type_reference_kind::rvalue,
            type_cv_flags::none);
  } else if (requested.reference == type_reference_kind::none) {
    publish(derivation::move, requested, type_reference_kind::rvalue,
            type_cv_flags::none);
    publish(derivation::copy, requested, type_reference_kind::lvalue,
            requested.cv | type_cv_flags::is_const);
  }
  return sources;
}

// What a runtime binding tells the derivation of a request it does not
// publish: whether it keeps its instance, places what it makes, whether a
// value of its interface would slice what it stores, and whether it publishes
// before it constructs; and the derivations whose source it publishes, among
// them the pointer it makes that a reference request refers into, or that a
// value request is taken out of, as the value can be moved.
constexpr derivation_facts
runtime_derivation_facts(const binding_facts &binding,
                         type_descriptor requested) {
  derivation_facts facts{binding.traits, runtime_published(binding, requested)};
  facts.traits.set(
      binding_trait::slices,
      binding.slices && requested.reference != type_reference_kind::lvalue &&
          requested.pointee == nullptr && requested.tag == binding.exposed.tag);
  if (requested.reference == type_reference_kind::lvalue &&
      hands_over_request(binding, requested, false)) {
    facts.sources.set(derivation::adopt);
  }
  if (requested.reference != type_reference_kind::lvalue &&
      requested.pointee == nullptr) {
    type_descriptor pointee = requested;
    pointee.reference = type_reference_kind::lvalue;
    pointee.cv = type_cv_flags::none;
    if (hands_over_request(binding, pointee, true)) {
      facts.sources.set(derivation::take);
    }
  }
  return facts;
}

// The derivation a binding of its facts makes of a request: of the descriptor
// of the request and, when the request converts the sole owner a sharing
// handle is made of, of the descriptor of that owner, which only the request
// names. Both containers decide by it: the runtime one as it resolves, the
// static one as it compiles.
constexpr derivation_facts derivation_facts_of(const binding_facts &binding,
                                               type_descriptor requested,
                                               bool converts,
                                               type_descriptor owner) {
  derivation_facts facts = runtime_derivation_facts(binding, requested);
  if (converts) {
    facts.sources.set(derivation::convert, publishes(binding, owner));
  }
  return facts;
}

// What a runtime binding hands the derivation of a request it does not
// publish: what it keeps of its instance, the type it registers, the
// descriptor the request was dispatched with, the key the request is cached
// under, if it caches it, and what it tells of itself.
struct derivation_site {
  retention *kept;
  type_descriptor registered;
  type_descriptor requested;
  const void *cache_key;
  const binding_facts *binding;
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

// What a request is of, and its shape: the object a reference refers to,
// the object a pointer points to, of any qualification of the pointer, or
// the object a value is. A request of an object of any qualification is one
// of the eight shapes of it, as const or not, of the object as volatile or
// not.
enum class request_shape : unsigned char {
  value,
  const_value,
  lvalue,
  const_lvalue,
  rvalue,
  const_rvalue,
  pointer,
  const_pointer
};

inline constexpr std::size_t request_shape_count = 8;

template <typename Request> struct request_object {
private:
  using qualified = std::conditional_t<
      std::is_reference_v<Request>, std::remove_reference_t<Request>,
      std::conditional_t<std::is_pointer_v<Request>,
                         std::remove_pointer_t<Request>, Request>>;
  static constexpr unsigned kind = std::is_lvalue_reference_v<Request>   ? 2
                                   : std::is_rvalue_reference_v<Request> ? 4
                                   : std::is_pointer_v<Request>          ? 6
                                                                         : 0;

public:
  using type = std::remove_cv_t<qualified>;
  static constexpr bool is_volatile = std::is_volatile_v<qualified>;
  static constexpr request_shape shape =
      static_cast<request_shape>(kind + std::is_const_v<qualified>);
};

template <typename Request>
using request_object_t = typename request_object<Request>::type;

// Whether a request of a shape refers to the object: by a reference or by a
// pointer.
constexpr bool refers_shape(request_shape shape) {
  return shape == request_shape::lvalue ||
         shape == request_shape::const_lvalue ||
         shape == request_shape::pointer ||
         shape == request_shape::const_pointer;
}

constexpr bool value_shape(request_shape shape) {
  return shape == request_shape::value || shape == request_shape::const_value;
}

// What an object is, which the derivations of the requests of it depend on:
// whether it can exist, is complete, an array or a pointer; whether a value
// of it can be placed, and, of a pointer, what it points to placed as one;
// whether it is constructed of an rvalue of it, moved or copied; whether it
// holds a copy that shares what it was copied of; whether a binding composes
// it of what it holds, or converts it of a sole owner. It is told once of
// each object, of which each shape of a request of it tells its derivations.
struct object_traits {
  bool object;
  bool complete;
  bool array;
  bool pointer;
  bool places;
  bool takes;
  bool moves;
  bool copies;
  bool shares;
  bool composes;
  bool converts;
};

template <typename Object> constexpr object_traits make_object_traits() {
  object_traits traits{};
  traits.object = std::is_object_v<Object>;
  traits.array = std::is_array_v<Object>;
  traits.pointer = std::is_pointer_v<Object>;
  if constexpr (std::is_object_v<Object>) {
    traits.complete = is_complete<Object>::value;
    if constexpr (is_complete<Object>::value && !std::is_array_v<Object>) {
      traits.places = !std::is_abstract_v<Object> &&
                      (!std::is_pointer_v<Object> ||
                       std::is_object_v<std::remove_pointer_t<Object>>);
      traits.takes = std::is_constructible_v<Object, Object &&>;
      if constexpr (!std::is_abstract_v<Object>) {
        traits.moves = std::is_move_constructible_v<Object>;
        traits.copies = is_copy_constructible_v<Object>;
      }
    }
    if constexpr (!std::is_pointer_v<Object>) {
      if constexpr (type_traits<Object>::enabled) {
        traits.shares = holds_sharing_copy_v<Object>;
      }
      traits.composes = composes_v<Object>;
    }
    traits.converts = !std::is_void_v<typename converted_from<Object>::type>;
  }
  return traits;
}

template <typename Object>
inline constexpr object_traits object_traits_v = make_object_traits<Object>();

// What a request is, which the derivation that applies to it depends on: the
// derivations whose shape it has, whether it is a wrapper a binding composes,
// and whether it refers to a wrapper that shares what a binding keeps. A
// derivation is chosen of these values, so that requests of one shape share
// the code that chooses it.
struct request_facts {
  flags<derivation> shapes;
  bool composes;
  bool refers_to_sharing;
};

// The facts of a request of a shape of an object. The address of a pointer
// is not taken as a pointer to a pointer of another qualification, which
// would let it point to what it must not.
constexpr request_facts shape_facts(object_traits object, request_shape shape) {
  const bool refers = refers_shape(shape);
  const bool lvalue =
      shape == request_shape::lvalue || shape == request_shape::const_lvalue;
  const bool pointer =
      shape == request_shape::pointer || shape == request_shape::const_pointer;
  const bool refers_object = refers && object.object && !object.array;
  flags<derivation> shapes;
  shapes.set(derivation::adopt, lvalue && object.complete);
  shapes.set(derivation::keep, refers && object.shares);
  shapes.set(derivation::place, refers && object.places);
  shapes.set(derivation::take, !refers && object.takes);
  shapes.set(derivation::refer, refers_object);
  shapes.set(derivation::address, pointer && refers_object && !object.pointer);
  shapes.set(derivation::move, value_shape(shape) && object.moves);
  shapes.set(derivation::copy, value_shape(shape) && object.copies);
  shapes.set(derivation::convert, !refers && object.converts);
  shapes.set(derivation::compose, !refers && object.composes);
  return {shapes, object.composes, refers && object.shares};
}

template <typename Request>
inline constexpr request_facts request_facts_v = shape_facts(
    object_traits_v<request_object_t<Request>>, request_object<Request>::shape);

template <derivation Derivation, typename Request>
inline constexpr bool derivation_shape_v =
    request_facts_v<Request>.shapes.has(Derivation);

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
  using type = request_object_t<Request>;
};

template <typename Request> struct derived_request<derivation::take, Request> {
  using type = std::add_pointer_t<remove_cvref_t<Request>>;
};

template <typename Request> struct derived_request<derivation::refer, Request> {
  using type = request_object_t<Request> &&;
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

template <typename T> constexpr type_descriptor converted_owner() {
  if constexpr (derivation_shape_v<derivation::convert, T>) {
    return described_type_v<derived_request_t<derivation::convert, T>>;
  } else {
    return {};
  }
}

// The derivation facts of a request of T.
template <typename T>
constexpr derivation_facts derivation_facts_of(const binding_facts &binding,
                                               type_descriptor requested) {
  return derivation_facts_of(binding, requested,
                             derivation_shape_v<derivation::convert, T>,
                             converted_owner<T>());
}

// Whether a request copies or moves nothing of what a binding publishes: a
// value of it would slice what the binding stores, or the binding publishes
// it before it constructs it.
constexpr bool copies_nothing(derivation_facts facts) {
  return facts.traits.has(binding_trait::slices) ||
         facts.traits.has(binding_trait::publishes_early);
}

// Whether a derivation applies to a request of a binding, of what the
// binding tells of itself: one that resolves through a source applies where
// the binding publishes the source; a wrapper that shares what the binding
// keeps is kept with it; a value a reference refers to is placed, when the
// binding makes it, or it is a wrapper made of what the binding resolves or
// sharing it; a value is moved or copied only of what may be; anything else
// is composed.
constexpr bool applies(derivation how, request_facts request,
                       derivation_facts facts) {
  if (!request.shapes.has(how)) {
    return false;
  }
  switch (how) {
  case derivation::keep:
    return facts.traits.has(binding_trait::keeps);
  case derivation::place:
    return facts.traits.has(binding_trait::places) || request.composes ||
           request.refers_to_sharing;
  case derivation::move:
  case derivation::copy:
    return !copies_nothing(facts) && facts.sources.has(how);
  case derivation::compose:
    return true;
  default:
    return facts.sources.has(how);
  }
}

// The order the derivations of a request are tried in. A reference to what a
// pointer the binding makes points to refers to it; a reference or a pointer
// to a value the binding makes refers to it, and a pointer to what the
// binding refers to is its address; then a wrapper is kept, or a value
// placed. A value is taken out of a pointer the binding makes, moved out of a
// value it makes, copied of one it keeps, converted of a sole owner it makes,
// or composed.
inline constexpr derivation derivation_order[] = {
    derivation::adopt,  derivation::refer, derivation::address,
    derivation::keep,   derivation::place, derivation::take,
    derivation::move,   derivation::copy,  derivation::convert,
    derivation::compose};

// The derivation of a request a binding does not publish: the first that
// applies to it.
constexpr derivation derive_request(request_facts request,
                                    derivation_facts facts) {
  for (derivation how : derivation_order) {
    if (applies(how, request, facts)) {
      return how;
    }
  }
  return derivation::none;
}

template <typename Request>
constexpr derivation derive_request(derivation_facts facts) {
  return derive_request(request_facts_v<Request>, facts);
}

// Whether a request may be derived at all, by any binding: by one that keeps
// its instance and places what it makes, whose values do not slice what it
// stores, which constructs what it publishes first, and which publishes the
// source of every derivation.
constexpr bool derivable(request_facts request) {
  return derive_request(
             request,
             {flags<binding_trait>::of(binding_trait::keeps,
                                       binding_trait::places),
              flags<derivation>::of(derivation::adopt, derivation::take,
                                    derivation::refer, derivation::address,
                                    derivation::move, derivation::copy,
                                    derivation::convert)}) != derivation::none;
}

template <typename Request>
inline constexpr bool derivable_v = derivable(request_facts_v<Request>);
} // namespace detail
} // namespace dingo
