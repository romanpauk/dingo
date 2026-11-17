//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/request/conversion.h>
#include <dingo/runtime/layout.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

// The conversion of a request of a binding, compiled into erased operations
// the first time a request of its type is resolved.

namespace dingo::detail::execution {

// A conversion as a runtime resolution runs it: the operations of the
// instructions of its conversion, each with whether what it makes lives as long
// as the container; how it satisfies the request, how the request takes the
// value of what it reaches, and whether what it gives the request the address
// of lives as long as the container.
struct runtime_conversion {
  struct call {
    operation_function owned;
    operation_function borrowed;
    bool stable;
  };
  bool found = false;
  // The route of the binding the conversion takes first, to a state of its
  // closure, and whether it is the one of a value.
  std::uint16_t route = 0;
  bool value_route = false;
  semantics::access access = semantics::access::refer;
  finish last = finish::satisfy;
  std::uint8_t depth = 0;
  std::uint8_t quals = 0;
  bool stable_address = false;
  // The table of the type of what the conversion reaches.
  const type_table *reached = nullptr;
  std::size_t count = 0;
  call calls[semantics::max_steps] = {};
};

// The conversions of a request of a binding, for a request that needs what it
// refers to to live as long as the container, and for one that needs it to
// live as long as the request: of one search, in the universe of the
// binding, which the layers of the request it does not have follow, from the
// binding's layer sources.
struct runtime_conversions {
  runtime_conversion stable;
  runtime_conversion transient;
};

inline runtime_conversion make_runtime_conversion(
    const semantics::derivation &derivation, const binding_layout &binding,
    const request_layout &request, const semantics::request &goal);

DINGO_COLD inline runtime_conversions
make_runtime_conversions(const binding_layout &binding,
                         const request_layout &request) {
  runtime_conversions made;
  // The layers of the request in the universe of the binding: those of the
  // binding at their index, any other after them.
  semantics::layer mapped[semantics::max_kinds] = {0, 1, 2, 3};
  bool foreign = false;
  for (std::size_t index = 0; index < request.layer_count && !foreign;
       ++index) {
    std::size_t binding_index = 0;
    while (binding_index < binding.layer_count &&
           binding.layers[binding_index].identity !=
               request.layers[index].identity) {
      ++binding_index;
    }
    foreign = binding_index == binding.layer_count;
    mapped[4 + index] = static_cast<semantics::layer>(4 + binding_index);
  }
  semantics::context context = binding.context;
  if (foreign) {
    // Of the thread, as the search is: a universe holds the kinds of every
    // standard layer, which making one copies.
    thread_local semantics::universe merged;
    // The universe of the binding, which the layers of the request it does
    // not have follow, and the conversions between them all.
    merged = *binding.context.layers;
    const void *identities[semantics::max_kinds] = {};
    const void *targets[semantics::max_kinds] = {};
    const void *unknown_bound_targets[semantics::max_kinds] = {};
    for (std::size_t index = 0; index < 4; ++index) {
      identities[index] = language_identities[index];
    }
    for (std::size_t index = 0; index < binding.layer_count; ++index) {
      identities[4 + index] = binding.layers[index].identity;
      targets[4 + index] = binding.layers[index].sharing_identity;
      unknown_bound_targets[4 + index] =
          binding.layers[index].unknown_bound_identity;
    }
    for (std::size_t index = 0; index < request.layer_count; ++index) {
      const runtime_layer &layer = request.layers[index];
      std::size_t merged_index = 4;
      while (merged_index < merged.count &&
             identities[merged_index] != layer.identity) {
        ++merged_index;
      }
      if (merged_index == merged.count) {
        if (merged.count == semantics::max_kinds) {
          return made;
        }
        identities[merged_index] = layer.identity;
        targets[merged_index] = layer.sharing_identity;
        unknown_bound_targets[merged_index] = layer.unknown_bound_identity;
        merged.add(layer.kind);
      }
      mapped[4 + index] = static_cast<semantics::layer>(merged_index);
    }
    for (std::size_t index = 4; index < merged.count; ++index) {
      merged.kinds[index].transfers = 0;
      merged.kinds[index].converts_unknown_bound = 0;
      for (std::size_t target = 4; target < merged.count; ++target) {
        if (targets[index] != nullptr && identities[target] == targets[index]) {
          merged.kinds[index].transfers |= std::uint32_t{1} << target;
        }
        if (unknown_bound_targets[index] != nullptr &&
            identities[target] == unknown_bound_targets[index]) {
          merged.kinds[index].converts_unknown_bound |= std::uint32_t{1}
                                                        << target;
        }
      }
    }
    context.layers = &merged;
  }
  semantics::request goal = request.goal;
  for (std::size_t index = 0; index < goal.depth; ++index) {
    goal.layers[index] = mapped[goal.layers[index]];
  }
  goal.element = context.abstract ? semantics::element::abstract
                                  : semantics::element::leaf;

  // The search of a thread, cleared rather than made: what makes one is its
  // states, which a search reads only once it added them. Deriving runs no
  // code of a user, so it is never deriving already on the thread.
  thread_local semantics::search found;
  found.clear();
  // The closed states are distinct already, so they are not hashed: an object
  // the request reaches again is only a state more.
  for (std::size_t index = 0; index < binding.state_count; ++index) {
    found.states[index] = binding.sources[index];
  }
  found.count = binding.state_count;
  found.closed = binding.state_count;
  semantics::extend_reach(found, binding.state_count,
                          binding.context.layers->count, context, goal);
  // The derivations of the lifetimes differ in what satisfies the request
  // alone.
  made.stable = make_runtime_conversion(
      semantics::answer_derivation(
          found, semantics::satisfy(found, goal, request.category,
                                    semantics::lifetime::stable, context)),
      binding, request, goal);
  made.transient = make_runtime_conversion(
      semantics::answer_derivation(
          found, semantics::satisfy(found, goal, request.category,
                                    semantics::lifetime::request, context)),
      binding, request, goal);
  return made;
}

// A conversion as a runtime resolution runs it: a derivation compiled into the
// operations of the tables of the types it steps through.
inline runtime_conversion make_runtime_conversion(
    const semantics::derivation &derivation, const binding_layout &binding,
    const request_layout &request, const semantics::request &goal) {
  runtime_conversion made;
  if (!derivation.found) {
    return made;
  }

  const conversion compiled =
      compile_conversion(derivation, binding.source_count);
  // The instructions into states of the closure are the binding's route;
  // the rest the request takes, of the tables of the types it spells.
  std::size_t first = 0;
  while (first < compiled.count &&
         derivation.states[compiled.entries[first].last + 1] <
             binding.state_count) {
    ++first;
  }
  if (compiled.last == finish::copy && first == compiled.count &&
      derivation.states[derivation.count] < binding.state_count) {
    made.route = derivation.states[derivation.count];
    made.value_route = true;
  } else {
    made.route = first ? derivation.states[compiled.entries[first - 1].last + 1]
                       : std::uint16_t{0};
  }
  // The table of the type of what the conversion reached so far: of a state of
  // the closure, only of one a step after its route takes an operation of.
  const type_table *table = binding.tables[made.route];
  for (std::size_t index = first; index < compiled.count; ++index) {
    const instruction &each = compiled.entries[index];
    const semantics::object &source = derivation.objects[each.first];
    const semantics::object &target = derivation.objects[each.last + 1];
    const std::uint16_t state = derivation.states[each.last + 1];
    // The table of what an instruction makes a layer of: of a source of the
    // binding, or of a type the request spells, by its depth in the request.
    const bool tabled = state < binding.source_count;
    const type_table *maker = tabled
                                  ? binding.tables[state]
                                  : request.tables[goal.depth - target.depth];
    const erased_operation *step = nullptr;
    switch (each.kind) {
    case semantics::step::wrap:
      step = &(*maker)[operation_slot(slot::wrap_runs, each.wraps)];
      break;
    case semantics::step::adopt:
      step = &(
          *maker)[target.layers[1] == semantics::array_layer && target.depth > 1
                      ? operation_slot(slot::adopted_array)
                      : operation_slot(slot::adopted)];
      break;
    case semantics::step::observe:
      step = &(*maker)[operation_slot(slot::observing)];
      break;
    case semantics::step::qualify:
      step = &(*maker)[operation_slot(
          slot::qualified,
          static_cast<std::uint8_t>(target.quals[1] & ~source.quals[1]))];
      break;
    case semantics::step::release:
      step = &(*table)[source.layers[0] == semantics::array_pointer_layer
                           ? operation_slot(slot::release_array)
                           : operation_slot(each.kind)];
      break;
    default:
      if (table == nullptr) {
        return {};
      }
      step = &(*table)[operation_slot(each.kind)];
      break;
    }
    made.calls[made.count++] = {step->owned, step->borrowed,
                                target.lifetimes[0] ==
                                    semantics::lifetime::stable};
    // The table of what a step of the binding makes is that of its source;
    // of what a step of a request makes, that its operation names.
    table = tabled ? binding.tables[state]
            : (each.kind == semantics::step::wrap ||
               each.kind == semantics::step::adopt ||
               each.kind == semantics::step::observe ||
               each.kind == semantics::step::qualify)
                ? maker
                : step->made;
  }
  made.last = compiled.last;
  made.depth = compiled.depth;
  made.quals = compiled.quals;
  made.found = true;
  made.reached = table;
  made.access = derivation.access;
  const semantics::object &reached = derivation.objects[derivation.count];
  made.stable_address =
      (derivation.access == semantics::access::point
           ? reached.lifetimes[1]
           : reached.lifetimes[0]) == semantics::lifetime::stable;
  return made;
}

} // namespace dingo::detail::execution
