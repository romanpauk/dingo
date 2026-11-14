//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/type_descriptor.h>

namespace dingo {
namespace detail {

// What the materialization of the source of a binding depends on, of its
// storage and its scope: whether it is guarded against recursion on every
// materialization or while its storage is unresolved, whether an unresolved
// storage retains the source it materializes, whether the scope consumes what
// it makes, whether the storage yields a value rather than what it keeps, and
// whether what it stores is moved trivially. Both containers materialize a
// source by the decisions of these facts below: the static container as it
// compiles, the runtime engine as it resolves.
struct materialization_facts {
  bool guards_always;
  bool guards_unresolved;
  bool retains_unresolved;
  bool consumes;
  bool yields_value;
  bool moves_trivially;
};

// The shape of a request a source is materialized for: whether it refers,
// as an lvalue reference or a pointer, and whether it is an rvalue reference.
struct materialized_request {
  bool refers;
  bool rvalue;
};

constexpr materialized_request
materialized_request_of(type_descriptor requested) {
  return {requested.reference == type_reference_kind::lvalue ||
              (requested.reference == type_reference_kind::none &&
               requested.pointee != nullptr),
          requested.reference == type_reference_kind::rvalue};
}

// Where a source is materialized: as the storage yields it, in the context of
// the resolution, or, for a request that refers, as a value placed in the
// context.
enum class source_placement {
  yielded,
  in_context,
  value_in_context,
};

// Whether the materialization of a source is guarded against recursion.
constexpr bool materialization_guarded(const materialization_facts &facts,
                                       bool resolved) {
  return facts.guards_always || (facts.guards_unresolved && !resolved);
}

// Whether a source is materialized persistently and retained in the
// transaction of a runtime resolution, which resets the storage if it fails:
// only while the storage is unresolved.
constexpr bool materialization_retains(const materialization_facts &facts,
                                       bool resolved, bool transactional) {
  return transactional && facts.retains_unresolved && !resolved;
}

// Where the source of a request is materialized. A transactional context
// places every source a scope consumes. Any other places a value the storage
// yields for a request that refers to it, and the source of an rvalue request
// to what a scope consumes, which refers to what it is converted to, unless it
// is moved trivially, which costs nothing.
constexpr source_placement
materialization_placement(const materialization_facts &facts,
                          materialized_request request, bool transactional) {
  if (transactional) {
    return facts.consumes ? source_placement::in_context
                          : source_placement::yielded;
  }
  if (request.refers && facts.yields_value) {
    return source_placement::value_in_context;
  }
  if (facts.consumes && request.rvalue && !facts.moves_trivially) {
    return source_placement::in_context;
  }
  return source_placement::yielded;
}

// Whether the caller is refused an address into what a source is placed as:
// a value the storage yields lives only as long as the context, which ends
// before the caller uses an address into it.
constexpr bool refuses_caller_address(const materialization_facts &facts,
                                      materialized_request request) {
  return facts.yields_value && request.refers;
}

} // namespace detail
} // namespace dingo
