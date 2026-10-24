//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The runtime backend: a binding is registered at runtime with the terms it
// serves, derived at compile time, and a thunk per term that walks it. A
// request finds its binding and selects its term with the same engine
// functions as the static backend.

#include <dingo/backend/diagnose.h>
#include <dingo/backend/local.h>
#include <dingo/backend/memo.h>
#include <dingo/backend/slot.h>
#include <dingo/conversion/collection.h>
#include <dingo/conversion/convert.h>
#include <dingo/conversion/key_kinds.gen.h>
#include <dingo/conversion/lookup.h>
#include <dingo/conversion/request.h>
#include <dingo/conversion/route.h>
#include <dingo/primitives/allocation.h>
#include <dingo/primitives/diagnostics.gen.h>
#include <dingo/primitives/exceptions.h>
#include <dingo/primitives/retained.h>
#include <dingo/primitives/shape.h>
#include <dingo/primitives/slot.h>
#include <dingo/primitives/type_name.h>
#include <dingo/primitives/type_path.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo::core {

template <typename Resolver> class runtime_entry;

// Walks a term to its boundary and returns the boundary object.
template <typename Boundary, typename Resolver> struct boundary_maker {
  Boundary (*make)(runtime_entry<Resolver> &, Resolver &);
};

// A binding of a runtime container, whose dependencies `Resolver`, the
// container, resolves.
template <typename Resolver> class runtime_entry {
public:
  // Walks a term to its boundary and returns the address of the borrowed
  // object, or null for the element of a wrapper that holds none when the
  // request takes its address, `null_allowed`.
  using thunk = void *(*)(runtime_entry &, Resolver &, bool);

  using name_function = void (*)(std::string &);

  runtime_entry(const binding_descriptor &descriptor, const terms &published,
                std::uint8_t exposed, const completion_plan *plans,
                const void *const *boundaries, const thunk *thunks,
                const void *const *makers, name_function stored_name)
      : stored_name_(stored_name), descriptor_(descriptor),
        published_(published), exposed_(exposed), plans_(plans),
        boundaries_(boundaries), thunks_(thunks), makers_(makers) {}

  virtual ~runtime_entry() = default;

  // The value of the key of the binding, or nullptr for a key without one.
  virtual const void *key_value() const = 0;

  // The value of the key of the binding for `interface` in the domain of the
  // key type `key_type`, or nullptr when it has none there; a registration
  // may name a value of its own for an interface, and keys of other types.
  virtual const void *key_value_for(type_id interface,
                                    type_id key_type) const = 0;

  const void *key_value_for(type_id interface) const {
    return key_value_for(interface, descriptor_.key.type);
  }

  // Whether the objects the binding borrows keep their addresses: its scope
  // is stable and its slot settled.
  virtual bool settled(Resolver &resolver) const = 0;

  const binding_descriptor &descriptor() const { return descriptor_; }
  const terms &published() const { return published_; }
  std::uint8_t exposed() const { return exposed_; }
  const completion_plan &plan(std::size_t term) const { return plans_[term]; }

  // Appends the name of the stored type, for diagnostics.
  void append_stored_name(std::string &text) const { stored_name_(text); }

  void *borrow(std::size_t term, Resolver &resolver,
               bool null_allowed = false) {
    return thunks_[term](*this, resolver, null_allowed);
  }

  // Whether the binding constructs a boundary object of type `Boundary` for
  // a term.
  template <typename Boundary> bool makes(std::size_t term) const {
    return boundaries_[term] == type_address_v<Boundary>;
  }

  // The boundary object of a term, whose type `makes` confirmed.
  template <typename Boundary>
  Boundary make(std::size_t term, Resolver &resolver) {
    return static_cast<const boundary_maker<Boundary, Resolver> *>(
               makers_[term])
        ->make(*this, resolver);
  }

private:
  name_function stored_name_;
  const binding_descriptor &descriptor_;
  const terms &published_;
  std::uint8_t exposed_;
  const completion_plan *plans_;
  const void *const *boundaries_;
  const thunk *thunks_;
  const void *const *makers_;
};

// Marks the constructor arguments of a binding that begin with the value of
// its runtime key.
struct with_key_value {};
struct without_key_value {};

template <typename Binding, typename Resolver>
class runtime_binding final : public runtime_entry<Resolver> {
  using entry = runtime_entry<Resolver>;
  using thunk = typename entry::thunk;
  using terms_sequence = std::make_index_sequence<Binding::published.size>;
  using key_store_type = key_store<typename Binding::key_type>;

public:
  template <typename... Args>
  explicit runtime_binding(without_key_value /*key*/, Args &&...args)
      : entry(Binding::descriptor, Binding::published, Binding::exposed,
              plans(terms_sequence()), boundaries(terms_sequence()),
              thunks(terms_sequence()), makers(terms_sequence()),
              &append_type_name<typename Binding::stored_type>),
        slot_(std::forward<Args>(args)...) {}

