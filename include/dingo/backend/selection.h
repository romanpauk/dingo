//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The selection of a container: the interpreter of the selection rules of
// the spec over its sources, its static and runtime bindings, its parent,
// auto-construction of an unregistered type, and the factory of a request to
// construct an object. A step whose sources all know
// what they name when the request is compiled is decided then, and reported
// at compile time when no runtime source can change it; any other step is
// decided when the request is resolved. spec: selection_rules
//
// `Self` derives from selection<Self> and declares its sources:
// `has_static_source` with static_source(), `has_runtime_source` with
// runtime_source(), `parent_type`, void without a parent, with parent(),
// which may be null, and the local bindings of a registration,
// `has_local_static_source` with local_static_source() and
// `has_local_runtime_source` with local_runtime_source(), which may be null.
// Its container sources resolve with container_resolver(), and it keeps the
// objects requests construct in retained().

#include <dingo/backend/diagnose.h>
#include <dingo/base/mechanics.h>
#include <dingo/conversion/construct.h>
#include <dingo/conversion/key.h>
#include <dingo/conversion/key_kinds.gen.h>
#include <dingo/conversion/request.h>
#include <dingo/conversion/selection_rules.gen.h>
#include <dingo/diagnostics/diagnostic_message.h>
#include <dingo/memory/retained.h>
#include <dingo/type/annotated.h>
#include <dingo/type/auto_constructible.h>
#include <dingo/type/collection_traits.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo::core {

inline constexpr std::size_t unknown_count = static_cast<std::size_t>(-1);

// Adds an element by `append` and records that it added one, and how many
// it added. One is made by the outermost collection request and passed to
// every level, so a level of any depth instantiates the same functions.
// The steps after a request a container answers alone: none.
struct nothing_further {};

// The steps of a child after its parent, which the parent takes when no
// ancestor names a binding.
template <typename Request> class selection_rest {
public:
  template <typename Rest>
  explicit selection_rest(Rest &rest)
      : rest_(&rest), call_([](void *erased) -> Request {
          return (*static_cast<Rest *>(erased))();
        }) {}

  Request operator()() const { return call_(rest_); }

private:
  void *rest_;
  Request (*call_)(void *);
};

template <typename Append> struct counting_append {
  Append &append;
  bool added = false;
  std::size_t appended = 0;

  template <typename Collection, typename Element>
  void operator()(Collection &collection, Element &&element) {
    added = true;
    ++appended;
    append(collection, std::forward<Element>(element));
  }

  // Reserves room for `count` more elements than it added.
  template <typename Collection>
  void reserve(Collection &collection, std::size_t count) const {
    reserve_collection(collection, appended + count);
  }
};

template <typename Request> struct annotated_request;

// The result of a request: the type a keyed or an annotated request decorates,
// and otherwise the request.
template <typename Request> struct resolved {
  using type = Request;
};

template <typename Type, typename Key> struct resolved<keyed<Type, Key>> {
  using type = Type;
};

template <typename Type, typename Tag>
struct resolved<::dingo::annotated<Type, Tag>> {
  using type = Type;
};

template <typename Request> using resolved_t = typename resolved<Request>::type;

template <typename Type, typename Tag>
struct annotated_request<::dingo::annotated<Type, Tag>> {
  using type = Type;
  using tag = Tag;
};

