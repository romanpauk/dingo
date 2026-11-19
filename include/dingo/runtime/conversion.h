//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/request/conversion.h>
#include <dingo/request/extension.h>
#include <dingo/runtime/layout.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <type_traits>
#include <utility>

// The extensions of a request type, compiled into erased operations of the
// tables of the types it spells, as the code compiles: a runtime resolution
// takes the one of a layer source of the binding it resolves.

namespace dingo::detail::execution {

// The calls an extension takes at most.
inline constexpr std::size_t max_calls = 8;

// An extension as a runtime resolution runs it, after the route of the binding
// to the layer source: the operations of the instructions of its
// conversion, each with whether what it makes lives as long as the container;
// how it satisfies the request, how the request takes the value of what it
// reaches, and whether what it gives the request the address of lives as long
// as the container.
struct conversion_call {
  operation_function owned = nullptr;
  operation_function borrowed = nullptr;
  bool stable = false;
};

struct runtime_conversion {
  semantics::access access = semantics::access::refer;
  finish last = finish::satisfy;
  std::uint8_t depth = 0;
  std::uint8_t quals = 0;
  bool stable_address = false;
  // The table of the type of what the conversion reaches.
  const type_table *reached = nullptr;
  std::size_t count = 0;
  const conversion_call *calls = nullptr;
};

// The operation of a slot of a table, as a constant: none when the table has
// none. A table holds only the operations it has.
constexpr erased_operation table_operation(const type_table &table,
                                           std::uint8_t slot) {
  for (std::size_t index = 0; index < table.count; ++index) {
    if (table.entries[index].slot == slot) {
      return table.entries[index].operation;
    }
  }
  return {};
}

// The table a call of an extension is taken of: of the type the request
// spells at a depth, of the pointer in place of it, or of what the call
// before made.
enum class call_table : std::uint8_t { made, spelled, pointer };

// A call of an extension as the tables give it: the table and the slot of its
// operation. The calls of two extensions of the same keys are the same
// functions, which are not compared themselves: GCC does not compare the
// addresses of functions in a constant expression when it keeps null pointer
// checks, as -fsanitize=null makes it.
struct call_key {
  call_table table = call_table::made;
  std::uint8_t level = 0;
  std::uint8_t slot = 0;
};

// What compiling an extension gives: the conversion, the key of each call,
// whether it takes a step of the type of the layer source, whose table only
// the binding has, which no extension does, and whether a table it takes a
// step of has no operation of it, as of a type incomplete where the request
// is compiled.
struct compiled_extension {
  runtime_conversion conversion;
  conversion_call calls[max_calls] = {};
  call_key keys[max_calls] = {};
  bool unknown = false;
  bool missing = false;
};

// The conversion of the derivation of an extension, of the tables of the
// types the request spells, from its shape at depth 0: every step of it is
// the request's, made of a layer it spells or of what such a step made. A
// pointer in place of a handle the request spells, which the request converts
// to the qualifiers it spells below the pointer, is made of the table of
// pointers at the depth of the handle, where there is one (pointed).
constexpr compiled_extension
compile_extension(const semantics::derivation &derivation,
                  const type_table *const *tables,
                  const type_table *const *pointers, const bool *pointed,
                  std::size_t goal_depth) {
  compiled_extension made;
  runtime_conversion &converted = made.conversion;
  const conversion compiled = compile_conversion(derivation, 1);
  // The table of the type the call before made, none before the first.
  const type_table *table = nullptr;
  for (std::size_t index = 0; index < compiled.count; ++index) {
    const instruction &each = compiled.entries[index];
    const semantics::object &source = derivation.objects[each.first];
    const semantics::object &target = derivation.objects[each.last + 1];
    const std::size_t level = goal_depth - target.depth;
    const bool pointer = target.depth && pointed[level] &&
                         (target.layers[0] == semantics::pointer_layer ||
                          target.layers[0] == semantics::array_pointer_layer);
    const type_table &maker = pointer ? *pointers[level] : *tables[level];
    call_key key{pointer ? call_table::pointer : call_table::spelled,
                 static_cast<std::uint8_t>(level), 0};
    switch (each.kind) {
    case semantics::step::wrap:
      key.slot = operation_slot(slot::wrap_runs, each.wraps);
      break;
    case semantics::step::adopt:
      key.slot = target.layers[1] == semantics::array_layer && target.depth > 1
                     ? operation_slot(slot::adopted_array)
                     : operation_slot(slot::adopted);
      break;
    case semantics::step::observe:
      key.slot = operation_slot(slot::observing);
      break;
    case semantics::step::qualify:
      key.slot = operation_slot(
          slot::qualified,
          static_cast<std::uint8_t>(target.quals[1] & ~source.quals[1]));
      break;
    case semantics::step::release:
      key = {call_table::made, 0,
             source.layers[0] == semantics::array_pointer_layer
                 ? operation_slot(slot::release_array)
                 : operation_slot(each.kind)};
      break;
    default:
      key = {call_table::made, 0, operation_slot(each.kind)};
      break;
    }
    const bool makes = key.table != call_table::made;
    if (!makes && converted.count == 0) {
      made.unknown = true;
      return made;
    }
    const erased_operation step =
        table_operation(makes ? maker : *table, key.slot);
    if (!has(step)) {
      made.missing = true;
      return made;
    }
    if (converted.count == max_calls) {
      made.unknown = true;
      return made;
    }
    made.keys[converted.count] = key;
    made.calls[converted.count++] = {step.owned, step.borrowed,
                                     target.lifetimes[0] ==
                                         semantics::lifetime::stable};
    table = makes ? &maker : step.made;
  }
  converted.last = compiled.last;
  converted.depth = compiled.depth;
  converted.quals = compiled.quals;
  converted.reached = table;
  converted.access = derivation.access;
  const semantics::object &reached = derivation.objects[derivation.count];
  converted.stable_address =
      (derivation.access == semantics::access::point
           ? reached.lifetimes[1]
           : reached.lifetimes[0]) == semantics::lifetime::stable;
  return made;
}

// An extension of a request as a runtime resolution chooses it: the form of
// the layer source, and for a request that needs what it refers to to
// live as long as the container, and as long as the request, the cost of
// what it reaches, whether that may be absent, and its conversion, if any.
struct extension_entry {
  std::uint16_t form = 0;
  semantics::lifetime_pair<semantics::cost> fewest;
  semantics::lifetime_pair<bool> absent;
  semantics::lifetime_pair<const runtime_conversion *> conversions;
};

// The entries of a nested source of a request, by the key of its type.
struct extension_group {
  const void *shape = nullptr;
  std::uint16_t first = 0;
  std::uint16_t count = 0;
};

struct extension_table {
  const extension_group *groups = nullptr;
  std::size_t group_count = 0;
  const extension_entry *entries = nullptr;
};

// The table of a request that has no extension.
inline constexpr extension_table no_extensions{};

// A type of no qualifiers below its own: of none at any level it holds.
template <typename T, typename Element, typename = void> struct unqualified {
  using type = std::remove_cv_t<T>;
};
template <typename T, typename Element> struct unqualified<T *, Element, void> {
  using type = typename unqualified<std::remove_cv_t<T>, Element>::type *;
};
template <typename T, typename Element>
struct unqualified<
    T, Element,
    std::enable_if_t<!std::is_same_v<std::remove_cv_t<T>, Element> &&
                     !std::is_pointer_v<T> &&
                     type_traits<std::remove_cv_t<T>>::enabled>> {
  using held =
      std::remove_cv_t<typename type_traits<std::remove_cv_t<T>>::value_type>;
  using type = typename type_traits<std::remove_cv_t<T>>::template rebind_t<
      typename unqualified<held, Element>::type>;
};

// The extensions of a request type, of the abstract interface of the leaf
// (Abstract) or of the leaf itself, as a runtime resolution chooses among
// them: the entries of each nested source, by the key of its type, and the
// conversions of the entries, each once.
template <typename Request, bool Abstract> struct runtime_extensions {
  using extensions = request_extension<Request, Abstract>;
  using element_type = typename extensions::element_type;
  using types = request_types<Request, element_type>;
  using tables = request_tables<Request, element_type>;

  // Of a pointer nested source of the request: whether it points to the first
  // element of an array, the depth of the handle or the array it is in place
  // of, the element it points to of the qualifiers the request spells, and
  // the pointer of them.
  template <std::size_t Nested> struct nested_pointer {
    static constexpr auto group = extensions::nested.entries[Nested];
    static constexpr semantics::request goal = extensions::goal();
    static constexpr bool array =
        group.offset + 1u < goal.depth &&
        goal.layers[group.offset + 1] == semantics::array_layer;
    static constexpr std::size_t level = group.offset + (array ? 1u : 0u);
    using element =
        typename types::template nested<group.offset + (array ? 2 : 1)>::type;
    using type = element *;
  };

  // The key of the type of a nested source of the request.
  template <std::size_t Nested> static constexpr const void *nested_key() {
    constexpr auto group = extensions::nested.entries[Nested];
    if constexpr (group.kind == semantics::nested_kind::spelled) {
      return &shape_key<std::remove_cv_t<
          typename types::template nested<group.offset>::type>>;
    } else if constexpr (group.kind == semantics::nested_kind::unqualified) {
      return &shape_key<typename unqualified<
          typename types::template nested<group.offset>::type,
          element_type>::type>;
    } else {
      using spelled_element = typename nested_pointer<Nested>::element;
      // The pointer of no qualifiers below its own.
      using element = std::conditional_t<
          group.kind == semantics::nested_kind::unqualified_pointer,
          typename unqualified<spelled_element, element_type>::type,
          spelled_element>;
      if constexpr (nested_pointer<Nested>::array) {
        return &shape_key<first_element<element>>;
      } else {
        return &shape_key<element *>;
      }
    }
  }

  // The table of a pointer a request converts of an unqualified pointer nested
  // source, if the nested source is one.
  template <std::size_t Nested>
  static constexpr const type_table *pointer_table() {
    if constexpr (unqualified_pointer<Nested>()) {
      using type = typename nested_pointer<Nested>::type;
      return &operation_table<type, element_type, element_type,
                              tables::template type_mask<type>()>::value;
    } else {
      return nullptr;
    }
  }

  // Whether a nested source of the request is an unqualified pointer, of
  // which the request converts a pointer.
  template <std::size_t Nested> static constexpr bool unqualified_pointer() {
    return extensions::nested.entries[Nested].kind ==
           semantics::nested_kind::unqualified_pointer;
  }

  // The tables of pointers in place of the handles the request spells, by
  // their depth: of each one the request converts of an unqualified pointer
  // nested source, none at another depth; and whether a depth has one.
  struct pointer_list {
    const type_table *tables[types::depth + 1] = {};
    bool exists[types::depth + 1] = {};
  };
  template <std::size_t... I>
  static constexpr pointer_list make_pointers(std::index_sequence<I...>) {
    pointer_list made_pointers;
    const type_table *each[] = {pointer_table<I>()..., nullptr};
    const bool exists[] = {unqualified_pointer<I>()..., false};
    const std::size_t levels[] = {nested_level<I>()..., 0};
    for (std::size_t index = 0; index < sizeof...(I); ++index) {
      if (exists[index]) {
        made_pointers.tables[levels[index]] = each[index];
        made_pointers.exists[levels[index]] = true;
      }
    }
    return made_pointers;
  }
  template <std::size_t Nested> static constexpr std::size_t nested_level() {
    if constexpr (semantics::pointer_kind(
                      extensions::nested.entries[Nested].kind)) {
      return nested_pointer<Nested>::level;
    } else {
      return 0;
    }
  }
  static constexpr pointer_list pointers =
      make_pointers(std::make_index_sequence<extensions::nested.count>{});

  // An extension of the request as the code compiles it: the form of the layer
  // source, and for each lifetime whether it reaches the request, at what
  // cost, and the index of its conversion.
  struct entry {
    std::uint16_t form = 0;
    semantics::lifetime_pair<bool> found;
    semantics::lifetime_pair<semantics::cost> fewest;
    semantics::lifetime_pair<bool> absent;
    semantics::lifetime_pair<std::uint8_t> conversion;
  };
  static constexpr std::size_t max_entries = 256;
  static constexpr std::size_t max_conversions = 64;
  // The extensions of every nested source and every form of it, of one search
  // each, and their conversions, each once.
  struct extension_list {
    entry entries[max_entries] = {};
    std::size_t count = 0;
    std::uint16_t first[semantics::max_nested] = {};
    std::uint16_t counts[semantics::max_nested] = {};
    compiled_extension conversions[max_conversions] = {};
    std::size_t conversion_count = 0;
    std::size_t call_count = 0;
    bool overflow = false;
    bool unknown = false;
  };

  // Whether two extensions compile to the same conversion: of the same calls,
  // by their keys, which reach the same table.
  static constexpr bool same(const compiled_extension &first,
                             const compiled_extension &second) {
    const runtime_conversion &a = first.conversion;
    const runtime_conversion &b = second.conversion;
    if (a.access != b.access || a.last != b.last || a.depth != b.depth ||
        a.quals != b.quals || a.stable_address != b.stable_address ||
        a.count != b.count) {
      return false;
    }
    for (std::size_t index = 0; index < a.count; ++index) {
      const call_key &x = first.keys[index];
      const call_key &y = second.keys[index];
      if (x.table != y.table || x.level != y.level || x.slot != y.slot ||
          first.calls[index].stable != second.calls[index].stable) {
        return false;
      }
    }
    return true;
  }

  static constexpr extension_list make() {
    extension_list table_list;
    if constexpr (extensions::modeled) {
      const semantics::request goal = extensions::goal();
      for (std::size_t nested_index = 0;
           nested_index < extensions::nested.count; ++nested_index) {
        const auto &group = extensions::nested.entries[nested_index];
        table_list.first[nested_index] =
            static_cast<std::uint16_t>(table_list.count);
        const semantics::form_list forms = extensions::forms(group);
        for (std::size_t index = 0; index < forms.count; ++index) {
          const auto reached = extensions::extended(group, forms.forms[index]);
          table_list.overflow = table_list.overflow || reached.overflow;
          entry each;
          each.form = forms.forms[index];
          bool any = false;
          for (const semantics::lifetime needed :
               {semantics::lifetime::stable, semantics::lifetime::request}) {
            const semantics::derivation &derived = reached.derivations[needed];
            if (!derived.found) {
              continue;
            }
            const compiled_extension compiled =
                compile_extension(derived, tables::value.value, pointers.tables,
                                  pointers.exists, goal.depth);
            // An extension of a step no table has an operation of is none.
            if (compiled.missing) {
              continue;
            }
            any = true;
            each.found[needed] = true;
            each.fewest[needed] = derived.fewest;
            each.absent[needed] = derived.absent;
            table_list.unknown = table_list.unknown || compiled.unknown;
            std::size_t found = 0;
            while (found < table_list.conversion_count &&
                   !same(table_list.conversions[found], compiled)) {
              ++found;
            }
            if (found == table_list.conversion_count) {
              if (table_list.conversion_count == max_conversions) {
                table_list.unknown = true;
                continue;
              }
              table_list.conversions[table_list.conversion_count++] = compiled;
              table_list.call_count += compiled.conversion.count;
            }
            each.conversion[needed] = static_cast<std::uint8_t>(found);
          }
          if (any) {
            if (table_list.count == max_entries) {
              table_list.unknown = true;
            } else {
              table_list.entries[table_list.count++] = each;
            }
          }
        }
        table_list.counts[nested_index] = static_cast<std::uint16_t>(
            table_list.count - table_list.first[nested_index]);
      }
    }
    return table_list;
  }
  static constexpr extension_list made = make();
  static_assert(!made.overflow,
                "an extension reaches more states than extension_states, or "
                "takes more steps than semantics::max_steps");
  static_assert(!made.unknown,
                "an extension takes only steps of the types the request "
                "spells, of no more conversions and entries than a table "
                "holds");

  // The calls of the conversions, one after another.
  static constexpr std::array<conversion_call, made.call_count + 1>
  make_calls() {
    std::array<conversion_call, made.call_count + 1> table_list{};
    std::size_t offset = 0;
    for (std::size_t index = 0; index < made.conversion_count; ++index) {
      for (std::size_t call = 0;
           call < made.conversions[index].conversion.count; ++call) {
        table_list[offset++] = made.conversions[index].calls[call];
      }
    }
    return table_list;
  }
  static constexpr std::array<conversion_call, made.call_count + 1> calls =
      make_calls();

  static constexpr runtime_conversion conversion_v(std::size_t index) {
    std::size_t offset = 0;
    for (std::size_t each = 0; each < index; ++each) {
      offset += made.conversions[each].conversion.count;
    }
    runtime_conversion converted = made.conversions[index].conversion;
    converted.calls = calls.data() + offset;
    return converted;
  }
  template <std::size_t... I>
  static constexpr std::array<runtime_conversion, sizeof...(I)>
  make_conversions(std::index_sequence<I...>) {
    return {{conversion_v(I)...}};
  }
  static constexpr std::array<runtime_conversion, made.conversion_count>
      conversions =
          make_conversions(std::make_index_sequence<made.conversion_count>{});

  static constexpr extension_entry entry_v(std::size_t index) {
    const entry &each = made.entries[index];
    extension_entry made_entry;
    made_entry.form = each.form;
    made_entry.fewest = each.fewest;
    made_entry.absent = each.absent;
    for (const semantics::lifetime needed :
         {semantics::lifetime::stable, semantics::lifetime::request}) {
      made_entry.conversions[needed] =
          each.found[needed] ? &conversions[each.conversion[needed]] : nullptr;
    }
    return made_entry;
  }
  template <std::size_t... I>
  static constexpr std::array<extension_entry, sizeof...(I)>
  make_entries(std::index_sequence<I...>) {
    return {{entry_v(I)...}};
  }
  static constexpr std::array<extension_entry, made.count> entries =
      make_entries(std::make_index_sequence<made.count>{});

  template <std::size_t... I>
  static constexpr std::array<extension_group, sizeof...(I)>
  make_groups(std::index_sequence<I...>) {
    return {{{nested_key<I>(), made.first[I], made.counts[I]}...}};
  }
  static constexpr std::array<extension_group, extensions::nested.count>
      groups =
          make_groups(std::make_index_sequence<extensions::nested.count>{});

  static constexpr extension_table value{
      groups.data(), extensions::nested.count, entries.data()};

  // Whether the request has no extension.
  static constexpr bool empty() { return made.count == 0; }

  // The table of the request: one of no extension shared by every request
  // that has none.
  static constexpr const extension_table *table() {
    if constexpr (empty()) {
      return &no_extensions;
    } else {
      return &value;
    }
  }
};

} // namespace dingo::detail::execution