  template <typename Key, typename... Args>
  runtime_binding(with_key_value /*key*/, const Key &key, Args &&...args)
      : entry(Binding::descriptor, Binding::published, Binding::exposed,
              plans(terms_sequence()), boundaries(terms_sequence()),
              thunks(terms_sequence()), makers(terms_sequence()),
              &append_type_name<typename Binding::stored_type>),
        key_(key), slot_(std::forward<Args>(args)...) {}

  const void *key_value() const override { return key_.value(); }

  const void *key_value_for(type_id interface,
                            type_id key_type) const override {
    if (key_type != Binding::key_type::descriptor.type) {
      return alias_value_for(interface, key_type,
                             std::make_index_sequence<aliases_type::size>());
    }
    if constexpr (has_interface_keys) {
      const std::size_t index = interface_index(interface);
      if (index != no_index && interface_keys_[index]) {
        return &*interface_keys_[index];
      }
    }
    return key_.value();
  }

  // Names the value of the key of the alias key type `Alias` for the
  // declared interface `Interface`, or for every interface when it is void.
  template <typename Alias, typename Interface, typename Value>
  void set_alias_key(const Value &value) {
    constexpr std::size_t alias = type_list_index_of<Alias, aliases_type>();
    auto &keys = std::get<alias>(aliases_);
    if constexpr (std::is_void_v<Interface>) {
      keys.shared = value;
    } else {
      keys.interfaces[type_list_index_of<Interface,
                                         typename Binding::interfaces>()] =
          value;
    }
  }

  // Names the key value of the binding for the declared interface
  // `Interface`.
  template <typename Interface, typename Value>
  void set_interface_key(const Value &value) {
    static_assert(has_interface_keys,
                  "a key value names an interface of a binding by a runtime "
                  "key");
    constexpr std::size_t index =
        type_list_index_of<Interface, typename Binding::interfaces>();
    static_assert(require<index != Binding::descriptor.interface_count,
                          diag::key_interface_undeclared, Interface,
                          typename Binding::stored_type>());
    interface_keys_[index] = value;
  }

  // The local bindings of the registration.
  local_bindings<Resolver, typename Binding::locals> &locals() {
    return locals_;
  }

  bool settled(Resolver &resolver) const override {
    if constexpr (Binding::scope.stable) {
      return slot_.settled(resolver);
    } else {
      return false;
    }
  }

private:
  // Whether a request may borrow the object of a term: one the term refers
  // to, copies, or takes the address of, or an array, which a request takes
  // the address of and never by value.
  template <std::size_t Term>
  static constexpr bool borrows =
      plan_v<Binding, Term>.kind != completion::construct ||
      has_properties(semantics_of(Binding::published.items[Term].value),
                     property::immovable);

  template <std::size_t Term>
  static void *borrow_term(entry &binding, Resolver &resolver,
                           bool null_allowed) {
    if constexpr (!walk_supported_v<Binding, Term> || !borrows<Term>) {
      throw std::logic_error(
          "the next implementation does not provide this route yet");
    } else {
      auto &self = static_cast<runtime_binding &>(binding);
      auto view = self.view(resolver);
      const auto address =
          address_from<Binding, Term, plan_v<Binding, Term>.binding_steps>(
              self.slot_.context(),
              [&]() -> decltype(auto) { return self.slot_.get(view); },
              null_allowed);
      // The address of a const or volatile object is erased too; the request
      // restores the qualifiers of the type it names.
      return const_cast<void *>(static_cast<const volatile void *>(address));
    }
  }

  template <std::size_t Term>
  static boundary_t<Binding, Term> make_term(entry &binding,
                                             Resolver &resolver) {
    auto &self = static_cast<runtime_binding &>(binding);
    auto view = self.view(resolver);
    return walk_to_boundary<Binding, Term>(
        self.slot_.context(),
        [&]() -> decltype(auto) { return self.slot_.get(view); });
  }

  // Whether the binding constructs a boundary object for a term; an array is
  // never a boundary object, as no request takes one by value.
  template <std::size_t Term>
  static constexpr bool constructs =
      walk_supported_v<Binding, Term> && !borrows<Term> &&
      !std::is_array_v<boundary_t<Binding, Term>>;

  template <std::size_t Term> static constexpr const void *boundary_of() {
    if constexpr (constructs<Term>) {
      return type_address_v<boundary_t<Binding, Term>>;
    } else {
      return nullptr;
    }
  }

  template <std::size_t Term>
  static constexpr boundary_maker<boundary_t<Binding, Term>, Resolver> maker{
      &make_term<Term>};

  template <std::size_t Term> static constexpr const void *maker_of() {
    if constexpr (constructs<Term>) {
      return &maker<Term>;
    } else {
      return nullptr;
    }
  }

