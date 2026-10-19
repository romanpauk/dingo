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

#include <dingo/core/backend/slot.h>
#include <dingo/core/engine/derive.h>
#include <dingo/core/engine/execute.h>
#include <dingo/core/engine/request.h>
#include <dingo/core/primitives/exceptions.h>
#include <dingo/core/primitives/retained.h>
#include <dingo/core/primitives/shape.h>
#include <dingo/core/primitives/slot.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo::core {

class runtime_backend;
class runtime_entry;

// Walks a term to its boundary and returns the boundary object.
template <typename Boundary> struct boundary_maker {
  Boundary (*make)(runtime_entry &, runtime_backend &);
};

class runtime_entry {
public:
  // Walks a term to its boundary and returns the address of the borrowed
  // object. The backend resolves the dependencies of the object.
  using thunk = void *(*)(runtime_entry &, runtime_backend &);

  runtime_entry(const binding_descriptor &descriptor, const terms &published,
                std::uint8_t exposed, const completion_plan *plans,
                const void *const *boundaries, const thunk *thunks,
                const void *const *makers)
      : descriptor_(descriptor), published_(published), exposed_(exposed),
        plans_(plans), boundaries_(boundaries), thunks_(thunks),
        makers_(makers) {}

  virtual ~runtime_entry() = default;

  const binding_descriptor &descriptor() const { return descriptor_; }
  const terms &published() const { return published_; }
  std::uint8_t exposed() const { return exposed_; }
  const completion_plan &plan(std::size_t term) const { return plans_[term]; }

  void *borrow(std::size_t term, runtime_backend &backend) {
    return thunks_[term](*this, backend);
  }

  // Whether the binding constructs a boundary object of type `Boundary` for
  // a term.
  template <typename Boundary> bool makes(std::size_t term) const {
    return boundaries_[term] == type_address_v<Boundary>;
  }

  // The boundary object of a term, whose type `makes` confirmed.
  template <typename Boundary>
  Boundary make(std::size_t term, runtime_backend &backend) {
    return static_cast<const boundary_maker<Boundary> *>(makers_[term])
        ->make(*this, backend);
  }

private:
  const binding_descriptor &descriptor_;
  const terms &published_;
  std::uint8_t exposed_;
  const completion_plan *plans_;
  const void *const *boundaries_;
  const thunk *thunks_;
  const void *const *makers_;
};

