//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/request/binding_semantics.h>
#include <dingo/request/operation.h>

#include <cassert>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

// The conversion a derivation is compiled into, which both containers run;
// the derivation of a request of a binding; and the execution of a
// conversion as the code compiles, which applies its operations one after
// another, then gives the request what they reach.

namespace dingo::detail::execution {

// A conversion: a derivation as both containers run it, of instructions, each
// of the derivation steps from first to last, which it takes at once, with
// whether the binding takes them or the request; and how the request takes the
// value of what the last of them reaches. The containers take the decisions of
// how to run a derivation here alone:
//
// - A run of wraps is one instruction, made at once of the value it wraps,
//   of the steps of one side.
// - A layer a request makes of a value or of a pointer is made of the
//   qualifiers its element needs at once: the conversion to them that follows
//   it is the same instruction.
// - A value a request takes is made where it takes it when the last step
//   makes it: a copy or a run of wraps the request makes is left to it; and a
//   copy of the element of a layer that may be empty, which the binding
//   makes, is made by the request of what maybe_element gives.

// How a request takes the value of what a conversion reaches: as the derivation
// satisfies it, or by making it of what the conversion reached, a copy of it or
// the value a run of wraps makes of it.
enum class finish : std::uint8_t { satisfy, copy, run };

// An instruction of a conversion: the derivation steps from first to last, the
// kind it takes, and whether the binding takes it; the wraps of a run of wraps,
// and whether a conversion to a more qualified element ends it.
struct instruction {
  semantics::step kind = semantics::step::copy;
  std::uint8_t first = 0;
  std::uint8_t last = 0;
  bool binding = false;
  std::uint8_t wraps = 0;
  bool qualified = false;
};

struct conversion {
  instruction entries[semantics::max_steps] = {};
  std::size_t count = 0;
  finish last = finish::satisfy;
  // The wraps of a run the request makes last.
  std::uint8_t depth = 0;
  // The qualifiers of what the conversion reaches, which a copy is made of.
  std::uint8_t quals = 0;
};

// The conversion of a found derivation of what a binding publishes, whose first
// source_count states are its layer sources: the steps into them are the
// binding's.
DINGO_COLD constexpr conversion
compile_conversion(const semantics::derivation &derivation,
                   std::size_t source_count) {
  using semantics::step;
  conversion made;
  const bool value = derivation.access == semantics::access::move ||
                     derivation.access == semantics::access::copy;
  // Whether the binding takes the derivation step at an index.
  auto binding_takes = [&](std::size_t index) {
    return derivation.states[index + 1] < source_count;
  };
  std::size_t index = 0;
  while (index < derivation.count) {
    const step kind = derivation.steps[index];
    std::size_t last = index;
    while (kind == step::wrap && last + 1 < derivation.count &&
           derivation.steps[last + 1] == step::wrap &&
           binding_takes(last + 1) == binding_takes(index)) {
      ++last;
    }
    const auto wraps =
        static_cast<std::uint8_t>(kind == step::wrap ? last - index + 1 : 0);
    bool qualified = false;
    if ((kind == step::wrap || kind == step::adopt || kind == step::observe) &&
        last + 1 < derivation.count &&
        derivation.steps[last + 1] == step::qualify && !binding_takes(last)) {
      ++last;
      qualified = true;
    }
    const bool binding = binding_takes(last);
    if (last + 1 == derivation.count && value) {
      if (binding && kind == step::maybe_element_copy) {
        made.entries[made.count++] = {step::maybe_element,
                                      static_cast<std::uint8_t>(index),
                                      static_cast<std::uint8_t>(last),
                                      true,
                                      0,
                                      false};
        made.last = finish::copy;
        made.quals = derivation.objects[index].quals[1];
        return made;
      }
      if (!binding && kind == step::copy) {
        made.last = finish::copy;
        made.quals = derivation.objects[index].quals[0];
        return made;
      }
      if (!binding && kind == step::wrap && !qualified) {
        made.last = finish::run;
        made.depth = wraps;
        return made;
      }
    }
    made.entries[made.count++] = {kind,
                                  static_cast<std::uint8_t>(index),
                                  static_cast<std::uint8_t>(last),
                                  binding,
                                  wraps,
                                  qualified};
    index = last + 1;
  }
  return made;
}

// A request of a stored object, derived as the code compiles: the C++
// properties of the stored type Stored, of the request type Request and of the
// leaf Leaf, classified as the semantics model them, and the derivation of the
// request for each lifetime it may need: as long as the container, when what it
// refers to is passed to the caller, or as long as the request. The request
// is derived from what the binding publishes, in a universe of the layers of
// the binding, which those of the request follow.
template <typename Stored, typename Request, typename Leaf, bool Makes,
          bool Early>
struct request_derivation {
  using binding = binding_semantics<Stored, Leaf, Makes, Early>;
  using category = requested_category<Request>;
  using shape = typename category::shape;
  using stored_stack = typename binding::stored_stack;
  using request_stack = stack_t<shape, Leaf>;
  using universe_type =
      layer_universe<typename joined<typename binding::layers,
                                     typename request_stack::layers>::type>;
  using identities = typename universe_type::identities;
  using leaf_type = Leaf;
  using interface_type = innermost_t<shape, Leaf>;

