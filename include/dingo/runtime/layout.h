//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/runtime/operation_table.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

// What a runtime resolution records of a binding and of a request: the answers
// of the closure of a binding and its layer sources, and the types and tables
// of a request.

namespace dingo::detail::execution {

// How the route of a state of the closure of a binding gives a value the
// request takes: whether it makes it of what the route reaches, a copy of
// the qualifiers quals, rather than taking what the route reaches.
struct route_finish {
  bool copies = false;
  std::uint8_t quals = 0;
};

// The key of a type a request is of, without its own qualifiers, which a
// binding finds the answers of its closure for a request by.
template <typename T> inline constexpr char shape_key = 0;

// The type of the pointer to the first element of an array of T, which C++
// spells as a pointer to one T: a layer source of each is apart, as a handle
// adopts it as an array.
template <typename T> struct first_element {};

// How a state of the closure of a binding satisfies a request, as the code
// compiles: its route, whether it is the one of a value, how the request
// takes what it reaches and whether what it gives lives as long as the
// container.
struct closed_answer {
  bool found = false;
  bool value_route = false;
  bool stable_address = false;
  std::uint16_t route = 0;
  semantics::access access = semantics::access::refer;
  std::uint8_t quals = 0;
};

// The answers of a request, for each lifetime it may need.
using closed_answers = semantics::lifetime_pair<closed_answer>;

// The answers of the closure for the requests of the type of the key shape:
// of each category, of an object of each qualifiers, at
// first + category * 4 + qualifiers.
inline constexpr std::size_t shape_answer_count = std::size_t{7} * 4;

struct closed_group {
  const void *shape;
  std::uint16_t first;
};

// A layer source of a binding an extension of a request starts from: the key
// of its type, its form, its state of the closure, the copies and the moves of
// the leaf that reach it, and whether it may be absent.
struct layer_source {
  const void *shape = nullptr;
  std::uint16_t code = 0;
  std::uint16_t state = 0;
  semantics::cost made;
  bool absent = false;
};

// A binding as a runtime resolution converts it: whether it binds the
// abstract interface of its leaf; the table of the type of each state of the
// closure of its layer sources and how its route gives a value; the answers of
// the closure by the type a request is of; and the layer sources an extension
// of a request the closure does not satisfy starts from.
struct binding_layout {
  bool abstract;
  const type_table *const *tables;
  const route_finish *finishes;
  const closed_group *groups;
  std::size_t group_count;
  const closed_answers *answers;
  const layer_source *layer_sources;
  std::size_t layer_source_count;
};

// The types a request of Request spells, from its shape at depth 0 to its
// element, of which Element is the innermost.
template <typename Request, typename Element> struct request_types {
  using shape = typename requested_category<Request>::shape;
  static constexpr std::size_t depth =
      type_list_size_v<typename stack_t<shape, Element>::layers>;

  template <std::size_t K, typename T = shape> struct nested {
    using type = typename nested<K - 1, held_element_t<T, Element>>::type;
  };
  template <typename T> struct nested<0, T> {
    using type = T;
  };
};

// The tables of the types a request spells, which make the layers it makes.
template <typename Request, typename Element> struct request_tables {
  using types = request_types<Request, Element>;

  // Whether a type is a layer a request makes: of a wrapper or an
  // alternative type.
  template <typename T> static constexpr bool makes() {
    using type = std::remove_cv_t<T>;
    return !std::is_pointer_v<type> && !std::is_array_v<type> &&
           (is_alternative_type_v<type> || type_traits<type>::enabled);
  }

  // Whether a type is a pointer or a handle of a qualified element, which a
  // request converts one of fewer qualifiers to.
  template <typename T> static constexpr bool qualifies() {
    using type = std::remove_cv_t<T>;
    if constexpr (std::is_pointer_v<type>) {
      return element_quals<type>() != 0;
    } else if constexpr (!makes<type>() || is_alternative_type_v<type> ||
                         !is_complete_v<type>) {
      return false;
    } else {
      return wrapper_kind_v<layer_identity_t<type>>.qualifies &&
             element_quals<type>() != 0;
    }
  }

  template <typename T> static constexpr std::uint32_t type_mask() {
    return (makes<T>() ? makers_mask : 0) |
           (qualifies<T>() ? qualified_mask : 0);
  }

  template <std::size_t K>
  using table_t =
      operation_table<typename types::template nested<K>::type, Element,
                      Element,
                      type_mask<typename types::template nested<K>::type>()>;

  // A layer the semantics make of what it holds, as a wrapper the request
  // spells is made of its element value, adopts a pointer or observes one,
  // has the operation that makes it.
  template <std::size_t K> static constexpr bool checked() {
    using nested = typename types::template nested<K>::type;
    using type = std::remove_cv_t<nested>;
    // A pointer or a handle of a qualified element is made of one of each
    // fewer qualifiers, moved, and copied when it can be.
    if constexpr (qualifies<type>() && std::is_same_v<nested, type>) {
      using maker = typename table_t<K>::maker;
      constexpr std::uint8_t quals = element_quals<type>();
      constexpr bool copies = std::is_copy_constructible_v<type>;
      for (std::uint8_t adds = 1; adds < 4; ++adds) {
        if ((quals & adds) == adds &&
            (!maker::full.qualified[adds].has_owned ||
             (copies && !maker::full.qualified[adds].has_borrowed))) {
          return false;
        }
      }
    }
    // What the semantics make is of no qualifiers.
    if constexpr (!makes<type>() || is_alternative_type_v<type> ||
                  !is_complete_v<type> || !std::is_same_v<nested, type>) {
      return true;
    } else {
      constexpr semantics::layer_kind kind =
          wrapper_kind_v<layer_identity_t<type>>;
      using maker = typename table_t<K>::maker;
      using nested_type = typename types::template nested<K + 1>::type;
      return (!kind.wraps_value ||
              !layer_types<nested_type, Element,
                           Element>::template movable<nested_type>() ||
              has(maker::full.wrap_runs[1])) &&
             (!kind.observes || has(maker::full.observing));
    }
  }

  template <std::size_t... K>
  static constexpr auto make(std::index_sequence<K...>) {
    static_assert((checked<K>() && ...),
                  "a request has the operation that makes each layer it makes, "
                  "and that converts each pointer or handle to the qualifiers "
                  "it names");
    struct table {
      const type_table *value[sizeof...(K)];
    };
    return table{{&table_t<K>::value...}};
  }

  static constexpr auto value =
      make(std::make_index_sequence<types::depth + 1>{});
};

} // namespace dingo::detail::execution
