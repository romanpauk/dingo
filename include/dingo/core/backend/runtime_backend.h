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

#include <dingo/core/backend/diagnose.h>
#include <dingo/core/backend/local.h>
#include <dingo/core/backend/memo.h>
#include <dingo/core/backend/slot.h>
#include <dingo/core/engine/derive.h>
#include <dingo/core/engine/execute.h>
#include <dingo/core/engine/lookup.h>
#include <dingo/core/engine/request.h>
#include <dingo/core/primitives/exceptions.h>
#include <dingo/core/primitives/retained.h>
#include <dingo/core/primitives/shape.h>
#include <dingo/core/primitives/slot.h>
#include <dingo/core/primitives/type_name.h>
#include <dingo/core/primitives/type_path.h>
#include <dingo/core/projection/collection.h>
#include <dingo/core/vocabulary/diagnostics.gen.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
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
  // object.
  using thunk = void *(*)(runtime_entry &, Resolver &);

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

  // Whether the key value of the binding equals `value`, a value of the type
  // of its key domain; a key without a value is always equal.
  virtual bool key_equals(const void *value) const = 0;

  // Whether the objects the binding borrows keep their addresses: its scope
  // is stable and its slot settled.
  virtual bool settled(Resolver &resolver) const = 0;

  const binding_descriptor &descriptor() const { return descriptor_; }
  const terms &published() const { return published_; }
  std::uint8_t exposed() const { return exposed_; }
  const completion_plan &plan(std::size_t term) const { return plans_[term]; }

  // Appends the name of the stored type, for diagnostics.
  void append_stored_name(std::string &text) const { stored_name_(text); }

  void *borrow(std::size_t term, Resolver &resolver) {
    return thunks_[term](*this, resolver);
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

  // The local bindings of the registration.
  local_bindings<Resolver, typename Binding::locals> &locals() {
    return locals_;
  }

  bool key_equals(const void *value) const override {
    return key_store_type::equal(key_.value(), value);
  }

  bool settled(Resolver &resolver) const override {
    if constexpr (info(Binding::scope).stable) {
      return slot_.settled(resolver);
    } else {
      return false;
    }
  }

private:
  template <std::size_t Term>
  static constexpr bool borrows =
      plan_v<Binding, Term>.kind != completion::construct;

  template <std::size_t Term>
  static void *borrow_term(entry &binding, Resolver &resolver) {
    if constexpr (!walk_supported_v<Binding, Term> || !borrows<Term>) {
      throw std::logic_error(
          "the next implementation does not provide this route yet");
    } else {
      auto &self = static_cast<runtime_binding &>(binding);
      auto view = self.view(resolver);
      auto &object = walk_to_boundary<Binding, Term>(
          self.slot_.context(),
          [&]() -> decltype(auto) { return self.slot_.get(view); });
      // The address of a const or volatile object is erased too; the request
      // restores the qualifiers of the type it names.
      return const_cast<void *>(
          static_cast<const volatile void *>(std::addressof(object)));
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

  key_store_type key_;
  local_bindings<Resolver, typename Binding::locals> locals_;
  binding_slot<Binding> slot_;
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

// The positions of the arguments of a registration: the key values and the
// arguments of the slot.
template <typename... Args> struct registration_arguments {
  static constexpr bool keys[] = {is_key_value<std::decay_t<Args>>::value...,
                                  false};

  static constexpr std::size_t key_count() {
    std::size_t count = 0;
    for (std::size_t index = 0; index < sizeof...(Args); ++index) {
      count += keys[index] ? 1 : 0;
    }
    return count;
  }

  static constexpr std::size_t key_index() {
    std::size_t index = 0;
    while (index < sizeof...(Args) && !keys[index]) {
      ++index;
    }
    return index;
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
template <typename Resolver> class runtime_backend {
  using entry = runtime_entry<Resolver>;

public:
  // A backend of the lookup declarations of `table`, which outlives it.
  explicit runtime_backend(const lookup_table &table) : table_(&table) {}

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
    static_assert(Binding::status == registration_status::registered,
                  "a binding cannot be stored by its scope");
    using arguments = detail::registration_arguments<Args...>;
    static_assert(arguments::key_count() <= 1,
                  "a registration takes at most one key value");
    auto all = std::forward_as_tuple(std::forward<Args>(args)...);
    std::unique_ptr<runtime_binding<Binding, Resolver>> added =
        make_entry<Binding, arguments, std::tuple<std::decay_t<Args>...>>(
            all, std::make_index_sequence<sizeof...(Args) -
                                          arguments::key_count()>());
    if constexpr (Binding::key_type::descriptor.kind == key_kind::fixed ||
                  Binding::key_type::descriptor.kind == key_kind::runtime) {
      // A key value outside the bound of a dense array is never stored.
      const std::size_t bound = key_bound(*table_, Binding::descriptor);
      if (bound != unbounded && !key_store<typename Binding::key_type>::within(
                                    added->key_value(), bound)) {
        raise_out_of_range<Binding>();
      }
    }
    if (held(added->key_value())) {
      raise_already_registered<Binding>();
    }
    for (const auto &existing : entries_) {
      if (conflicts(*table_, existing->descriptor(), Binding::descriptor) &&
          existing->key_equals(added->key_value())) {
        raise_already_registered<Binding>();
      }
    }
    auto &locals = added->locals();
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
  Request resolve_named(Resolver &resolver, const void *key_value) {
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
  Request select_named(Resolver &resolver, const void *key_value,
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
      return otherwise();
    }
    return resolve_from<Request>(resolver, *named);
  }

  // Adds every binding a request of the element type of `Collection` by
  // `Key` names to `collection`, in the order of registration.
  template <typename Collection, typename Key, typename Append>
  void collect(Resolver &resolver, Collection &collection,
               const void *key_value, Append &append) {
    using element = typename collection_traits<Collection>::resolve_type;
    memo<element, Key>().visit(key_value, [&](const candidate_type &found) {
      append(collection, resolve_from<element>(resolver, found));
      return true;
    });
  }

private:
  using candidate_type = candidate<entry>;

  static void undo_add(void *backend) noexcept {
    auto &self = *static_cast<runtime_backend *>(backend);
    self.entries_.pop_back();
    ++self.generation_;
  }

  // The memo of a request by `Key`, made by its first resolution and made
  // again by the first one after a registration.
  template <typename Request, typename Key>
  const request_memo<entry, Key> &memo() const {
    using memo_type = request_memo<entry, Key>;
    const std::size_t id = request_id<Request, Key>();
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
  DINGO_NOINLINE void remember(request_memo<entry, Key> &found) const {
    constexpr request_descriptor request =
        request_traits<Request, Key>::descriptor;
    found.clear();
    for (const auto &binding : entries_) {
      if (finds(binding->descriptor(), request)) {
        const std::size_t term =
            select_term(binding->published(), request, binding->exposed());
        found.add({binding.get(), term, binding->key_value(),
                   term == no_term ? completion::construct
                                   : binding->plan(term).kind});
      }
    }
    found.index(
        declaration_of(*table_, innermost_leaf(request.value), request.key));
    found.generation = generation_;
  }
  template <typename Binding, typename Arguments, typename Decayed,
            typename All, std::size_t... Slots>
  static std::unique_ptr<runtime_binding<Binding, Resolver>>
  make_entry(All &all, std::index_sequence<Slots...> /*slots*/) {
    using arguments = Arguments;
    if constexpr (arguments::key_count() == 0) {
      return std::make_unique<runtime_binding<Binding, Resolver>>(
          without_key_value(),
          std::get<arguments::slot_index(Slots)>(std::move(all))...);
    } else {
      using value = std::tuple_element_t<arguments::key_index(), Decayed>;
      return std::make_unique<runtime_binding<Binding, Resolver>>(
          with_key_value(),
          runtime_key_of<value>::value(std::get<arguments::key_index()>(all)),
          std::get<arguments::slot_index(Slots)>(std::move(all))...);
    }
  }

  template <typename Request>
  static Request resolve_from(Resolver &resolver, const candidate_type &found) {
    if (found.term == no_term) {
      raise_not_convertible<Request>(*found.binding);
    }
    return complete<Request>(resolver, found);
  }

  // The address the term of `found` borrows, kept once its binding is
  // settled.
  static void *borrowed(Resolver &resolver, const candidate_type &found) {
    if (found.resolved != nullptr) {
      return found.resolved;
    }
    void *address = found.binding->borrow(found.term, resolver);
    if (found.binding->settled(resolver)) {
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
    switch (found.kind) {
    case completion::refer:
      if constexpr (std::is_lvalue_reference_v<Request>) {
        return static_cast<Request>(
            *static_cast<value_type *>(borrowed(resolver, found)));
      }
      break;
    case completion::copy:
      if constexpr (!std::is_reference_v<Request> &&
                    std::is_copy_constructible_v<value_type> &&
                    detail::casts_to_v<value_type, Request>) {
        return static_cast<Request>(value_type(
            *static_cast<const value_type *>(borrowed(resolver, found))));
      }
      break;
    case completion::address:
      if constexpr (std::is_pointer_v<value_type> &&
                    detail::casts_to_v<value_type, Request>) {
        return static_cast<Request>(
            static_cast<value_type>(borrowed(resolver, found)));
      }
      break;
    case completion::construct:
      if constexpr (!std::is_lvalue_reference_v<Request>) {
        return construct<Request>(resolver, *found.binding, found.term,
                                  found.binding->plan(found.term));
      }
      break;
    }
    throw std::logic_error("the selected term does not complete the request");
  }

  // Makes the request from the boundary object by the creating rules of the
  // plan: none, wrapping into an optional, or adopting a pointer, which may be
  // wrapped into an optional in turn.
  template <typename Request>
  static Request construct(Resolver &resolver, entry &binding, std::size_t term,
                           const completion_plan &plan) {
    using made_type = typename request_traits<Request>::made_type;
    const rule_id *rules = plan.creating;
    switch (plan.created) {
    case 0:
      return construct_with<Request, created_layers<made_type>>(resolver,
                                                                binding, term);
    case 1:
      switch (rules[0]) {
      case rule_id::wrap_optional:
        return construct_with<
            Request, created_layers<made_type, rule_id::wrap_optional>>(
            resolver, binding, term);
      case rule_id::adopt_pointer_owned:
        return construct_with<
            Request, created_layers<made_type, rule_id::adopt_pointer_owned>>(
            resolver, binding, term);
      case rule_id::adopt_pointer_shared:
        return construct_with<
            Request, created_layers<made_type, rule_id::adopt_pointer_shared>>(
            resolver, binding, term);
      default:
        break;
      }
      break;
    case 2:
      if (rules[1] == rule_id::wrap_optional) {
        switch (rules[0]) {
        case rule_id::adopt_pointer_owned:
          return construct_with<
              Request, created_layers<made_type, rule_id::wrap_optional,
                                      rule_id::adopt_pointer_owned>>(
              resolver, binding, term);
        case rule_id::adopt_pointer_shared:
          return construct_with<
              Request, created_layers<made_type, rule_id::wrap_optional,
                                      rule_id::adopt_pointer_shared>>(
              resolver, binding, term);
        default:
          break;
        }
      }
      break;
    default:
      break;
    }
    throw std::logic_error("the selected term does not complete the request");
  }

  // Makes the request by the created layers `Layers` from the boundary object
  // of the term; the layers a request cannot be made of are not instantiated.
  template <typename Request, typename Layers>
  static Request construct_with(Resolver &resolver, entry &binding,
                                std::size_t term) {
    if constexpr (Layers::valid) {
      using made = decltype(Layers::make(
          std::declval<typename Layers::boundary (&)()>()));
      if constexpr (detail::casts_to_v<made, Request>) {
        return make_with<Request, Layers>(resolver, binding, term);
      }
    }
    throw std::logic_error("the selected term does not complete the request");
  }

  template <typename Request, typename Layers>
  static Request make_with(Resolver &resolver, entry &binding,
                           std::size_t term) {
    using made_type = typename request_traits<Request>::made_type;
    using boundary = typename Layers::boundary;
    if (!binding.template makes<boundary>(term)) {
      throw std::logic_error("the selected term does not complete the request");
    }
    auto make = [&] {
      return Layers::make(
          [&] { return binding.template make<boundary>(term, resolver); });
    };
    if constexpr (std::is_rvalue_reference_v<Request>) {
      made_type &kept = resolver.retained().template emplace<made_type>(make);
      resolver.transactions().record(&resolver.retained(),
                                     &retained_objects::undo_last);
      return static_cast<Request>(kept);
    } else {
      return static_cast<Request>(make());
    }
  }

  const lookup_table *table_;
  transaction_log transactions_;
  std::vector<std::unique_ptr<entry>> entries_;
  // The memos of the request types by request_id, and the number of
  // registrations they are valid for.
  mutable std::vector<std::unique_ptr<memo_base>> memos_;
  std::size_t generation_ = 0;
};

} // namespace dingo::core