  static constexpr bool abstract =
      request_stack::element == stack_element::abstract;

  static constexpr bool modeled =
      category::modeled && stored_stack::element != stack_element::other &&
      request_stack::element != stack_element::other;

  static constexpr semantics::context context() {
    semantics::context context = binding::context(abstract);
    context.layers = &universe_type::value;
    return context;
  }

  static constexpr semantics::request goal() {
    semantics::request made;
    place_layers<universe_type>(made.layers, typename request_stack::layers{});
    made.depth = static_cast<std::uint8_t>(
        type_list_size_v<typename request_stack::layers>);
    made.element = semantic_element(request_stack::element);
    stack_quals<shape, Leaf>::fill(made.quals);
    return made;
  }

  // The derivations of a request that needs what it refers to to live as long
  // as the container, and as long as the request, of one search: they differ
  // in what satisfies the request alone.
  struct derivations {
    semantics::derivation stable;
    semantics::derivation transient;
  };

  static constexpr derivations make() {
    if constexpr (!modeled) {
      return {};
    } else {
      static_assert(binding::template whole<abstract>());
      constexpr semantics::context closed_context = context();
      const semantics::request request = goal();
      const semantics::search found = semantics::reach_closed(
          binding::template closed<abstract>,
          binding::context(abstract).layers->count, closed_context, request);
      return {semantics::answer_derivation(
                  found, semantics::satisfy(found, request, category::value,
                                            semantics::lifetime::stable,
                                            closed_context)),
              semantics::answer_derivation(
                  found, semantics::satisfy(found, request, category::value,
                                            semantics::lifetime::request,
                                            closed_context))};
    }
  }

  static constexpr derivations derived = make();
  static constexpr semantics::derivation stable = derived.stable;
  static constexpr semantics::derivation transient = derived.transient;

  // The conversions of the derivations.
  static constexpr std::size_t source_count =
      binding::template sources<abstract>.count;
  static constexpr conversion stable_conversion =
      compile_conversion(stable, source_count);
  static constexpr conversion transient_conversion =
      compile_conversion(transient, source_count);
};

// The C++ step of a step of the semantics, from the object Semantic::source()
// to the object Semantic::reached(), to the layers of a universe of identities.
template <typename Identities, typename Semantic> struct step_operation {
  static constexpr semantics::step kind = Semantic::kind;
  static constexpr semantics::object reached = Semantic::reached();

  template <semantics::layer Layer>
  using layer_type = type_list_element_t<Layer - 4, Identities>;

  static auto make() {
    if constexpr (kind == semantics::step::wrap) {
      return operation<semantics::step::wrap, layer_type<reached.layers[0]>>{};
    } else if constexpr (kind == semantics::step::release) {
      // The pointer is to the first element of an array when the step
      // before decayed or handed over an array.
      return release_operation<Semantic::source().layers[0] ==
                               semantics::array_pointer_layer>{};
    } else if constexpr (kind == semantics::step::adopt) {
      return adopt_operation<layer_type<reached.layers[0]>,
                             reached.layers[1] == semantics::array_layer>{};
    } else if constexpr (kind == semantics::step::observe) {
      return observe_operation<layer_type<reached.layers[0]>>{};
    } else if constexpr (kind == semantics::step::qualify) {
      return qualify_operation<reached.quals[1]>{};
    } else {
      return operation<kind>{};
    }
  }
  using operation_type = decltype(make());
};

// The step at an index of a derivation.
template <const semantics::derivation &Derivation, std::size_t Index>
struct derivation_step {
  static constexpr semantics::step kind = Derivation.steps[Index];
  static constexpr semantics::object source() {
    return Derivation.objects[Index];
  }
  static constexpr semantics::object reached() {
    return Derivation.objects[Index + 1];
  }
};

// The step into a state of a search.
template <const semantics::search &Search, std::size_t State>
struct search_step {
  static constexpr semantics::step kind = Search.states[State].step;
  static constexpr semantics::object source() {
    return Search.states[Search.states[State].parent].reached;
  }
  static constexpr semantics::object reached() {
    return Search.states[State].reached;
  }
};

// The C++ step of the step at an index of a derivation.
template <const semantics::derivation &Derivation, typename Identities,
          std::size_t Index>
using derivation_step_t =
    typename step_operation<Identities,
                            derivation_step<Derivation, Index>>::operation_type;

// The type a run of Count wraps of a derivation, from the derivation step
// First, makes of a value of T.
template <const semantics::derivation &Derivation, typename Identities,
          typename Leaf, typename Interface, typename T, std::size_t First,
          std::size_t Count>
struct wrapped_run {
  using type = typename wrapped_run<
      Derivation, Identities, Leaf, Interface,
      typename derivation_step_t<Derivation, Identities,
                                 First>::template wrapped_t<Leaf, Interface, T>,
      First + 1, Count - 1>::type;
};

template <const semantics::derivation &Derivation, typename Identities,
          typename Leaf, typename Interface, typename T, std::size_t First>
struct wrapped_run<Derivation, Identities, Leaf, Interface, T, First, 0> {
  using type = T;
};

// A conversion of a derivation, run as the code compiles: each instruction
// applies its step to what the one before reached, and the request takes its
// value.
template <const semantics::derivation &Derivation, const conversion &Conversion,
          typename Identities, typename Leaf, typename Interface>
struct typed_conversion {
  template <typename Request, std::size_t Index = 0, typename Carrier,
            typename Store>
  static Request execute(Carrier source, Store &store) {
    if constexpr (Index == Conversion.count) {
      return finished<Request>(source);
    } else {
      return execute<Request, Index + 1>(apply<Index>(source, store), store);
    }
  }