  template <std::size_t... Terms>
  static const completion_plan *plans(std::index_sequence<Terms...>) {
    static constexpr completion_plan table[] = {plan_v<Binding, Terms>...,
                                                completion_plan{}};
    return table;
  }

  template <std::size_t... Terms>
  static const void *const *boundaries(std::index_sequence<Terms...>) {
    static constexpr const void *table[] = {boundary_of<Terms>()..., nullptr};
    return table;
  }

  template <std::size_t... Terms>
  static const thunk *thunks(std::index_sequence<Terms...>) {
    static constexpr thunk table[] = {&borrow_term<Terms>..., nullptr};
    return table;
  }

  template <std::size_t... Terms>
  static const void *const *makers(std::index_sequence<Terms...>) {
    static constexpr const void *table[] = {maker_of<Terms>()..., nullptr};
    return table;
  }

  // The resolver of the object of the binding: a view of its local bindings
  // over the container.
  local_view<Resolver, typename Binding::locals> view(Resolver &resolver) {
    return local_view<Resolver, typename Binding::locals>(
        resolver, &locals_.statics(), locals_.runtime());
  }

  // The key values of the declared interfaces a registration names its own
  // for.
  template <typename Key, typename = void> struct interface_keys_of {
    struct type {};
  };

  template <typename Key>
  struct interface_keys_of<
      Key, std::enable_if_t<Key::descriptor.kind == key_kind::runtime>> {
    using type = std::optional<typename Key::value_type>[max_interfaces];
  };

  static constexpr bool has_interface_keys =
      Binding::key_type::descriptor.kind == key_kind::runtime;

  static constexpr std::size_t no_index = static_cast<std::size_t>(-1);

  static std::size_t interface_index(type_id interface) {
    for (std::size_t index = 0; index < Binding::descriptor.interface_count;
         ++index) {
      if (Binding::descriptor.interfaces[index].nodes[0].type == interface) {
        return index;
      }
    }
    return no_index;
  }

  // The values of the keys of an alias key type, for every interface and for
  // each.
  template <typename Alias> struct alias_keys {
    std::optional<typename Alias::value_type> shared;
    std::optional<typename Alias::value_type> interfaces[max_interfaces];
  };

  using aliases_type = typename Binding::key_aliases;

  template <typename List> struct alias_storage;

  template <typename... Aliases> struct alias_storage<type_list<Aliases...>> {
    using type = std::tuple<alias_keys<Aliases>...>;
  };

  template <std::size_t... Indices>
  const void *
  alias_value_for([[maybe_unused]] type_id interface,
                  [[maybe_unused]] type_id key_type,
                  std::index_sequence<Indices...> /*indices*/) const {
    const void *found = nullptr;
    (
        [&] {
          using alias = type_list_at_t<Indices, aliases_type>;
          if (found != nullptr || alias::descriptor.type != key_type) {
            return;
          }
          const auto &keys = std::get<Indices>(aliases_);
          const std::size_t index = interface_index(interface);
          if (index != no_index && keys.interfaces[index]) {
            found = &*keys.interfaces[index];
          } else if (keys.shared) {
            found = &*keys.shared;
          }
        }(),
        ...);
    return found;
  }

  key_store_type key_;
  typename interface_keys_of<typename Binding::key_type>::type
      interface_keys_{};
  typename alias_storage<aliases_type>::type aliases_;
  local_bindings<Resolver, typename Binding::locals> locals_;
  slot_of_t<Binding> slot_;
};