template <typename Self> class selection {
public:
  // Resolves a request; a keyed or an annotated request names the type it
  // decorates, which is the result. The constructions of a request are the
  // transactions of their containers. The entry points of a request are
  // inlined, so a static resolution compiles to the code of its route.
  template <typename Request>
  DINGO_ALWAYS_INLINE resolved_t<Request> resolve() {
    return resolve_request<Request>();
  }

  // Resolves a request by a key: a key_type object, a key_value, or a value
  // of the key type.
  template <typename Request, typename Value>
  DINGO_ALWAYS_INLINE resolved_t<Request> resolve(Value &&value) {
    using key = request_key_of<std::decay_t<Value>>;
    if constexpr (is_annotated<Request>::value) {
      return select<typename annotated_request<Request>::type,
                    annotated_key<typename annotated_request<Request>::tag,
                                  typename key::type>,
                    true>(key::value(value));
    } else {
      return select<Request, typename key::type, true>(key::value(value));
    }
  }

  // The argument of a parameter of type `Request`: a keyed or an annotated
  // parameter takes the decorated object.
  template <typename Request> DINGO_ALWAYS_INLINE Request argument() {
    if constexpr (is_annotated<Request>::value || is_keyed<Request>::value) {
      return Request(resolve<Request>());
    } else {
      return resolve<Request>();
    }
  }

  // Constructs an object of `Type` by the construct rule: a binding of the
  // type answers it, and otherwise its detected factory makes it. A wrapper
  // of an element whose binding answers the request but cannot serve the
  // wrapper is made from a copy of the bound element.
  template <typename Type> DINGO_ALWAYS_INLINE Type construct_by_rule() {
    if constexpr (wraps_element<Type>()) {
      using element = leaf_of_t<Type>;
      if constexpr (static_binding_refuses<Type>()) {
        return wrap_element<Type>(construct_by_rule<element>());
      } else {
        try {
          return select<Type, no_key, true, selection_mode::construct>(nullptr);
        } catch (const ::dingo::type_not_convertible_exception &) {
          return wrap_element<Type>(construct_by_rule<element>());
        }
      }
    } else {
      return select<Type, no_key, true, selection_mode::construct>(nullptr);
    }
  }

  // The collection of every binding a request of its element type by `Key`
  // names, by the collection rule, each added by `append`.
  template <typename Collection, typename Key, typename Append>
  Collection collect(const void *key_value, Append &&append) {
    return self().transactions().run([&]() -> Collection {
      Collection result;
      counting_append<std::remove_reference_t<Append>> counted{append};
      collect_into<Collection, Key>(result, key_value, counted);
      return result;
    });
  }

  // The number of bindings a request names in this container or its
  // ancestors, by the singular rule without auto-construction: 0, 1, or 2.
  template <typename Request, typename Key>
  std::size_t inherited_count(const void *key_value) {
    return inherited_count_at<Request, Key, 0>(key_value);
  }

  // The count inherited_count() returns when it is known when the request is
  // compiled, else unknown_count: the first step that names a binding, or two
  // the sources known then name, decides it.
  template <typename Request, typename Key, std::size_t Step = 0>
  static constexpr std::size_t inherited_known() {
    if constexpr (Step == singular.step_count) {
      return 0;
    } else if constexpr (singular.steps[Step].when != step_condition::always) {
      return inherited_known<Request, Key, Step + 1>();
    } else {
      constexpr std::size_t known =
          known_step_count<Request, Key, selection_mode::singular, Step>();
      if constexpr (known == unknown_count) {
        return known_step_least<Request, Key, selection_mode::singular, Step>(
                   std::make_index_sequence<
                       singular.steps[Step].source_count>()) > 1
                   ? 2
                   : unknown_count;
      } else if constexpr (known != 0) {
        return known < 2 ? known : 2;
      } else {
        return inherited_known<Request, Key, Step + 1>();
      }
    }
  }

  // Resolves a request from this container or its ancestors, without
  // auto-construction.
  template <typename Request, typename Key>
  Request resolve_inherited(const void *key_value) {
    return select<Request, Key, false>(key_value);
  }

  // Resolves a request from this container or its ancestors, without
  // auto-construction, and by `otherwise` when none of them names a binding.
  template <typename Request, typename Key>
  Request select_inherited(const void *key_value,
                           selection_rest<Request> &otherwise) {
    return select<Request, Key, false>(key_value, otherwise);
  }

  // The number of bindings a request names among the container's own
  // bindings, its local and container sources without its parent: 0, 1, or 2.
  template <typename Request, typename Key>
  std::size_t own_count(const void *key_value) {
    return own_count_at<Request, Key, 0>(key_value);
  }

  // Resolves a request from the container's own bindings.
  template <typename Request, typename Key>
  Request resolve_own(const void *key_value) {
    return resolve_own_at<Request, Key, 0>(key_value);
  }

  // Adds the container's own bindings a collection request names to
  // `collection` through `counted`, a counting_append, by the collection rule
  // without its parent.
  template <typename Collection, typename Key, typename Counted>
  void collect_own(Collection &collection, const void *key_value,
                   Counted &counted) {
    collect_steps<Collection, Key, false>(collection, key_value, counted);
  }

  // Adds the bindings a collection request names by the collection rule to
  // `collection` through `counted`, a counting_append.
  template <typename Collection, typename Key, typename Counted>
  void collect_into(Collection &collection, const void *key_value,
                    Counted &counted) {
    collect_steps<Collection, Key, true>(collection, key_value, counted);
  }

  // Collects by the steps of the collection rule, with or without the
  // parent, until a step adds a binding when the rule takes the first that
  // does.
  template <typename Collection, typename Key, bool WithParent,
            typename Counted>
  void collect_steps(Collection &collection, const void *key_value,
                     Counted &counted) {
    constexpr const selection_rule &rule = rule_of(selection_mode::collection);
    for (std::size_t step = 0; step < rule.step_count; ++step) {
      const bool before = counted.added;
      counted.added = false;
      for (std::size_t source = 0; source < rule.steps[step].source_count;
           ++source) {
        if (WithParent ||
            rule.steps[step].sources[source] != selection_source::parent) {
          collect_source<Collection, Key>(rule.steps[step].sources[source],
                                          collection, key_value, counted);
        }
      }
      const bool added = counted.added;
      counted.added = before || added;
      if (rule.first_non_empty && added) {
        return;
      }
    }
  }

private:
  // Whether `Type` is a value of a wrapper whose element is a copyable leaf
  // the wrapper is made from.
  template <typename Type> static constexpr bool wraps_element() {
    if constexpr (std::is_reference_v<Type> || std::is_pointer_v<Type>) {
      return false;
    } else {
      using traits = wrapper_traits<std::remove_cv_t<Type>>;
      if constexpr (has_properties_v<Type, property::mapped> &&
                    !has_properties_v<Type, property::variadic>) {
        using element = type_list_at_t<0, typename traits::arguments>;
        return !is_wrapper_v<element> && std::is_copy_constructible_v<element>;
      } else {
        return false;
      }
    }
  }

  // Whether a static binding of the element of `Type` names the request of
  // `Type` and cannot serve it, which is decided when it is compiled.
  template <typename Type> static constexpr bool static_binding_refuses() {
    if constexpr (Self::has_static_source) {
      return Self::static_source_type::template fixed_count<Type, no_key>() ==
                 1 &&
             !Self::static_source_type::template serves_named<Type, no_key>();
    } else {
      return false;
    }
  }

  template <typename Type, typename Element>
  static Type wrap_element(Element &&element) {
    return wrapper_traits<std::remove_cv_t<Type>>::make(
        std::forward<Element>(element));
  }

  template <typename Request>
  DINGO_ALWAYS_INLINE resolved_t<Request> resolve_request() {
    if constexpr (is_annotated<Request>::value) {
      using inner = typename annotated_request<Request>::type;
      using key =
          annotated_key<typename annotated_request<Request>::tag, no_key>;
      return select<inner, key, true>(nullptr);
    } else if constexpr (is_keyed<Request>::value) {
      using key = typename Request::key;
      return select<typename Request::type, key, true>(
          key_store<key>().value());
    } else {
      return select<Request, no_key, true>(nullptr);
    }
  }

  static constexpr const selection_rule &singular =
      rule_of(selection_mode::singular);

public:
  // Whether every dependency without a key the factories of `Bindings`
  // declare names a binding, unless the container decides only when it is
  // resolved, which reports a dependency that names none then; one that names
  // none when the container is compiled is reported then. A binding with
  // local bindings, which may answer its dependencies, is not checked. spec:
  // diagnostics.type_not_found_required_by
  template <typename... Bindings>
  static constexpr bool
  declared_dependencies_named(type_list<Bindings...> /*bindings*/) {
    return (dependencies_named<Bindings>() && ...);
  }

private:
  template <typename Binding> static constexpr bool dependencies_named() {
    using arguments = typename declared_arguments<
        typename Binding::factory_type,
        leaf_of_t<typename Binding::stored_type>>::type;
    if constexpr (std::is_void_v<arguments> ||
                  !std::is_same_v<typename Binding::locals, type_list<>>) {
      return true;
    } else {
      return arguments_named<Binding>(arguments());
    }
  }

  template <typename Binding, typename... Arguments>
  static constexpr bool arguments_named(type_list<Arguments...> /*arguments*/) {
    return (argument_named<Binding, Arguments>() && ...);
  }

  template <typename Binding, typename Argument>
  static constexpr bool argument_named() {
    using request = detail::argument_request<Argument>;
    using type = typename request::type;
    using key = typename request::key;
    if constexpr (std::is_same_v<key, no_key> &&
                  !is_collection_request<type>() &&
                  names_none_when_compiled<type, key, 0>()) {
      fail<diag::type_not_found_required_by, type,
           dependency_path<leaf_of_t<typename Binding::stored_type>>>();
    }
    return true;
  }

  // Whether every step a request takes is known to name no binding when it
  // is compiled.
  template <typename Request, typename Key, std::size_t Step>
  static constexpr bool names_none_when_compiled() {
    if constexpr (Step == singular.step_count) {
      return true;
    } else if constexpr (!meets<Request, Key, true>(
                             singular.steps[Step].when)) {
      return names_none_when_compiled<Request, Key, Step + 1>();
    } else {
      return known_step_count<Request, Key, selection_mode::singular, Step>() ==
                 0 &&
             names_none_when_compiled<Request, Key, Step + 1>();
    }
  }

  Self &self() { return static_cast<Self &>(*this); }

  // Whether a request meets the condition of a step; only the outermost
  // container takes a conditional step.
  template <typename Request, typename Key, bool Outermost>
  static constexpr bool meets(step_condition condition) {
    constexpr bool unkeyed = std::is_same_v<Key, no_key>;
    switch (condition) {
    case step_condition::always:
      return true;
    case step_condition::constructible:
      return Outermost && unkeyed && auto_constructs_by<Request, Self>();
    case step_condition::has_factory:
      return Outermost && unkeyed && has_detected_factory<Request>::value;
    case step_condition::collection:
      return is_collection_request<Request>();
    }
    return false;
  }

  // Whether a request is of a collection type, by value, whose collection
  // traits add an element of its element type: only such a collection is
  // collected from the bindings of its element type.
  template <typename Request> static constexpr bool is_collection_request() {
    using value_type = typename request_traits<Request>::value_type;
    if constexpr (std::is_reference_v<Request> ||
                  !collection_traits<value_type>::is_collection) {
      return false;
    } else {
      return appends_elements_v<value_type>;
    }
  }

  // The number of bindings a source names when the request is compiled, or
  // unknown_count when it depends on the container or the key at runtime.
  template <typename Request, typename Key, selection_source Source>
  static constexpr std::size_t known_count() {
    if constexpr (Source == selection_source::container_static) {
      if constexpr (!Self::has_static_source) {
        return 0;
      } else {
        return value_count<Key>(
            Self::static_source_type::template fixed_count<Request, Key>());
      }
    } else if constexpr (Source == selection_source::container_runtime) {
      return Self::has_runtime_source ? unknown_count : 0;
    } else if constexpr (Source == selection_source::parent) {
      // A parent decides when the request is compiled what it decides then.
      if constexpr (std::is_void_v<typename Self::parent_type>) {
        return 0;
      } else {
        return Self::parent_type::template inherited_known<Request, Key>();
      }
    } else if constexpr (Source == selection_source::auto_construct ||
                         Source == selection_source::factory) {
      return 1;
    } else if constexpr (Source == selection_source::elements) {
      if constexpr (Self::has_runtime_source ||
                    Self::has_local_runtime_source ||
                    Key::descriptor.kind == key_kind::runtime ||
                    !is_collection_request<Request>()) {
        return is_collection_request<Request>() ? unknown_count : 0;
      } else {
        return elements_known<Request, Key>() ? 1 : 0;
      }
    } else if constexpr (Source == selection_source::local_static) {
      if constexpr (!Self::has_local_static_source) {
        return 0;
      } else {
        return value_count<Key>(
            Self::local_static_source_type::template fixed_count<Request,
                                                                 Key>());
      }
    } else {
      return Self::has_local_runtime_source ? unknown_count : 0;
    }
  }

  // The count of static bindings by `Key`: a runtime key value picks among
  // the bindings of its domain when it is resolved, so their number is not
  // the count unless there are none.
  template <typename Key>
  static constexpr std::size_t value_count(std::size_t domain_count) {
    if constexpr (Key::descriptor.kind == key_kind::runtime) {
      return domain_count == 0 ? 0 : unknown_count;
    } else {
      return domain_count;
    }
  }

  // The bindings the sources of step `Step` known when the request is
  // compiled name, at least.
  template <typename Request, typename Key, selection_mode Mode,
            std::size_t Step, std::size_t... Sources>
  static constexpr std::size_t
  known_step_least(std::index_sequence<Sources...> /*sources*/) {
    constexpr std::size_t counts[] = {
        known_count<Request, Key,
                    rule_of(Mode).steps[Step].sources[Sources]>()...,
        0};
    std::size_t total = 0;
    for (std::size_t index = 0; index < sizeof...(Sources); ++index) {
      total += counts[index] == unknown_count ? 0 : counts[index];
    }
    return total;
  }

  // Whether a step that names two bindings when the request is compiled
  // decides its ambiguity: every step before it is known to name none, so
  // only registrations at it remain, which add candidates.
  template <typename Request, typename Key, bool Outermost, selection_mode Mode,
            std::size_t Step, std::size_t Before = 0>
  static constexpr bool closed_before() {
    if constexpr (Before == Step) {
      return true;
    } else if constexpr (!meets<Request, Key, Outermost>(
                             rule_of(Mode).steps[Before].when)) {
      return closed_before<Request, Key, Outermost, Mode, Step, Before + 1>();
    } else {
      return known_step_count<Request, Key, Mode, Before>() == 0 &&
             closed_before<Request, Key, Outermost, Mode, Step, Before + 1>();
    }
  }

  template <typename Request, typename Key, selection_mode Mode,
            std::size_t Step, std::size_t... Sources>
  static constexpr std::size_t
  known_step_count(std::index_sequence<Sources...> /*sources*/) {
    constexpr std::size_t counts[] = {
        known_count<Request, Key,
                    rule_of(Mode).steps[Step].sources[Sources]>()...,
        0};
    std::size_t total = 0;
    for (std::size_t index = 0; index < sizeof...(Sources); ++index) {
      if (counts[index] == unknown_count) {
        return unknown_count;
      }
      total += counts[index];
    }
    return total;
  }

  template <typename Request, typename Key, selection_mode Mode,
            std::size_t Step>
  static constexpr std::size_t known_step_count() {
    return known_step_count<Request, Key, Mode, Step>(
        std::make_index_sequence<rule_of(Mode).steps[Step].source_count>());
  }

  // Whether every step of the rule is decided when the request is compiled.
  template <typename Request, typename Key, selection_mode Mode,
            std::size_t Step = 0>
  static constexpr bool decided_when_compiled() {
    if constexpr (Step == rule_of(Mode).step_count) {
      return true;
    } else {
      return known_step_count<Request, Key, Mode, Step>() != unknown_count &&
             decided_when_compiled<Request, Key, Mode, Step + 1>();
    }
  }

  template <typename Request, typename Key>
  std::size_t source_count(selection_source source, const void *key_value) {
    switch (source) {
    case selection_source::container_static:
      if constexpr (Self::has_static_source) {
        return self().static_source().template count<Request, Key>(key_value);
      }
      return 0;
    case selection_source::container_runtime:
      if constexpr (Self::has_runtime_source) {
        return self().runtime_source().template count<Request, Key>(key_value);
      }
      return 0;
    case selection_source::parent:
      if constexpr (!std::is_void_v<typename Self::parent_type>) {
        if (self().parent() != nullptr) {
          return self().parent()->template inherited_count<Request, Key>(
              key_value);
        }
      }
      return 0;
    case selection_source::auto_construct:
    case selection_source::factory:
      return 1;
    case selection_source::elements:
      if constexpr (is_collection_request<Request>()) {
        using element = typename collection_traits<
            typename request_traits<Request>::value_type>::resolve_type;
        return own_count<element, Key>(key_value) != 0 ? 1 : 0;
      }
      return 0;
    case selection_source::local_static:
      if constexpr (Self::has_local_static_source) {
        return self().local_static_source().template count<Request, Key>(
            key_value);
      }
      return 0;
    case selection_source::local_runtime:
      if constexpr (Self::has_local_runtime_source) {
        if (self().local_runtime_source() != nullptr) {
          return self().local_runtime_source()->template count<Request, Key>(
              key_value);
        }
      }
      return 0;
    }
    return 0;
  }

  template <typename Request, typename Key, selection_mode Mode,
            std::size_t Step>
  std::size_t step_count(const void *key_value) {
    constexpr const selection_step &step = rule_of(Mode).steps[Step];
    std::size_t total = 0;
    for (std::size_t source = 0; source < step.source_count; ++source) {
      total += source_count<Request, Key>(step.sources[source], key_value);
    }
    return total < 2 ? total : 2;
  }

  template <typename Request, typename Key, std::size_t Step>
  std::size_t inherited_count_at(const void *key_value) {
    if constexpr (Step == singular.step_count) {
      return 0;
    } else if constexpr (singular.steps[Step].when != step_condition::always) {
      return inherited_count_at<Request, Key, Step + 1>(key_value);
    } else {
      const std::size_t count =
          step_count<Request, Key, selection_mode::singular, Step>(key_value);
      return count != 0 ? count
                        : inherited_count_at<Request, Key, Step + 1>(key_value);
    }
  }

  // Whether a step of the singular rule names only bindings of the container
  // itself, from its local and container sources.
  static constexpr bool is_own_step(const selection_step &step) {
    for (std::size_t source = 0; source < step.source_count; ++source) {
      if (step.sources[source] == selection_source::parent ||
          step.sources[source] == selection_source::auto_construct ||
          step.sources[source] == selection_source::factory ||
          step.sources[source] == selection_source::elements) {
        return false;
      }
    }
    return true;
  }

  template <typename Request, typename Key, std::size_t Step>
  std::size_t own_count_at(const void *key_value) {
    if constexpr (Step == singular.step_count) {
      return 0;
    } else if constexpr (!is_own_step(singular.steps[Step])) {
      return own_count_at<Request, Key, Step + 1>(key_value);
    } else {
      const std::size_t count =
          step_count<Request, Key, selection_mode::singular, Step>(key_value);
      return count != 0 ? count
                        : own_count_at<Request, Key, Step + 1>(key_value);
    }
  }

  template <typename Request, typename Key, std::size_t Step>
  Request resolve_own_at(const void *key_value) {
    if constexpr (Step == singular.step_count) {
      raise_not_found<Request, Key>();
    } else if constexpr (!is_own_step(singular.steps[Step])) {
      return resolve_own_at<Request, Key, Step + 1>(key_value);
    } else {
      const std::size_t count =
          step_count<Request, Key, selection_mode::singular, Step>(key_value);
      if (count == 0) {
        return resolve_own_at<Request, Key, Step + 1>(key_value);
      }
      if (count > 1) {
        raise_ambiguous<Request>();
      }
      return resolve_step<Request, Key, selection_mode::singular, Step>(
          key_value);
    }
  }

  // The number of sources of step `Step` that may name a binding when the
  // request is compiled. With one, the step is answered by that source alone;
  // a source that names several makes the step ambiguous, so it counts.
  template <typename Request, typename Key, selection_mode Mode,
            std::size_t Step, std::size_t... Sources>
  static constexpr std::size_t
  possible_sources(std::index_sequence<Sources...> /*sources*/) {
    constexpr std::size_t counts[] = {
        known_count<Request, Key,
                    rule_of(Mode).steps[Step].sources[Sources]>()...,
        0};
    std::size_t possible = 0;
    for (std::size_t index = 0; index < sizeof...(Sources); ++index) {
      possible += counts[index] != 0;
    }
    return possible;
  }

  template <typename Request, typename Key, selection_mode Mode,
            std::size_t Step>
  static constexpr std::size_t possible_sources() {
    return possible_sources<Request, Key, Mode, Step>(
        std::make_index_sequence<rule_of(Mode).steps[Step].source_count>());
  }

  // The source of step `Step` that can name its one binding, when only one
  // can.
  template <typename Request, typename Key, selection_mode Mode,
            std::size_t Step>
  static constexpr selection_source sole_source() {
    return sole_source_of<Request, Key, Mode, Step>(
        std::make_index_sequence<rule_of(Mode).steps[Step].source_count>());
  }

  template <typename Request, typename Key, selection_mode Mode,
            std::size_t Step, std::size_t... Sources>
  static constexpr selection_source
  sole_source_of(std::index_sequence<Sources...> /*sources*/) {
    constexpr const selection_step &step = rule_of(Mode).steps[Step];
    constexpr std::size_t counts[] = {
        known_count<Request, Key, step.sources[Sources]>()..., 0};
    for (std::size_t index = 0; index < sizeof...(Sources); ++index) {
      if (counts[index] != 0) {
        return step.sources[index];
      }
    }
    return step.sources[0];
  }

  static constexpr bool is_runtime_source(selection_source source) {
    return source == selection_source::container_runtime ||
           source == selection_source::local_runtime;
  }

  // Answers step `Step`, whose one possible source is a runtime backend, in
  // one visit of the backend; a step that names nothing goes on to the next.
  template <typename Request, typename Key, bool Outermost, selection_mode Mode,
            std::size_t Step, typename Rest>
  DINGO_ALWAYS_INLINE Request select_sole(const void *key_value, Rest &rest) {
    auto otherwise = [&]() -> Request {
      return select<Request, Key, Outermost, Mode, Step + 1>(key_value, rest);
    };
    if constexpr (sole_source<Request, Key, Mode, Step>() ==
                  selection_source::container_runtime) {
      return self().runtime_source().template select_named<Request, Key>(
          self().container_resolver(), key_value, otherwise);
    } else {
      auto *source = self().local_runtime_source();
      if (source == nullptr) {
        return otherwise();
      }
      return source->template select_named<Request, Key>(self(), key_value,
                                                         otherwise);
    }
  }

  // Resolves the request from the source of step `Step` that names its one
  // binding. A source that names none or several bindings when the request is
  // compiled cannot name the one, so it is not asked and its code is not
  // instantiated.
  template <typename Request, typename Key, selection_mode Mode,
            std::size_t Step, std::size_t Source = 0>
  DINGO_ALWAYS_INLINE Request resolve_step(const void *key_value) {
    constexpr const selection_step &step = rule_of(Mode).steps[Step];
    if constexpr (Source == step.source_count) {
      raise_not_found<Request, Key>();
    } else {
      constexpr selection_source kind = step.sources[Source];
      constexpr std::size_t known = known_count<Request, Key, kind>();
      if constexpr (known == 0 || (known != unknown_count && known > 1)) {
        return resolve_step<Request, Key, Mode, Step, Source + 1>(key_value);
      } else if constexpr (known == 1 ||
                           possible_sources<Request, Key, Mode, Step>() == 1) {
        // The one binding of the step is in the one source that can hold
        // it, so it is not counted again.
        return resolve_source<Request, Key, kind>(key_value);
      } else {
        if (source_count<Request, Key>(kind, key_value) != 0) {
          return resolve_source<Request, Key, kind>(key_value);
        }
        return resolve_step<Request, Key, Mode, Step, Source + 1>(key_value);
      }
    }
  }

  template <typename Request, typename Key, selection_source Source>
  DINGO_ALWAYS_INLINE Request resolve_source(const void *key_value) {
    if constexpr (Source == selection_source::container_static) {
      return self().static_source().template resolve_named<Request, Key>(
          self().container_resolver(), key_value);
    } else if constexpr (Source == selection_source::container_runtime) {
      return self().runtime_source().template resolve_named<Request, Key>(
          self().container_resolver(), key_value);
    } else if constexpr (Source == selection_source::local_static) {
      return self().local_static_source().template resolve_named<Request, Key>(
          self(), key_value);
    } else if constexpr (Source == selection_source::local_runtime) {
      return self()
          .local_runtime_source()
          ->template resolve_named<Request, Key>(self(), key_value);
    } else if constexpr (Source == selection_source::parent) {
      // A request of the parent is part of the transaction of the container,
      // so a construction of the parent that registers into the container is
      // undone with it.
      return self().transactions().run([&]() -> Request {
        return self().parent()->template resolve_inherited<Request, Key>(
            key_value);
      });
    } else if constexpr (Source == selection_source::factory) {
      return make_by_factory<Request, Key>();
    } else if constexpr (Source == selection_source::elements) {
      return collect_elements<Request, Key>(key_value);
    } else {
      return auto_construct<Request, Key>();
    }
  }

  // The collection of the elements the container's own bindings name.
  template <typename Request, typename Key>
  Request collect_elements([[maybe_unused]] const void *key_value) {
    if constexpr (is_collection_request<Request>()) {
      using collection = typename request_traits<Request>::value_type;
      return self().transactions().run([&]() -> Request {
        collection result;
        collection_append append;
        counting_append<collection_append> counted{append};
        collect_own<collection, Key>(result, key_value, counted);
        return result;
      });
    } else {
      raise_not_found<Request, Key>();
    }
  }

  // Whether the static bindings of a container without runtime bindings name
  // an element of the collection a request asks for.
  template <typename Request, typename Key>
  static constexpr bool elements_known() {
    using element = typename collection_traits<
        typename request_traits<Request>::value_type>::resolve_type;
    std::size_t count = 0;
    if constexpr (Self::has_static_source &&
                  Key::descriptor.kind != key_kind::runtime) {
      count += Self::static_source_type::template fixed_count<element, Key>();
    }
    if constexpr (Self::has_local_static_source &&
                  Key::descriptor.kind != key_kind::runtime) {
      count +=
          Self::local_static_source_type::template fixed_count<element, Key>();
    }
    return count != 0;
  }

  template <typename Request, typename Key> Request make_by_factory() {
    if constexpr (has_detected_factory<Request>::value &&
                  std::is_same_v<Key, no_key>) {
      return detected_factory().make<Request>(self());
    } else {
      raise_not_found<Request, Key>();
    }
  }

  template <typename Request, typename Key> Request auto_construct() {
    if constexpr (auto_constructs_by<Request, Self>() &&
                  std::is_same_v<Key, no_key>) {
      using value_type = std::remove_cv_t<std::remove_reference_t<Request>>;
      if constexpr (std::is_reference_v<Request>) {
        // The construction is a transaction of the container.
        return self().transactions().run([this]() -> value_type & {
          self().retained().enter(self().transactions());
          return self().retained().template emplace<value_type>(
              [this] { return detected_factory().make<value_type>(self()); });
        });
      } else {
        return self().transactions().run([this]() -> value_type {
          return detected_factory().make<value_type>(self());
        });
      }
    } else {
      raise_not_found<Request, Key>();
    }
  }

  // Answers a request by the rule of `Mode` from step `Step`; only the
  // outermost container takes a conditional step, auto-construction or the
  // factory, and collects a collection no binding stores. A request a child
  // asks of its parent answers by `rest`, the steps of the child after its
  // parent, when no step names a binding.
  template <typename Request, typename Key, bool Outermost,
            selection_mode Mode = selection_mode::singular,
            std::size_t Step = 0, typename Rest = nothing_further>
  DINGO_ALWAYS_INLINE Request select(const void *key_value,
                                     Rest &&rest = Rest()) {
    if constexpr (Step == rule_of(Mode).step_count) {
      if constexpr (std::is_same_v<std::decay_t<Rest>, nothing_further>) {
        return nothing_selected<Request, Key, Outermost, Mode>(key_value);
      } else {
        return rest();
      }
    } else if constexpr (!meets<Request, Key, Outermost>(
                             rule_of(Mode).steps[Step].when)) {
      return select<Request, Key, Outermost, Mode, Step + 1>(key_value, rest);
    } else {
      constexpr std::size_t known =
          known_step_count<Request, Key, Mode, Step>();
      if constexpr (known == 0) {
        return select<Request, Key, Outermost, Mode, Step + 1>(key_value, rest);
      } else if constexpr (known == 1) {
        return resolve_step<Request, Key, Mode, Step>(key_value);
      } else if constexpr (known != unknown_count ||
                           known_step_least<Request, Key, Mode, Step>(
                               std::make_index_sequence<
                                   rule_of(Mode).steps[Step].source_count>()) >
                               1) {
        // Two bindings known when the request is compiled make it ambiguous
        // whatever is registered later; it is decided when the steps before
        // are known to name none, and the request is the outermost.
        static_assert(
            require<!(Outermost &&
                      closed_before<Request, Key, Outermost, Mode, Step>()),
                    diag::type_ambiguous, Request>());
        raise_ambiguous<Request>();
      } else if constexpr (possible_sources<Request, Key, Mode, Step>() == 1 &&
                           sole_source<Request, Key, Mode, Step>() ==
                               selection_source::parent) {
        return select_parent<Request, Key, Outermost, Mode, Step>(key_value,
                                                                  rest);
      } else if constexpr (possible_sources<Request, Key, Mode, Step>() == 1 &&
                           is_runtime_source(
                               sole_source<Request, Key, Mode, Step>())) {
        return select_sole<Request, Key, Outermost, Mode, Step>(key_value,
                                                                rest);
      } else {
        const std::size_t count =
            step_count<Request, Key, Mode, Step>(key_value);
        if (count == 0) {
          return select<Request, Key, Outermost, Mode, Step + 1>(key_value,
                                                                 rest);
        }
        if (count > 1) {
          raise_ambiguous<Request>();
        }
        return resolve_step<Request, Key, Mode, Step>(key_value);
      }
    }
  }

  // Answers step `Step`, whose one possible source is the parent, by the
  // parent, which goes on to the steps of the container after it when no
  // ancestor names a binding, so that each ancestor is searched once. A
  // request of the parent is part of the transaction of the container.
  template <typename Request, typename Key, bool Outermost, selection_mode Mode,
            std::size_t Step, typename Rest>
  Request select_parent(const void *key_value, Rest &rest) {
    auto otherwise = [&]() -> Request {
      return select<Request, Key, Outermost, Mode, Step + 1>(key_value, rest);
    };
    if (self().parent() == nullptr) {
      return otherwise();
    }
    // The rest is passed by its type erased, as a parent may be of the type
    // of its child.
    selection_rest<Request> erased(otherwise);
    return self().transactions().run([&]() -> Request {
      return self().parent()->template select_inherited<Request, Key>(key_value,
                                                                      erased);
    });
  }

  template <typename Request, typename Key, bool Outermost, selection_mode Mode>
  Request nothing_selected(const void *key_value) {
    using value_type = typename request_traits<Request>::value_type;
    if constexpr (Outermost && Mode == selection_mode::singular &&
                  is_collection_request<Request>()) {
      // A collection no binding stores is the collection of the bindings of
      // its element type.
      return collect<value_type, Key>(key_value, collection_append());
    } else {
      // A request no source can answer when it is compiled is reported
      // then, and otherwise when it is resolved.
      if constexpr (Outermost && decided_when_compiled<Request, Key, Mode>()) {
        if constexpr (std::is_same_v<Key, no_key>) {
          fail<diag::type_not_found, Request>();
        } else {
          fail<diag::type_not_found_keyed, Request,
               typename detail::key_type_of<Key>::type>();
        }
      }
      raise_not_found<Request, Key>();
    }
  }

  template <typename Collection, typename Key, typename Append>
  void collect_source(selection_source source, Collection &collection,
                      const void *key_value, Append &append) {
    if constexpr (Self::has_static_source) {
      if (source == selection_source::container_static) {
        self().static_source().template collect<Collection, Key>(
            self().container_resolver(), collection, key_value, append);
      }
    }
    if constexpr (Self::has_runtime_source) {
      if (source == selection_source::container_runtime) {
        self().runtime_source().template collect<Collection, Key>(
            self().container_resolver(), collection, key_value, append);
      }
    }
    if constexpr (Self::has_local_static_source) {
      if (source == selection_source::local_static) {
        self().local_static_source().template collect<Collection, Key>(
            self(), collection, key_value, append);
      }
    }
    if constexpr (Self::has_local_runtime_source) {
      if (source == selection_source::local_runtime &&
          self().local_runtime_source() != nullptr) {
        self().local_runtime_source()->template collect<Collection, Key>(
            self(), collection, key_value, append);
      }
    }
    if constexpr (!std::is_void_v<typename Self::parent_type>) {
      if (source == selection_source::parent && self().parent() != nullptr) {
        self().parent()->template collect_into<Collection, Key>(
            collection, key_value, append);
      }
    }
  }
};

} // namespace dingo::core