template <typename Binding> class runtime_binding final : public runtime_entry {
  using terms_sequence = std::make_index_sequence<Binding::published.size>;

public:
  template <typename... Args>
  explicit runtime_binding(Args &&...args)
      : runtime_entry(Binding::descriptor, Binding::published, Binding::exposed,
                      plans(terms_sequence()), boundaries(terms_sequence()),
                      thunks(terms_sequence()), makers(terms_sequence())),
        slot_(std::forward<Args>(args)...) {}

private:
  template <std::size_t Term>
  static constexpr bool borrows =
      plan_v<Binding, Term>.kind != completion::construct;

  template <std::size_t Term>
  static void *borrow_term(runtime_entry &entry, runtime_backend &backend) {
    if constexpr (!walk_supported_v<Binding, Term> || !borrows<Term>) {
      throw std::logic_error(
          "the next implementation does not provide this route yet");
    } else {
      auto &self = static_cast<runtime_binding &>(entry);
      auto &object = walk_to_boundary<Binding, Term>(
          self.slot_.context(),
          [&]() -> decltype(auto) { return self.slot_.get(backend); });
      return const_cast<void *>(
          static_cast<const void *>(std::addressof(object)));
    }
  }

  template <std::size_t Term>
  static boundary_t<Binding, Term> make_term(runtime_entry &entry,
                                             runtime_backend &backend) {
    auto &self = static_cast<runtime_binding &>(entry);
    return walk_to_boundary<Binding, Term>(
        self.slot_.context(),
        [&]() -> decltype(auto) { return self.slot_.get(backend); });
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
  static constexpr boundary_maker<boundary_t<Binding, Term>> maker{
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

  binding_slot<Binding> slot_;
};

class runtime_backend {
public:
  template <typename Binding, typename... Args> void add(Args &&...args) {
    static_assert(Binding::status == registration_status::registered,
                  "a binding cannot be stored by its scope");
    entries_.push_back(std::make_unique<runtime_binding<Binding>>(
        std::forward<Args>(args)...));
  }

  template <typename Request> Request resolve() {
    using traits = request_traits<Request>;
    constexpr request_descriptor request = traits::descriptor;
    runtime_entry *found = nullptr;
    for (const auto &entry : entries_) {
      if (finds(entry->descriptor(), request)) {
        if (found != nullptr) {
          throw type_ambiguous_exception("type ambiguous");
        }
        found = entry.get();
      }
    }
    if (found == nullptr) {
      throw type_not_found_exception("type not found");
    }
    std::size_t term =
        select_term(found->published(), request, found->exposed());
    if (term == no_term) {
      throw type_not_convertible_exception("type not convertible");
    }
    return complete<Request>(*found, term);
  }

private:
  template <typename Request>
  Request complete(runtime_entry &entry, std::size_t term) {
    using value_type = typename request_traits<Request>::value_type;
    const completion_plan &plan = entry.plan(term);
    switch (plan.kind) {
    case completion::refer:
      if constexpr (std::is_lvalue_reference_v<Request>) {
        return static_cast<Request>(
            *static_cast<value_type *>(entry.borrow(term, *this)));
      }
      break;
    case completion::copy:
      if constexpr (!std::is_reference_v<Request> &&
                    std::is_copy_constructible_v<value_type>) {
        return static_cast<Request>(value_type(
            *static_cast<const value_type *>(entry.borrow(term, *this))));
      }
      break;
    case completion::address:
      if constexpr (std::is_pointer_v<value_type>) {
        return static_cast<Request>(
            static_cast<value_type>(entry.borrow(term, *this)));
      }
      break;
    case completion::construct:
      if constexpr (!std::is_lvalue_reference_v<Request>) {
        return construct<Request>(entry, term, plan);
      }
      break;
    }
    throw std::logic_error("the selected term does not complete the request");
  }

  // Makes the request from the boundary object by the creating rules of the
  // plan: none, wrapping into an optional, or adopting a pointer, which may be
  // wrapped into an optional in turn.
  template <typename Request>
  Request construct(runtime_entry &entry, std::size_t term,
                    const completion_plan &plan) {
    using value_type = typename request_traits<Request>::value_type;
    const rule_id *rules = plan.creating;
    switch (plan.created) {
    case 0:
      return construct_with<Request, created_layers<value_type>>(entry, term);
    case 1:
      switch (rules[0]) {
      case rule_id::wrap_optional:
        return construct_with<
            Request, created_layers<value_type, rule_id::wrap_optional>>(entry,
                                                                         term);
      case rule_id::adopt_pointer_owned:
        return construct_with<
            Request, created_layers<value_type, rule_id::adopt_pointer_owned>>(
            entry, term);
      case rule_id::adopt_pointer_shared:
        return construct_with<
            Request, created_layers<value_type, rule_id::adopt_pointer_shared>>(
            entry, term);
      default:
        break;
      }
      break;
    case 2:
      if (rules[1] == rule_id::wrap_optional) {
        switch (rules[0]) {
        case rule_id::adopt_pointer_owned:
          return construct_with<
              Request, created_layers<value_type, rule_id::wrap_optional,
                                      rule_id::adopt_pointer_owned>>(entry,
                                                                     term);
        case rule_id::adopt_pointer_shared:
          return construct_with<
              Request, created_layers<value_type, rule_id::wrap_optional,
                                      rule_id::adopt_pointer_shared>>(entry,
                                                                      term);
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

  template <typename Request, typename Layers>
  Request construct_with(runtime_entry &entry, std::size_t term) {
    using value_type = typename request_traits<Request>::value_type;
    using boundary = typename Layers::boundary;
    if constexpr (Layers::valid) {
      if (entry.makes<boundary>(term)) {
        auto make = [&] {
          return Layers::make(
              [&] { return entry.make<boundary>(term, *this); });
        };
        if constexpr (std::is_rvalue_reference_v<Request>) {
          return static_cast<Request>(
              retained_.template emplace<value_type>(make));
        } else {
          return static_cast<Request>(make());
        }
      }
    }
    throw std::logic_error("the selected term does not complete the request");
  }

  retained_objects retained_;
  std::vector<std::unique_ptr<runtime_entry>> entries_;
};

} // namespace dingo::core