namespace detail {
// A prvalue of `Type`, in an unevaluated operand: a cast of it needs no move
// constructor, as a cast of the completions' prvalues does not.
template <typename Type> Type prvalue_of();

template <typename From, typename To, typename = void>
struct casts_to : std::false_type {};

template <typename From, typename To>
struct casts_to<From, To,
                std::void_t<decltype(static_cast<To>(prvalue_of<From>()))>>
    : std::true_type {};

// Whether a static_cast converts a From value to To: no route of a request
// whose type it does not convert to completes it, so its completion is not
// instantiated.
template <typename From, typename To>
inline constexpr bool casts_to_v = casts_to<From, To>::value;

template <typename... Args> struct find_primary_key {
  using type = void;
};

template <typename First, typename... Rest>
struct find_primary_key<First, Rest...> {
  using type =
      typename std::conditional_t<is_key_value<std::decay_t<First>>::value,
                                  runtime_key_of<std::decay_t<First>>,
                                  find_primary_key<Rest...>>::type;
};

// The positions of the arguments of a registration: the key values and the
// arguments of the slot.
template <typename... Args> struct registration_arguments {
  // The arrays name their bounds: Clang 18 reads no element of an array of a
  // class template whose bound is deduced in a constant expression.
  static constexpr bool keys[sizeof...(Args) + 1] = {
      is_key_value<std::decay_t<Args>>::value..., false};

  // The runtime key of the first key value, the primary key of the binding.
  using primary_key = typename find_primary_key<Args...>::type;

  // Whether each argument is a key value of the primary key type.
  static constexpr bool primary_keys[sizeof...(Args) + 1] = {
      (is_key_value<std::decay_t<Args>>::value &&
       std::is_same_v<typename runtime_key_of<std::decay_t<Args>>::type,
                      primary_key>)...,
      false};

  static constexpr std::size_t key_count() {
    std::size_t count = 0;
    for (std::size_t index = 0; index < sizeof...(Args); ++index) {
      count += keys[index] ? 1 : 0;
    }
    return count;
  }

  // Whether each argument is a key value of an interface.
  static constexpr bool interface_keys[sizeof...(Args) + 1] = {
      !std::is_void_v<
          typename key_value_interface<std::decay_t<Args>>::type>...,
      false};

  // The primary key value of every interface without one of its own: the
  // primary key value that names none, or the first key value.
  static constexpr std::size_t key_index() {
    for (std::size_t index = 0; index < sizeof...(Args); ++index) {
      if (primary_keys[index] && !interface_keys[index]) {
        return index;
      }
    }
    std::size_t index = 0;
    while (index < sizeof...(Args) && !keys[index]) {
      ++index;
    }
    return index;
  }

  static constexpr std::size_t shared_key_count() {
    std::size_t count = 0;
    for (std::size_t index = 0; index < sizeof...(Args); ++index) {
      count += primary_keys[index] && !interface_keys[index] ? 1 : 0;
    }
    return count;
  }

  // The position of the slot argument `ordinal`.
  static constexpr std::size_t slot_index(std::size_t ordinal) {
    std::size_t index = 0;
    for (;; ++index) {
      if (!keys[index]) {
        if (ordinal == 0) {
          return index;
        }
        --ordinal;
      }
    }
  }
};
} // namespace detail

// The runtime bindings of a container, a source of its selection: it counts
// the bindings a request names, resolves the one it names, and collects them,
// with `Resolver`, the container, resolving their dependencies.
namespace detail {
// The lookup declarations of the traits of a resolver, a list of them.
template <typename Definition> struct declarations_in {
  using type = type_list<>;
};

template <template <typename...> typename List, typename... Declarations>
struct declarations_in<List<Declarations...>> {
  using type = type_list<Declarations...>;
};

template <typename Resolver, typename = void> struct resolver_declarations {
  using type = type_list<>;
};

template <typename Resolver>
struct resolver_declarations<
    Resolver, std::void_t<typename Resolver::lookup_definition_type>> {
  using type =
      typename declarations_in<typename Resolver::lookup_definition_type>::type;
};
} // namespace detail

