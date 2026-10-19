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

// How the request completes a term the binding walked: the binding returns the
// address of a borrowed object, and the request refers to it, copies it, takes
// its address, or decays it; or the binding constructs the value at `out`.
enum class completion : std::uint8_t { refer, copy, address, construct };

class runtime_entry {
public:
  // Walks a term up to its erasure boundary and returns the address of the
  // borrowed object, or of the value it constructed at `out`.
  using thunk = void *(*)(runtime_entry &, void *out);

  runtime_entry(const binding_descriptor &descriptor, const terms &published,
                std::uint8_t exposed, const thunk *thunks)
      : descriptor_(descriptor), published_(published), exposed_(exposed),
        thunks_(thunks) {}

  virtual ~runtime_entry() = default;

  const binding_descriptor &descriptor() const { return descriptor_; }
  const terms &published() const { return published_; }
  std::uint8_t exposed() const { return exposed_; }
  void *run(std::size_t term, void *out) { return thunks_[term](*this, out); }

  completion completion_of(std::size_t term) const {
    const struct term &route = published_.items[term];
    if (route.category == value_category::lvalue) {
      return completion::refer;
    }
    switch (route.rules[route.rule_count - 1]) {
    case rule_id::copy:
      return completion::copy;
    case rule_id::address_of:
    case rule_id::decay_array:
    case rule_id::decay_unbounded_array:
      return completion::address;
    default:
      return completion::construct;
    }
  }

private:
  const binding_descriptor &descriptor_;
  const terms &published_;
  std::uint8_t exposed_;
  const thunk *thunks_;
};

template <typename Binding> class runtime_binding final : public runtime_entry {
public:
  template <typename... Args>
  explicit runtime_binding(Args &&...args)
      : runtime_entry(
            Binding::descriptor, Binding::published, Binding::exposed,
            thunks(std::make_index_sequence<Binding::published.size>())),
        slot_(std::forward<Args>(args)...) {}

private:
  template <std::size_t Term>
  static void *run_term(runtime_entry &entry, void *out) {
    if constexpr (!walk_supported_v<Binding, Term>) {
      throw std::logic_error(
          "the next implementation does not provide this route yet");
    } else {
      auto &self = static_cast<runtime_binding &>(entry);
      constexpr std::size_t steps = binding_steps<Binding, Term>();
      constexpr bool borrowed =
          steps < Binding::published.items[Term].rule_count ||
          Binding::published.items[Term].category == value_category::lvalue;
      if constexpr (borrowed) {
        auto &object = walk<Binding, Term, steps>(self.slot_.get());
        return const_cast<void *>(
            static_cast<const void *>(std::addressof(object)));
      } else {
        using value_type = std::remove_cv_t<
            std::remove_reference_t<walk_result_t<Binding, Term>>>;
        return ::new (out) value_type(walk<Binding, Term>(self.slot_.get()));
      }
    }
  }

  template <std::size_t... Terms>
  static const thunk *thunks(std::index_sequence<Terms...>) {
    static constexpr thunk table[] = {&run_term<Terms>...};
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
  static Request complete(runtime_entry &entry, std::size_t term) {
    using value_type = typename request_traits<Request>::value_type;
    switch (entry.completion_of(term)) {
    case completion::refer:
      if constexpr (std::is_lvalue_reference_v<Request>) {
        return static_cast<Request>(
            *static_cast<value_type *>(entry.run(term, nullptr)));
      }
      break;
    case completion::copy:
      if constexpr (!std::is_reference_v<Request> &&
                    std::is_copy_constructible_v<value_type>) {
        return static_cast<Request>(value_type(
            *static_cast<const value_type *>(entry.run(term, nullptr))));
      }
      break;
    case completion::address:
      if constexpr (std::is_pointer_v<value_type>) {
        return static_cast<Request>(
            static_cast<value_type>(entry.run(term, nullptr)));
      }
      break;
    case completion::construct:
      if constexpr (!std::is_lvalue_reference_v<Request>) {
        alignas(value_type) unsigned char buffer[sizeof(value_type)];
        auto *value = std::launder(static_cast<value_type *>(
            entry.run(term, static_cast<void *>(buffer))));
        struct destroy {
          value_type *value;
          ~destroy() { value->~value_type(); }
        } guard{value};
        return static_cast<Request>(std::move(*value));
      }
      break;
    }
    throw std::logic_error("the selected term does not complete the request");
  }

  std::vector<std::unique_ptr<runtime_entry>> entries_;
};

} // namespace dingo::core
