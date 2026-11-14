//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// What the conversion layer of a binding answers a request, without resolving
// it: whether the request lists of the static container hold it, whether the
// descriptors the runtime container dispatches on select it, and whether the
// conversion of the resolution is available. Nothing of a container is
// instantiated, so many registrations are answered in one program.

#pragma once

#include "model/probe.h"

#include <dingo/runtime/binding.h>
#include <dingo/static/binding.h>

#include <cstddef>
#include <string>
#include <type_traits>

namespace dingo::model {

template <typename Operation> struct conversion_available : std::true_type {};

template <typename Source, typename Conversion>
struct conversion_available<detail::type_resolution<Source, Conversion>>
    : std::bool_constant<Conversion::available> {};

// The descriptors of the targets of the resolutions of a binding, once for
// the binding, and how many there are.
template <typename Resolutions> struct resolution_targets;

template <typename... Resolutions>
struct resolution_targets<type_list<Resolutions...>> {
  static constexpr type_descriptor values[] = {
      detail::described_type_v<typename Resolutions::target_type>..., {}};
  static constexpr std::size_t size = sizeof...(Resolutions);
};

// The index of the first target a request qualifies as, or the number of
// targets when none does. It is of no type.
constexpr std::size_t target_index(const type_descriptor *targets,
                                   std::size_t size, type_descriptor request) {
  std::size_t index = 0;
  while (index < size && !detail::qualifies_as(targets[index], request)) {
    ++index;
  }
  return index;
}

// The index of the resolution the runtime container dispatches a request to,
// or the number of resolutions when none matches.
template <typename Request, typename Resolutions>
inline constexpr std::size_t runtime_index_v =
    target_index(resolution_targets<Resolutions>::values,
                 resolution_targets<Resolutions>::size,
                 detail::described_type_v<Request>);

// The derivation the runtime container selects for a request its binding
// does not publish, from the facts the binding tells of itself and from what
// it publishes.
template <typename Request, typename Interface, typename Storage,
          typename Resolutions>
constexpr detail::derivation runtime_derivation() {
  if constexpr (!detail::derivable_v<Request>) {
    return detail::derivation::none;
  } else {
    return detail::derive_request<Request>(detail::binding_derivations<Request>(
        detail::binding_facts_v<Interface, Storage, Resolutions>::value,
        detail::described_type_v<Request>));
  }
}

// Whether the two containers resolve a request alike, down the chain of the
// requests it is derived from: both publish it, or both derive it the same
// way from a request they resolve alike, or neither resolves it.
template <typename Request, typename Interface, typename Binding,
          typename Storage, typename Resolutions>
constexpr bool derives_alike() {
  constexpr bool by_static_list =
      detail::binding_supports_request_v<Request, Binding>;
  constexpr bool by_runtime_list =
      runtime_index_v<Request, Resolutions> < type_list_size_v<Resolutions>;
  if constexpr (by_static_list != by_runtime_list) {
    return false;
  } else if constexpr (by_static_list) {
    return true;
  } else {
    constexpr detail::derivation by_static =
        detail::static_derivation_v<Request, Binding>;
    constexpr detail::derivation by_runtime =
        runtime_derivation<Request, Interface, Storage, Resolutions>();
    if constexpr (by_static != by_runtime) {
      return false;
    } else if constexpr (by_static == detail::derivation::none) {
      return true;
    } else {
      return derives_alike<detail::derived_request_t<by_static, Request>,
                           Interface, Binding, Storage, Resolutions>();
    }
  }
}

template <std::size_t Index, typename Resolutions>
struct resolution_at_available : std::false_type {};

template <std::size_t Index, typename Head, typename... Tail>
struct resolution_at_available<Index, type_list<Head, Tail...>>
    : resolution_at_available<Index - 1, type_list<Tail...>> {};

template <typename Head, typename... Tail>
struct resolution_at_available<0, type_list<Head, Tail...>>
    : conversion_available<typename Head::operation> {};

// Whether S is a handle of an abstract interface, which holds a leaf L
// through a handle of L it is constructed from.
template <typename S, typename L, typename = void>
struct handle_of_abstract : std::false_type {};

template <typename S, typename L>
struct handle_of_abstract<
    S, L,
    std::void_t<typename type_traits<S>::value_type,
                typename type_traits<S>::template rebind_t<L>>>
    : std::bool_constant<
          std::is_abstract_v<typename type_traits<S>::value_type> &&
          std::is_constructible_v<
              S, typename type_traits<S>::template rebind_t<L> &&>> {};

// A new object of type P holding the leaf L, which a conversion may adopt: a
// leaf when P is an interface of it, two rows when P is an array.
template <typename P, typename L> P *new_object() {
  if constexpr (std::is_abstract_v<P>) {
    return fresh<L *>::make();
  } else if constexpr (std::is_array_v<P>) {
    using element = std::remove_extent_t<P>;
    static_assert(std::extent_v<P> == 2, "a probe array has two rows");
    struct rows {
      P value;
    };
    return &(new rows{{made_value<element>(), made_value<element>()}})->value;
  } else if constexpr (std::is_move_constructible_v<P>) {
    return new P(instance<P>::make());
  } else {
    return fresh<P *>::make();
  }
}

// An object of a type S a conversion takes, which holds the leaf L, made
// anew for one conversion of what a registration stores as Stored: a leaf
// when S is an interface of it, a new object when S is a pointer, the rows
// of a new array when it is one to an element of the stored array, a handle
// of the leaf when S is a handle of an interface, and an instance of S
// otherwise. An array is not made by value.
template <typename S, typename L, typename Stored> struct source_object {
  static constexpr bool available = !std::is_array_v<S>;

  static auto make() {
    if constexpr (std::is_base_of_v<leaf_interface, S>) {
      return made_value<L>();
    } else if constexpr (std::is_pointer_v<S>) {
      using pointee = std::remove_cv_t<std::remove_pointer_t<S>>;
      using row = std::remove_cv_t<std::remove_extent_t<Stored>>;
      if constexpr (std::is_array_v<Stored> && std::is_same_v<pointee, row>) {
        static_assert(std::extent_v<Stored> == 2, "a probe array has two rows");
        return S(new row[2]{made_value<row>(), made_value<row>()});
      } else {
        return S(new_object<pointee, L>());
      }
    } else if constexpr (handle_of_abstract<S, L>::value) {
      return S(fresh<typename type_traits<S>::template rebind_t<L>>::make());
    } else {
      return fresh<S>::make();
    }
  }
};

template <typename Operation> struct conversion_of {
  static constexpr bool counted = false;
};

template <typename Source, typename Conversion>
struct conversion_of<detail::type_resolution<Source, Conversion>> {
  using source_type = Source;
  using conversion_type = Conversion;
  static constexpr bool counted = true;
};

// What a binding makes a request of what its conversion returns: a value is
// made of it as a binding makes one, and a reference or a pointer is what the
// conversion returns.
template <typename Request, typename Apply> void make_request(Apply &&apply) {
  using applied = decltype(apply());
  if constexpr (std::is_reference_v<Request> || std::is_pointer_v<Request>) {
    decltype(auto) result = apply();
    leaf_interface::counting = false;
    static_cast<void>(result);
  } else if constexpr (!std::is_reference_v<applied>) {
    Request result(apply());
    leaf_interface::counting = false;
    static_cast<void>(result);
  } else {
    auto &&instance = apply();
    Request result = detail::consume_resolved<Request>(
        std::forward<decltype(instance)>(instance),
        [](auto &&value) -> Request {
          return Request(std::forward<decltype(value)>(value));
        });
    leaf_interface::counting = false;
    static_cast<void>(result);
  }
}

// Records a published request, with the copies and the moves of the leaf L
// its conversion makes, applied alone to an object of its source, when one
// can be made and holds what the request converts: the alternative of a
// variant that is not the leaf is not converted.
template <typename Operation, typename Request, typename L, typename Stored>
void record_published(const char *prefix, const char *name) {
  const std::string cell = std::string(prefix) + name;
  using of = conversion_of<Operation>;
  if constexpr (!of::counted) {
    record(cell.c_str(), "published", nullptr);
  } else {
    using source = typename of::source_type;
    using object = source_object<detail::remove_cvref_t<source>, L, Stored>;
    if constexpr (!object::available) {
      record(cell.c_str(), "published", nullptr);
    } else {
      auto made = object::make();
      detail::empty_conversion_context state;
      const auto requested = detail::described_type_v<Request>;
      const auto registered = detail::described_type_v<source>;
      leaf_interface::copies = 0;
      leaf_interface::moves = 0;
      leaf_interface::counting = true;
      try {
        make_request<Request>([&]() -> decltype(auto) {
          if constexpr (std::is_lvalue_reference_v<source>) {
            return of::conversion_type::apply(state, state, made, requested,
                                              registered);
          } else {
            return of::conversion_type::apply(state, state, std::move(made),
                                              requested, registered);
          }
        });
      } catch (const type_not_convertible_exception &) {
        leaf_interface::counting = false;
        return record(cell.c_str(), "published", nullptr);
      }
      std::printf("{\"cell\": %s, \"outcome\": \"published\", \"copies\": "
                  "%ld, \"moves\": %ld}\n",
                  json_string(cell.c_str()).c_str(), leaf_interface::copies,
                  leaf_interface::moves);
    }
  }
}

// Records the outcome of a request of Interface the binding of a
// registration answers, as the code compiles: published when its resolution
// is selected and its conversion available, unconvertible when the
// conversion is not, composed when it is a wrapper the binding makes of a
// value it resolves, unpublished when it is none of these, and diverged when
// the two containers select differently. A request the lookup sends to the
// binding of another interface is answered there. Only a published cell
// makes code of its types, which applies the conversion; any other records
// what was decided as the code compiles.
template <typename Model, typename Interface, typename Request, typename Leaf>
DINGO_ALWAYS_INLINE void publication(const char *prefix, const char *name) {
  using Storage = typename Model::storage_type;
  using type = unannotated_t<Interface>;
  using resolutions = typename detail::binding_resolutions<type, Storage>::type;
  using binding = detail::binding<type, Model>;
  using selected =
      typename detail::binding_supports_request<Request, binding>::type;
  constexpr std::size_t index = runtime_index_v<Request, resolutions>;
  constexpr bool listed = !std::is_void_v<selected>;
  constexpr bool matched = index < type_list_size_v<resolutions>;
  if constexpr (!selects_v<Request, type>) {
    record(prefix, name, "selects_another", nullptr);
  } else if constexpr (listed != matched) {
    record(prefix, name, "diverged",
           listed ? "the static container selects a resolution, the runtime "
                    "container none"
                  : "the runtime container selects a resolution, the static "
                    "container none");
  } else if constexpr (!listed) {
    constexpr detail::derivation how =
        detail::static_request_derivation_v<Request, binding>;
    if constexpr (!derives_alike<Request, type, binding, Storage,
                                 resolutions>()) {
      record(prefix, name, "diverged",
             "the static and the runtime container derive the request "
             "differently");
    } else if constexpr (how == detail::derivation::keep ||
                         how == detail::derivation::compose) {
      record(prefix, name, "composed", nullptr);
    } else if constexpr (how == detail::derivation::take) {
      record(prefix, name, "adopted", nullptr);
    } else if constexpr (how == detail::derivation::refer ||
                         how == detail::derivation::address ||
                         how == detail::derivation::move ||
                         how == detail::derivation::copy ||
                         how == detail::derivation::convert) {
      record(prefix, name, "derived", nullptr);
    } else if constexpr (how == detail::derivation::adopt ||
                         how == detail::derivation::place) {
      record(prefix, name, "placed", nullptr);
    } else {
      record(prefix, name, "unpublished", nullptr);
    }
  } else if constexpr (!conversion_available<
                           typename selected::operation>::value ||
                       !resolution_at_available<index, resolutions>::value) {
    record(prefix, name, "unconvertible", nullptr);
  } else {
    record_published<typename selected::operation, Request, Leaf,
                     typename Storage::type>(prefix, name);
  }
}

} // namespace dingo::model