template <typename Resolver, typename Allocator> class runtime_backend {
  using entry = runtime_entry<Resolver>;
  using bytes_type = byte_allocator_t<Allocator>;
  using entry_pointer =
      std::unique_ptr<entry, allocated_deleter<entry, bytes_type>>;

public:
  // A backend of the lookup declarations of `table`, which outlives it, whose
  // bindings are allocated from `allocator`.
  explicit runtime_backend(const lookup_table &table,
                           const Allocator &allocator = Allocator())
      : table_(&table), bytes_(allocator) {}

  // Adds a binding, whose arguments are the arguments of its slot and the
  // value of its runtime key. A binding whose key value is outside the bound
  // of the dense array of its domain is rejected, and so is a binding whose
  // key a binding of a common interface already holds, unless its domain
  // holds many; `held` tells whether another source of the container holds
  // its key.
  // Returns the local bindings of the added binding.
  template <typename Binding, typename... Args> auto &add(Args &&...args) {
    return add_unless<Binding>([](const void * /*key_value*/) { return false; },
                               std::forward<Args>(args)...);
  }

  template <typename Binding, typename Held, typename... Args>
  auto &add_unless(Held &&held, Args &&...args) {
    static_assert(registers<Binding>());
    using arguments = detail::registration_arguments<Args...>;
    static_assert(require<arguments::shared_key_count() <= 1,
                          diag::registration_key_values,
                          typename Binding::stored_type>());
    auto all = std::forward_as_tuple(std::forward<Args>(args)...);
    entry_pointer added = make_entry<Binding, arguments,
                                     std::tuple<std::decay_t<Args>...>>(
        all,
        std::make_index_sequence<sizeof...(Args) - arguments::key_count()>());
    if constexpr (Binding::key_type::descriptor.kind == key_kind::fixed ||
                  Binding::key_type::descriptor.kind == key_kind::runtime) {
      // A key value outside the bound of a dense array is never stored.
      const std::size_t bound = key_bound(*table_, Binding::descriptor);
      if (bound != unbounded) {
        for_each_interface<Binding>([&](type_id interface) {
          if (!key_store<typename Binding::key_type>::within(
                  added->key_value_for(interface), bound)) {
            raise_out_of_range<Binding>();
          }
        });
      }
    }
    if (held(added->key_value())) {
      raise_already_registered<Binding>();
    }
    for (const auto &existing : entries_) {
      if (conflicts_by_value<Binding>(*existing, *added)) {
        raise_already_registered<Binding>();
      }
    }
    auto &locals =
        static_cast<runtime_binding<Binding, Resolver> &>(*added).locals();
    entries_.push_back(std::move(added));
    ++generation_;
    // A registration during a request is undone when the request fails.
    transactions_.record(this, &undo_add);
    return locals;
  }

  // The transaction of the requests of the container in progress.
  transaction_log &transactions() { return transactions_; }

  // The number of bindings a request by `Key` names, 0, 1, or 2 for many.
  template <typename Request, typename Key>
  std::size_t count(const void *key_value) const {
    std::size_t found = 0;
    memo<Request, Key>().visit(
        key_value, [&](const candidate_type &) { return ++found != 2; });
    return found;
  }

  // Resolves a request from the one binding it names.
  template <typename Request, typename Key>
  DINGO_ALWAYS_INLINE Request resolve_named(Resolver &resolver,
                                            const void *key_value) {
    const candidate_type *named = nullptr;
    memo<Request, Key>().visit(key_value, [&](const candidate_type &found) {
      named = &found;
      return false;
    });
    if (named == nullptr) {
      raise_not_found<Request, Key>();
    }
    return resolve_from<Request>(resolver, *named);
  }

  // Resolves a request from the one binding it names, in one visit of its
  // memo: it raises the ambiguity of two, and returns `otherwise()` when it
  // names none.
  template <typename Request, typename Key, typename Otherwise>
  DINGO_ALWAYS_INLINE Request select_named(Resolver &resolver,
                                           const void *key_value,
                                           Otherwise &&otherwise) {
    const candidate_type *named = nullptr;
    bool many = false;
    memo<Request, Key>().visit(key_value, [&](const candidate_type &found) {
      many = named != nullptr;
      named = &found;
      return !many;
    });
    if (many) {
      raise_ambiguous<Request>();
    }
    if (named == nullptr) {
      return otherwise_of<Request>(otherwise);
    }
    return resolve_from<Request>(resolver, *named);
  }

  // The rest of a selection whose step names nothing, out of line of the
  // memoized resolution.
  template <typename Request, typename Otherwise>
  DINGO_NOINLINE static Request otherwise_of(Otherwise &otherwise) {
    return otherwise();
  }

  // Adds every binding a request of the element type of `Collection` by
  // `Key` names to `collection`, in the order of registration.
  template <typename Collection, typename Key, typename Append>
  void collect(Resolver &resolver, Collection &collection,
               const void *key_value, Append &append) {
    using element = typename collection_traits<Collection>::resolve_type;
    std::size_t count = 0;
    memo<element, Key>().visit(key_value, [&](const candidate_type &) {
      ++count;
      return true;
    });
    if (count == 0) {
      return;
    }
    append.reserve(collection, count);
    memo<element, Key>().visit(key_value, [&](const candidate_type &found) {
      append(collection, resolve_from<element>(resolver, found));
      return true;
    });
  }

private:
  using candidate_type = candidate<entry>;

  template <typename Binding, typename Visit, std::size_t... Indices>
  static void visit_leaves(Visit &visit,
                           std::index_sequence<Indices...> /*leaves*/) {
    (visit(Binding::descriptor.leaves.items[Indices]), ...);
  }

  // Calls `visit` with every interface a binding of type `Binding` serves:
  // its declared ones, or the leaves its stored shape names.
  template <typename Binding, typename Visit>
  static void for_each_interface(Visit &&visit) {
    constexpr const binding_descriptor &binding = Binding::descriptor;
    if constexpr (binding.interface_count == 0) {
      visit_leaves<Binding>(
          visit, std::make_index_sequence<Binding::descriptor.leaves.size>());
    } else {
      for (std::size_t index = 0; index < binding.interface_count; ++index) {
        visit(binding.interfaces[index].nodes[0].type);
      }
    }
  }

  // Whether `added`, a binding of type `Binding`, conflicts with `existing`:
  // they serve an interface whose domain holds one binding with equal key
  // values for it.
  template <typename Binding>
  bool conflicts_by_value(const entry &existing, const entry &added) const {
    if (!conflicts(*table_, existing.descriptor(), Binding::descriptor)) {
      return false;
    }
    return conflicts_in_domain<typename Binding::key_type, Binding>(existing,
                                                                    added) ||
           conflicts_in_aliases<Binding>(existing, added,
                                         typename Binding::key_aliases());
  }

  template <typename Binding, typename... Aliases>
  bool conflicts_in_aliases(const entry &existing, const entry &added,
                            type_list<Aliases...> /*aliases*/) const {
    return (conflicts_in_domain<Aliases, Binding>(existing, added) || ...);
  }

  // Whether the bindings hold equal values of the key `Key` for an interface
  // of a domain that holds one binding.
  template <typename Key, typename Binding>
  bool conflicts_in_domain(const entry &existing, const entry &added) const {
    if (!has_key_domain(existing.descriptor(), Key::descriptor)) {
      return false;
    }
    bool found = false;
    for_each_interface<Binding>([&](type_id interface) {
      if (found || !serves_interface(existing.descriptor(), interface) ||
          holds_many(*table_, interface, Key::descriptor)) {
        return;
      }
      const void *left =
          existing.key_value_for(interface, Key::descriptor.type);
      const void *right = added.key_value_for(interface, Key::descriptor.type);
      if (!key_has_value(Key::descriptor)) {
        found = true;
      } else if (left != nullptr && right != nullptr) {
        found = key_store<Key>::equal(left, right);
      }
    });
    return found;
  }

  static void undo_add(void *backend, void * /*context*/) noexcept {
    auto &self = *static_cast<runtime_backend *>(backend);
    self.entries_.pop_back();
    ++self.generation_;
  }

  // The memo of a request by `Key`, which indexes its candidates by the index
  // of the lookup declaration of its domain, decided when it is compiled.
  template <typename Request, typename Key> struct indexed_memo {
    using declarations = declaration_list<
        typename detail::resolver_declarations<Resolver>::type>;
    static constexpr request_descriptor request =
        request_traits<Request, Key>::descriptor;
    using declared = typename declarations::template at<declarations::position(
        innermost_leaf(request.value), request.key)>::indexing;
    using type = request_memo<
        entry, Key,
        std::conditional_t<std::is_void_v<declared>,
                           typename Resolver::default_indexing_type, declared>>;
  };

  template <typename Request, typename Key>
  using memo_of = typename indexed_memo<Request, Key>::type;

  // The memo of a request by `Key`, made by its first resolution and made
  // again by the first one after a registration. A memo that is current is
  // read inline, and making one stays out of line.
  template <typename Request, typename Key>
  DINGO_ALWAYS_INLINE const memo_of<Request, Key> &memo() const {
    const std::size_t id = request_id<Request, Key>();
    if (id < memos_.size()) {
      const memo_base *held = memos_[id].get();
      if (held != nullptr && held->generation == generation_) {
        return static_cast<const memo_of<Request, Key> &>(*held);
      }
    }
    return refresh<Request, Key>(id);
  }

  template <typename Request, typename Key>
  DINGO_NOINLINE const memo_of<Request, Key> &refresh(std::size_t id) const {
    using memo_type = memo_of<Request, Key>;
    if (id >= memos_.size()) {
      memos_.resize(id + 1);
    }
    std::unique_ptr<memo_base> &held = memos_[id];
    if (!held) {
      held = std::make_unique<memo_type>();
    }
    auto &found = static_cast<memo_type &>(*held);
    if (found.generation != generation_) {
      remember<Request, Key>(found);
    }
    return found;
  }

  template <typename Request, typename Key>
  DINGO_NOINLINE void remember(memo_of<Request, Key> &found) const {
    constexpr request_descriptor request =
        request_traits<Request, Key>::descriptor;
    found.clear();
    for (const auto &binding : entries_) {
      if (!finds(binding->descriptor(), request)) {
        continue;
      }
      // The value of the key of the domain of the request for its interface;
      // a binding with none there for the interface is not a candidate.
      const void *key_value = binding->key_value_for(
          innermost_leaf(request.value), request.key.type);
      if (key_has_value(request.key) && key_value == nullptr) {
        continue;
      }
      const route_choice choice =
          select_route(binding->descriptor(), binding->published(), request,
                       binding->exposed());
      found.add({binding.get(), choice.term, key_value,
                 choice.term == no_term
                     ? request_plan{}
                     : plan_request(binding->plan(choice.term), choice)});
    }
    found.index();
    found.generation = generation_;
  }

  // A binding allocated from the allocator of the backend.
  template <typename Binding, typename Arguments, typename Decayed,
            typename All, std::size_t... Slots>
  entry_pointer make_entry(All &all, std::index_sequence<Slots...> /*slots*/) {
    using arguments = Arguments;
    using binding = runtime_binding<Binding, Resolver>;
    if constexpr (arguments::key_count() == 0) {
      return allocate_object<binding, entry>(
          bytes_, without_key_value(),
          std::get<arguments::slot_index(Slots)>(std::move(all))...);
    } else {
      using value = std::tuple_element_t<arguments::key_index(), Decayed>;
      entry_pointer made = allocate_object<binding, entry>(
          bytes_, with_key_value(),
          runtime_key_of<value>::value(std::get<arguments::key_index()>(all)),
          std::get<arguments::slot_index(Slots)>(std::move(all))...);
      set_keys<arguments, Decayed>(
          static_cast<binding &>(*made), all,
          std::make_index_sequence<std::tuple_size_v<Decayed>>());
      return made;
    }
  }

  // Names the key values of the arguments besides the primary one of every
  // interface: those of an interface, and those of the other key types.
  template <typename Arguments, typename Decayed, typename Binding,
            typename All, std::size_t... Indices>
  static void set_keys(Binding &binding, All &all,
                       std::index_sequence<Indices...> /*indices*/) {
    (set_key<Arguments, Indices, std::tuple_element_t<Indices, Decayed>>(
         binding, std::get<Indices>(all)),
     ...);
  }

  template <typename Arguments, std::size_t Index, typename Value,
            typename Binding, typename Argument>
  static void set_key(Binding &binding, Argument &argument) {
    if constexpr (Arguments::keys[Index]) {
      using interface = typename key_value_interface<Value>::type;
      if constexpr (Arguments::primary_keys[Index]) {
        if constexpr (!std::is_void_v<interface>) {
          binding.template set_interface_key<interface>(argument.value());
        }
      } else {
        binding.template set_alias_key<typename runtime_key_of<Value>::type,
                                       interface>(argument.value());
      }
    }
  }

  // A reference to an object whose address is kept is returned inline, with
  // no call; any other completion stays out of line.
  template <typename Request>
  DINGO_ALWAYS_INLINE static Request resolve_from(Resolver &resolver,
                                                  const candidate_type &found) {
    if constexpr (std::is_lvalue_reference_v<Request>) {
      // Only the term of a lvalue reference request keeps its address.
      using value_type = typename request_traits<Request>::value_type;
      if (found.resolved != nullptr) {
        return static_cast<Request>(*static_cast<value_type *>(found.resolved));
      }
    }
    return resolve_completing<Request>(resolver, found);
  }

  template <typename Request>
  DINGO_NOINLINE static Request
  resolve_completing(Resolver &resolver, const candidate_type &found) {
    if (found.term == no_term) {
      raise_not_convertible<Request>(*found.binding);
    }
    return complete<Request>(resolver, found);
  }

  // The address the term of `found` borrows, kept once its binding is
  // settled.
  static void *borrowed(Resolver &resolver, const candidate_type &found,
                        bool null_allowed = false) {
    if (found.resolved != nullptr) {
      return found.resolved;
    }
    void *address = found.binding->borrow(found.term, resolver, null_allowed);
    if (found.binding->settled(resolver) &&
        !found.binding->plan(found.term).rechecked) {
      found.resolved = address;
    }
    return address;
  }

  template <typename Request>
  [[noreturn]] DINGO_CORE_COLD DINGO_NOINLINE static void
  raise_not_convertible(const entry &binding) {
    const std::string request = type_name_of<Request>();
    std::string stored;
    binding.append_stored_name(stored);
    if (type_frame::active()) {
      std::string type;
      type_frame::append_innermost(type);
      raise_type_not_convertible_required_by(request, stored, type);
    }
    raise_type_not_convertible(request, stored);
  }

  template <typename Request>
  static Request complete(Resolver &resolver, const candidate_type &found) {
    using value_type = typename request_traits<Request>::value_type;
    switch (found.plan.kind) {
    case completion::refer:
      if constexpr (std::is_lvalue_reference_v<Request>) {
        return static_cast<Request>(
            *static_cast<value_type *>(borrowed(resolver, found)));
      }
      break;
    case completion::copy:
      // The value of a reference may be incomplete, so it is not asked.
      if constexpr (std::conjunction_v<
                        std::negation<std::is_reference<Request>>,
                        is_copied<value_type>> &&
                    detail::casts_to_v<value_type, Request>) {
        return static_cast<Request>(value_type(
            *static_cast<const value_type *>(borrowed(resolver, found))));
      }
      break;
    case completion::address:
      if constexpr (std::is_pointer_v<value_type> &&
                    detail::casts_to_v<value_type, Request>) {
        return static_cast<Request>(
            static_cast<value_type>(borrowed(resolver, found, true)));
      }
      break;
    case completion::construct:
      if constexpr (!std::is_lvalue_reference_v<Request>) {
        return construct<Request>(resolver, *found.binding, found.term,
                                  found.plan);
      }
      break;
    }
    throw std::logic_error("the selected term does not complete the request");
  }

  // Calls `visit` with the created wrappers of the plan made of `Made`: none,
  // or the wrappers the creating forms make, a wrapped element or an adopted
  // pointer, outermost first, which the wrappers of `Made` must have; or
  // returns visit.unsupported().
  template <typename Made, typename Visit>
  static decltype(auto) visit_wrappers(const request_plan &plan, Visit &visit) {
    const operation *forms = plan.creating;
    switch (plan.created) {
    case 0:
      return visit.template apply<created_wrappers<Made>>();
    case 1:
      return visit_wrapper<Made>(forms[0], visit);
    case 2:
      switch (forms[1]) {
      case operation::wrap:
        return visit_wrapper<Made, operation::wrap>(forms[0], visit);
      case operation::adopt:
        return visit_wrapper<Made, operation::adopt>(forms[0], visit);
      default:
        break;
      }
      break;
    default:
      break;
    }
    return visit.unsupported();
  }

  // Calls `visit` with the created wrappers of `Outer...` over the wrapper the
  // creating form `form` makes.
  template <typename Made, operation... Outer, typename Visit>
  static decltype(auto) visit_wrapper(operation form, Visit &visit) {
    switch (form) {
    case operation::wrap:
      return visit
          .template apply<created_wrappers<Made, Outer..., operation::wrap>>();
    case operation::adopt:
      return visit
          .template apply<created_wrappers<Made, Outer..., operation::adopt>>();
    default:
      return visit.unsupported();
    }
  }

  // Whether the binding makes the boundary object of the wrappers of a plan.
  struct makes_boundary {
    entry &binding;
    std::size_t term;

    template <typename Wrappers> bool apply() const {
      if constexpr (Wrappers::valid) {
        return binding.template makes<typename Wrappers::boundary>(term);
      } else {
        return false;
      }
    }

    static bool unsupported() { return false; }
  };

  template <typename Request> struct constructs_request {
    Resolver &resolver;
    entry &binding;
    std::size_t term;

    template <typename Wrappers> Request apply() const {
      return construct_with<Request, Wrappers>(resolver, binding, term);
    }

    [[noreturn]] static Request unsupported() {
      throw std::logic_error("the selected term does not complete the request");
    }
  };

  // Makes the request from the boundary object by the creating rules of the
  // plan. The binding makes the boundary object of the type of its term,
  // which has the qualifiers of the request when the request names them, and
  // otherwise none; the request asks for the qualified one first.
  template <typename Request>
  static Request construct(Resolver &resolver, entry &binding, std::size_t term,
                           const request_plan &plan) {
    using value_type = typename request_traits<Request>::value_type;
    using made_type = typename request_traits<Request>::made_type;
    constructs_request<Request> constructs{resolver, binding, term};
    if constexpr (!std::is_same_v<value_type, made_type>) {
      makes_boundary qualified{binding, term};
      if (visit_wrappers<value_type>(plan, qualified)) {
        return visit_wrappers<value_type>(plan, constructs);
      }
    }
    return visit_wrappers<made_type>(plan, constructs);
  }

  // Makes the request by the created wrappers `Wrappers` from the boundary
  // object of the term; the wrappers a request cannot be made of are not
  // instantiated.
  template <typename Request, typename Wrappers>
  static Request construct_with(Resolver &resolver, entry &binding,
                                std::size_t term) {
    if constexpr (Wrappers::valid) {
      using made = decltype(Wrappers::make(
          std::declval<typename Wrappers::boundary (&)()>()));
      using value_type = typename request_traits<Request>::value_type;
      // An rvalue reference refers to a kept object of its qualified type,
      // made from the object the wrappers make, or the object itself, which is
      // constructed in place; any other request converts.
      if constexpr (std::is_rvalue_reference_v<Request>
                        ? std::is_same_v<std::remove_cv_t<made>,
                                         std::remove_cv_t<value_type>> ||
                              std::is_constructible_v<value_type, made>
                        : detail::casts_to_v<made, Request>) {
        return make_with<Request, Wrappers>(resolver, binding, term);
      }
    }
    throw std::logic_error("the selected term does not complete the request");
  }

  template <typename Request, typename Wrappers>
  static Request make_with(Resolver &resolver, entry &binding,
                           std::size_t term) {
    using value_type = typename request_traits<Request>::value_type;
    using boundary = typename Wrappers::boundary;
    if (!binding.template makes<boundary>(term)) {
      throw std::logic_error("the selected term does not complete the request");
    }
    auto make = [&] {
      return Wrappers::make(
          [&] { return binding.template make<boundary>(term, resolver); });
    };
    if constexpr (std::is_rvalue_reference_v<Request>) {
      resolver.retained().enter(resolver.transactions());
      value_type &kept = resolver.retained().template emplace<value_type>(
          [&] { return value_type(make()); });
      return static_cast<Request>(kept);
    } else {
      return static_cast<Request>(make());
    }
  }

  const lookup_table *table_;
  transaction_log transactions_;
  bytes_type bytes_;
  std::vector<entry_pointer> entries_;
  // The memos of the request types by request_id, and the number of
  // registrations they are valid for.
  mutable std::vector<std::unique_ptr<memo_base>> memos_;
  std::size_t generation_ = 0;
};

} // namespace dingo::core
