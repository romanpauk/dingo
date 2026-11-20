//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/request/conversion.h>
#include <dingo/runtime/closure.h>
#include <dingo/runtime/conversion.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

// The routes of a binding over the closure of its layer sources, compiled
// for its types: to each state an answer is or a request starts from.

namespace dingo::detail::execution {

template <typename Semantics, const semantics::search &Sources,
          typename Identities, typename Stored, typename Leaf,
          typename Interface>
struct binding_sources {
  static constexpr bool abstract = !std::is_same_v<Leaf, Interface>;
  static_assert(Semantics::template whole<abstract>());
  using semantic =
      closed_semantics<typename Semantics::template shape<abstract>>;
  static constexpr const state_classes &classes = semantic::classes;
  static constexpr const auto &used = semantic::used;
  static constexpr const auto &masks = semantic::masks;
  static constexpr const semantics::search &closure = semantic::closure;
  static constexpr const auto &closed = semantic::closed;
  static constexpr const auto &route_finishes = semantic::route_finishes;
  template <std::size_t State, bool Value>
  static constexpr const semantics::derivation &route_derivation =
      semantic::template route_derivation<State, Value>;
  template <std::size_t State, bool Value>
  static constexpr const conversion &route_conversion =
      semantic::template route_conversion<State, Value>;
  static constexpr std::size_t run_length(std::size_t state) {
    return semantic::run_length(state);
  }

  template <std::size_t State>
  using reaching_operation =
      typename step_operation<Identities,
                              search_step<Sources, State>>::operation_type;

  template <typename U> struct tag {
    using type = U;
  };

  // The carrier a step into a state makes of the carrier Source.
  template <std::size_t State, typename Source>
  using reached_t =
      decltype(reaching_operation<State>::template apply<Leaf, Interface>(
          std::declval<Source>(), std::declval<store_view &>()));

  // The carrier of a class of states.
  template <std::size_t Class> struct class_carrier {
    static auto make() {
      if constexpr (Class == 0) {
        return tag<carrier<Stored, semantics::owned(
                                       Sources.states[0].reached.origin)>>{};
      } else {
        return tag<reached_t<
            Class,
            typename class_carrier<
                classes.state_class[Sources.states[Class].parent]>::type>>{};
      }
    }
    using type = typename decltype(make())::type;
  };

  template <std::size_t State>
  using table_t = operation_table<
      typename class_carrier<classes.state_class[State]>::type::type, Leaf,
      Interface, masks.state_mask[classes.state_class[State]]>;

  // A published state, renumbered, of the table of its type; the step into
  // it is checked to be an operation of the table it is taken from.
  template <std::size_t State> static constexpr semantics::state source() {
    if constexpr (State != 0) {
      constexpr semantics::state target = Sources.states[State];
      using source_carrier =
          typename class_carrier<classes.state_class[target.parent]>::type;
      static_assert(
          std::is_same_v<
              reached_t<State, source_carrier>,
              typename class_carrier<classes.state_class[State]>::type>,
          "a step into a state makes the type of its class");
      if constexpr (step_makes(target.step)) {
        using made = typename table_t<State>::maker;
        constexpr const semantics::object &reached =
            Sources.states[State].reached;
        static_assert(target.step == semantics::step::wrap
                          ? made::full.wrap_runs[run_length(State)].has_owned
                      : target.step == semantics::step::observe
                          ? has(made::full.observing)
                          : has(reached.depth > 1 && reached.layers[1] ==
                                                         semantics::array_layer
                                    ? made::full.adopted_array
                                    : made::full.adopted),
                      "the type a binding makes a layer of is made so");
      }
    }
    // Every step a request takes of it C++ makes of its type: the closure's,
    // which its routes take, and those of its table.
    using own = typename class_carrier<classes.state_class[State]>::type;
    constexpr std::uint32_t kinds = request_kinds(Sources.states[State].reached,
                                                  Semantics::context(abstract));
    constexpr const full_table &table = table_t<State>::maker::full;
    static_assert(
        makes<own, semantics::step::copy>(kinds) &&
            makes<own, semantics::step::decay>(kinds) &&
            makes<own, semantics::step::hand_over>(kinds) &&
            known<own>(table, kinds, semantics::step::transfer) &&
            known<own>(table, kinds, semantics::step::unknown_bound),
        "the type of a source makes every step a request takes of it");
    semantics::state made = Sources.states[State];
    if (made.parent != semantics::no_state) {
      made.parent = used.index[made.parent];
    }
    return made;
  }