  // What the conversion reaches, before the request takes its value.
  template <std::size_t Index = 0, typename Carrier, typename Store>
  static auto reach(Carrier source, Store &store) {
    if constexpr (Index == Conversion.count) {
      return source;
    } else {
      return reach<Index + 1>(apply<Index>(source, store), store);
    }
  }

  template <std::size_t Index, typename T, bool Owned, typename Store>
  static auto apply(carrier<T, Owned> source, Store &store) {
    constexpr instruction Each = Conversion.entries[Index];
    constexpr semantics::object reached = Derivation.objects[Each.last + 1];
    auto &&kept =
        lifetime_store<reached.lifetimes[0] == semantics::lifetime::stable>(
            store);
    if constexpr (Each.kind == semantics::step::wrap) {
      using made_type = typename layer_wrapper<T, Index>::type;
      auto &made = kept.template keep<made_type>([&] {
        return wrap_value<Leaf, Interface, made_type>(
            std::move(*source.object));
      });
      return carrier<made_type, true>{std::addressof(made)};
    } else if constexpr (Each.qualified) {
      using operation_type = typename holding_operation<
          derivation_step_t<Derivation, Identities, Each.first>,
          reached.quals[1]>::type;
      return operation_type::template apply<Leaf, Interface>(source, kept);
    } else if constexpr (Each.kind == Derivation.steps[Each.first]) {
      return derivation_step_t<Derivation, Identities, Each.first>::
          template apply<Leaf, Interface>(source, kept);
    } else {
      // A step the conversion takes in place of the derivation's.
      return operation<Each.kind>::template apply<Leaf, Interface>(source,
                                                                   kept);
    }
  }

  // The type a run of wraps makes of a value of T, of the qualifiers its
  // element needs when a conversion to them ends it.
  template <typename T, std::size_t Index> struct layer_wrapper {
    static constexpr instruction Each = Conversion.entries[Index];
    using wrapped =
        typename wrapped_run<Derivation, Identities, Leaf, Interface, T,
                             Each.first, Each.wraps>::type;
    template <typename U> struct tag {
      using type = U;
    };
    static auto make() {
      if constexpr (Each.qualified) {
        return tag<requalified_t<wrapped,
                                 Derivation.objects[Each.last + 1].quals[1]>>{};
      } else {
        return tag<wrapped>{};
      }
    }
    using type = typename decltype(make())::type;
  };

  // The request of what the conversion reached.
  template <typename Request, typename T, bool Owned>
  static Request finished(carrier<T, Owned> reached) {
    using semantics::access;
    if constexpr (Conversion.last == finish::copy) {
      return Request(*reached.object);
    } else if constexpr (Conversion.last == finish::run) {
      using made_type =
          typename wrapped_run<Derivation, Identities, Leaf, Interface, T,
                               Derivation.count - Conversion.depth,
                               Conversion.depth>::type;
      return static_cast<Request>(
          wrap_value<Leaf, Interface, made_type>(std::move(*reached.object)));
    } else if constexpr (Derivation.access == access::refer) {
      if constexpr (std::is_pointer_v<Request>) {
        return reached.object;
      } else {
        return static_cast<Request>(*reached.object);
      }
    } else if constexpr (Derivation.access == access::point) {
      return *reached.object;
    } else if constexpr (Derivation.access == access::take) {
      return static_cast<Request>(std::move(*reached.object));
    } else if constexpr (Derivation.access == access::move) {
      return Request(std::move(*reached.object));
    } else {
      return Request(*reached.object);
    }
  }
};

// Executes the conversion of a request_derivation for a lifetime, as the code
// compiles: as long as the container (Stable), or as long as the request.
template <typename Request, typename Derivation, bool Stable, typename Carrier,
          typename Store>
Request execute_conversion(Carrier source, Store &store) {
  if constexpr (Stable) {
    return typed_conversion<
        Derivation::stable, Derivation::stable_conversion,
        typename Derivation::identities, typename Derivation::leaf_type,
        typename Derivation::interface_type>::template execute<Request>(source,
                                                                        store);
  } else {
    return typed_conversion<
        Derivation::transient, Derivation::transient_conversion,
        typename Derivation::identities, typename Derivation::leaf_type,
        typename Derivation::interface_type>::template execute<Request>(source,
                                                                        store);
  }
}

} // namespace dingo::detail::execution