  // Whether C++ makes a step of a kind of a mask of the carrier Own.
  template <typename Own, semantics::step Kind>
  static constexpr bool makes(std::uint32_t kinds) {
    return !(kinds & (std::uint32_t{1} << static_cast<unsigned>(Kind))) ||
           operation_result<operation<Kind>, Leaf, Interface,
                            typename Own::type, Own::owned>::possible;
  }

  // Whether a table has a step of a kind of a mask, for the carrier Own.
  template <typename Own>
  static constexpr bool known(const full_table &table, std::uint32_t kinds,
                              semantics::step kind) {
    if (!(kinds & (std::uint32_t{1} << static_cast<unsigned>(kind)))) {
      return true;
    }
    const erased_operation &operation =
        table.operations[static_cast<std::size_t>(kind)];
    return Own::owned ? operation.has_owned : operation.has_borrowed;
  }

  // The table a runtime resolution reads of a published state: the address
  // what it points to is at, of no operation; a request takes its steps of
  // the tables of the types it spells.
  template <std::size_t State>
  using published_table = operation_table<
      typename class_carrier<classes.state_class[State]>::type::type, Leaf,
      Interface, 0>;

  template <std::size_t... I>
  static constexpr auto publish(std::index_sequence<I...>) {
    struct table {
      semantics::state states[sizeof...(I)];
      const type_table *tables[sizeof...(I)];
    };
    return table{{source<used.states[I]>()...},
                 {&published_table<used.states[I]>::value...}};
  }

  static constexpr auto published =
      publish(std::make_index_sequence<used.count>{});

  // The carrier of each state of the closure, and whether C++ makes it: a
  // published state is of the class of its type; what a step of a request
  // makes of a state is, when the types of the step make it.
  // The stored object is of the carrier of its class, also of a binding of
  // which no request takes anything.
  template <std::size_t State,
            bool Published = (State < used.count || State == 0)>
  struct closed_carrier {
    static constexpr bool possible = true;
    // MSVC does not parse a subscript in a subscript of a template argument.
    static constexpr std::size_t state = used.states[State];
    using type = typename class_carrier<classes.state_class[state]>::type;
  };
  template <std::size_t State> struct closed_carrier<State, false> {
    using parent = closed_carrier<closure.states[State].parent>;
    using operation_type = typename step_operation<
        Identities, search_step<semantic::closure, State>>::operation_type;
    template <bool Made, typename = void> struct reached_carrier {
      static constexpr bool possible = false;
      using type = void;
    };
    template <typename Unused> struct reached_carrier<true, Unused> {
      using result =
          operation_result<operation_type, Leaf, Interface,
                           typename parent::type::type, parent::type::owned>;
      static constexpr bool possible = result::possible;
      using type = typename result::type;
    };
    static constexpr bool possible =
        reached_carrier<parent::possible>::possible;
    using type = typename reached_carrier<parent::possible>::type;
  };

  using start_carrier = typename closed_carrier<0>::type;

  // The step into a state of the closure.
  template <std::size_t State>
  using closed_step = typename step_operation<
      Identities, search_step<semantic::closure, State>>::operation_type;

  // What the route of a state reaches: the last instruction of its conversion,
  // applied to what the route of the state it starts from reaches, so that
  // each state compiles one step, and the routes share what leads to them.
  // The instruction is the step into the state, or of the state before it
  // with the qualifiers of the state, when the semantics make it at once; or
  // the step the conversion takes in place of it.
  template <std::size_t State, bool Value, typename Store>
  static auto reach_state(start_carrier source, Store &store) {
    constexpr const conversion &made = route_conversion<State, Value>;
    if constexpr (made.count == 0) {
      return source;
    } else {
      constexpr const semantics::derivation &derivation =
          route_derivation<State, Value>;
      constexpr instruction each = made.entries[made.count - 1];
      constexpr std::uint16_t before = derivation.states[each.first];
      constexpr std::uint16_t first = derivation.states[each.first + 1];
      constexpr semantics::object reached = derivation.objects[each.last + 1];
      auto &&kept =
          lifetime_store<reached.lifetimes[0] == semantics::lifetime::stable>(
              store);
      auto reached_before = reach_state<before, false, Store>(source, store);
      if constexpr (each.qualified) {
        using operation_type =
            typename holding_operation<closed_step<first>,
                                       reached.quals[1]>::type;
        return operation_type::template apply<Leaf, Interface>(reached_before,
                                                               kept);
      } else if constexpr (each.kind == derivation.steps[each.first]) {
        return closed_step<first>::template apply<Leaf, Interface>(
            reached_before, kept);
      } else {
        return operation<each.kind>::template apply<Leaf, Interface>(
            reached_before, kept);
      }
    }
  }

  template <std::size_t State, bool Value, typename Store>
  static erased_carrier route(start_carrier source, Store &store) {
    return erase(reach_state<State, Value, Store>(source, store));
  }

  // Runs the route of a state, of a value when Value: the routes are
  // compiled in place, chosen among by the state.
  template <bool Value, typename Store, std::size_t... I>
  DINGO_ALWAYS_INLINE static erased_carrier
  run_route(std::size_t state, start_carrier source, Store &store,
            std::index_sequence<I...>) {
    erased_carrier reached;
    using fn = void (*)(start_carrier, Store &, erased_carrier &);
    static constexpr fn routes[] = {&run_state_route<I, Value, Store>...};
    routes[state](source, store, reached);
    return reached;
  }
  template <std::size_t State, bool Value, typename Store>
  static void run_state_route(start_carrier source, Store &store,
                              erased_carrier &reached) {
    if constexpr (closed_carrier<State>::possible &&
                  (Value ? answer_table::routed.value[State]
                         : answer_table::routed.through[State])) {
      reached = route<State, Value, Store>(source, store);
    }
  }
  template <bool Value, typename Store>
  DINGO_ALWAYS_INLINE static erased_carrier
  run_route(std::size_t state, start_carrier source, Store &store) {
    return run_route<Value>(state, source, store,
                            std::make_index_sequence<closure.count>{});
  }

  // Whether C++ makes each state of the closure.
  template <std::size_t... I>
  static constexpr auto make_possible(std::index_sequence<I...>) {
    struct list {
      bool entries[sizeof...(I)];
    };
    return list{{closed_carrier<I>::possible...}};
  }
  static constexpr auto possible_states =
      make_possible(std::make_index_sequence<closure.count>{});

  // The answers of the closure, of the states C++ makes, as bits.
  static constexpr std::size_t possible_words = (closure.count + 63) / 64;
  static constexpr std::uint64_t possible_word(std::size_t word) {
    std::uint64_t made = 0;
    for (std::size_t index = 0; index < 64 && word * 64 + index < closure.count;
         ++index) {
      if (possible_states.entries[word * 64 + index]) {
        made |= std::uint64_t{1} << index;
      }
    }
    return made;
  }
  template <std::size_t... I>
  static auto make_answer_table(std::index_sequence<I...>)
      -> closed_answer_table<semantic, possible_word(I)...>;
  using answer_table =
      decltype(make_answer_table(std::make_index_sequence<possible_words>{}));
  static constexpr const auto &shapes = answer_table::shapes;
  static constexpr const auto &answers = answer_table::answers;

  // The table of the type of each state of the closure an answer or an
  // extension may give what it points to of; none of another.
  template <std::size_t State>
  static constexpr const type_table *closed_table() {
    if constexpr (State < used.count) {
      return published.tables[State];
    } else if constexpr (!closed_carrier<State>::possible ||
                         !(answer_table::pointed.entries[State] ||
                           answer_table::routed.through[State])) {
      return nullptr;
    } else {
      using own = typename closed_carrier<State>::type;
      return &operation_table<typename own::type, Leaf, Interface, 0>::value;
    }
  }
  template <std::size_t... I>
  static constexpr auto make_tables(std::index_sequence<I...>) {
    struct list {
      const type_table *entries[sizeof...(I)];
    };
    return list{{closed_table<I>()...}};
  }
  static constexpr auto closed_tables =
      make_tables(std::make_index_sequence<closure.count>{});

  // The key of the type of a shape: of a state, or what it points to.
  template <std::size_t Shape> static constexpr const void *shape_type_key() {
    using type = typename closed_carrier<shapes.states[Shape]>::type::type;
    if constexpr (shapes.pointee[Shape]) {
      return &shape_key<std::remove_cv_t<std::remove_pointer_t<type>>>;
    } else {
      return &shape_key<std::remove_cv_t<type>>;
    }
  }
  template <std::size_t... I>
  static constexpr auto make_groups(std::index_sequence<I...>) {
    struct list {
      closed_group entries[sizeof...(I) + 1];
    };
    return list{{{shape_type_key<I>(),
                  static_cast<std::uint16_t>(I * shape_answer_count)}...,
                 {}}};
  }
  static constexpr auto groups =
      make_groups(std::make_index_sequence<shapes.count>{});

  // The key of the type of a layer source: of a pointer to the first element
  // of an array apart from one to a single object.
  template <typename T, bool FirstElement>
  static constexpr const void *source_key() {
    if constexpr (FirstElement && std::is_pointer_v<T>) {
      return &shape_key<first_element<std::remove_pointer_t<T>>>;
    } else {
      return &shape_key<T>;
    }
  }

  // Whether the tables of a request have the form of a layer source: every
  // form source_forms enumerates of its type, of a pointer as a handle that
  // adopts it and one that observes it would.
  static constexpr bool enumerated(const semantics::object &object) {
    const bool pointer =
        object.depth && (object.layers[0] == semantics::pointer_layer ||
                         object.layers[0] == semantics::array_pointer_layer);
    const semantics::form_list forms =
        semantics::source_forms(object, pointer, true, true);
    const std::uint16_t code = semantics::object_form(object).code();
    for (std::size_t index = 0; index < forms.count; ++index) {
      if (forms.forms[index] == code) {
        return true;
      }
    }
    return false;
  }

  // The layer sources an extension of a request starts from: the key of the
  // type of each, of no qualifiers of its own.
  template <std::size_t Index> static constexpr layer_source layer_source_v() {
    constexpr std::size_t state = answer_table::layer_sources.states[Index];
    using type = typename closed_carrier<state>::type::type;
    constexpr const semantics::state &each = closure.states[state];
    // A constant of its own: MSVC 19.4x fails on the call in the assertion.
    constexpr bool known_form = enumerated(each.reached);
    static_assert(known_form,
                  "the tables of a request have every form of a layer source "
                  "a binding publishes: semantics::source_forms enumerates it");
    return {each.reached.depth &&
                    each.reached.layers[0] == semantics::array_pointer_layer
                ? source_key<std::remove_cv_t<type>, true>()
                : source_key<std::remove_cv_t<type>, false>(),
            semantics::object_form(each.reached).code(),
            static_cast<std::uint16_t>(state), each.made, each.reached.absent};
  }
  template <std::size_t... I>
  static constexpr auto make_layer_sources(std::index_sequence<I...>) {
    struct list {
      layer_source entries[sizeof...(I) + 1];
    };
    return list{{layer_source_v<I>()..., {}}};
  }
  static constexpr auto layer_sources = make_layer_sources(
      std::make_index_sequence<answer_table::layer_sources.count>{});
};

// The layer sources a binding publishes, as its execution Execution derives
// them, and the tables of their types: the layout a runtime resolution
// selects a conversion of.
template <typename Execution> struct published_sources {
  using semantics_type = typename Execution::semantics_type;
  static constexpr bool abstract = Execution::abstract;
  using identities = typename semantics_type::universe_type::identities;
  using sources = binding_sources<
      semantics_type, semantics_type::template shape<abstract>::sources,
      identities, typename Execution::stored_type,
      typename Execution::leaf_type, typename Execution::interface_type>;
  static constexpr binding_layout value{
      abstract,
      sources::closed_tables.entries,
      sources::route_finishes.entries,
      sources::groups.entries,
      sources::shapes.count,
      sources::answers.entries,
      sources::layer_sources.entries,
      sources::answer_table::layer_sources.count};
};

// The layout of a binding, as its execution Execution derives it: none of a
// binding that is not modeled.
template <typename Execution>
constexpr const binding_layout *published_layout() {
  if constexpr (Execution::modeled) {
    return &published_sources<Execution>::value;
  } else {
    return nullptr;
  }
}

} // namespace dingo::detail::execution
