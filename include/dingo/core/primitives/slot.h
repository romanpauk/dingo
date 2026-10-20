//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Lifetime slots: where a binding keeps its object.

#include <dingo/core/base/mechanics.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo::core {

// Destroys an object, element by element for an array, which std::destroy_at
// handles only since C++20.
template <typename Type> void destroy_object(Type *object) {
  if constexpr (std::is_array_v<Type>) {
    for (auto &element : *object) {
      destroy_object(std::addressof(element));
    }
  } else {
    object->~Type();
  }
}

// Holds an object constructed on first use and destroyed with the slot. A
// request of the object while it is being constructed is a recursion.
template <typename Type> class lazy_slot {
public:
  lazy_slot() = default;
  lazy_slot(const lazy_slot &) = delete;
  lazy_slot &operator=(const lazy_slot &) = delete;

  ~lazy_slot() {
    if (state_ == state::constructed) {
      destroy_object(get());
    }
  }

  bool constructed() const { return state_ == state::constructed; }
  bool constructing() const { return state_ == state::constructing; }

  // The object, which constructed() tells exists.
  Type &object() { return *get(); }

  // Destroys the object, if it exists, so the slot is empty.
  void reset() noexcept {
    if (state_ == state::constructed) {
      destroy_object(get());
    }
    state_ = state::empty;
  }

  // Constructs the object in place by `construct(void *)` unless it exists;
  // the caller reports a request while it is being constructed.
  template <typename Construct> Type &get_or_construct(Construct &&construct) {
    if (state_ != state::constructed) {
      state_ = state::constructing;
      struct reset {
        state &value;
        ~reset() {
          if (value == state::constructing) {
            value = state::empty;
          }
        }
      } guard{state_};
      construct(static_cast<void *>(storage_));
      state_ = state::constructed;
    }
    return *get();
  }

private:
  enum class state : std::uint8_t { empty, constructing, constructed };

  Type *get() { return std::launder(reinterpret_cast<Type *>(storage_)); }

  alignas(Type) unsigned char storage_[sizeof(Type)];
  state state_ = state::empty;
};

// Holds an object supplied by the caller: a reference and an array are kept
// as the address of the caller's object, any other object by value.
template <typename Type> class external_slot {
public:
  template <typename Instance>
  explicit external_slot(Instance &&instance)
      : instance_(std::forward<Instance>(instance)) {}

  Type &get() { return instance_; }

private:
  Type instance_;
};

template <typename Type> class external_slot<Type &> {
public:
  explicit external_slot(Type &instance) : instance_(&instance) {}

  Type &get() { return *instance_; }

private:
  Type *instance_;
};

template <typename Type, std::size_t Extent>
class external_slot<Type[Extent]> : public external_slot<Type (&)[Extent]> {
public:
  using external_slot<Type (&)[Extent]>::external_slot;
};

// The storage of an element a sharing handle publishes before the element is
// constructed.
template <typename Type> struct publication_block {
  alignas(Type) unsigned char storage[sizeof(Type)];
  bool constructed = false;

  Type *object() { return reinterpret_cast<Type *>(storage); }

  static publication_block *of(Type *object) {
    return reinterpret_cast<publication_block *>(object);
  }
};

// Frees the block of a published element, destroying the element only when
// it was constructed.
template <typename Type> struct publication_deleter {
  void operator()(Type *object) const noexcept {
    auto *block = publication_block<Type>::of(object);
    if (block->constructed) {
      destroy_object(object);
    }
    delete block;
  }
};

// The transaction of a container: the effects on the container of its
// outermost request in progress, and of the requests it makes of the
// container, each recorded with the function that undoes it. A request that
// fails undoes them in the reverse order they happened, and one that returns
// keeps them. spec: transaction_rules
class transaction_log {
public:
  using undo_function = void (*)(void *) noexcept;

  // Runs `request` as a request of the container.
  template <typename Request> decltype(auto) run(Request &&request) {
    struct leave {
      transaction_log &log;
      ~leave() {
        if (--log.depth_ == 0 && !log.entries_.empty()) {
          log.finish();
        }
      }
    };
    ++depth_;
    leave guard{*this};
    return request();
  }

  // Records an effect on the container, which `undo` undoes, when a request
  // is in progress; an effect outside one is kept.
  void record(void *target, undo_function undo) {
    if (depth_ == 0) {
      return;
    }
    if (entries_.empty()) {
      // The exceptions in flight when the transaction began, so its own
      // failure is told from a request made while one unwinds.
      unwinding_ = std::uncaught_exceptions();
    }
    entries_.push_back(entry{target, undo});
  }

  // Whether a request is in progress, whose failure undoes its effects.
  bool active() const { return depth_ != 0; }

private:
  struct entry {
    void *target;
    undo_function undo;
  };

  DINGO_CORE_COLD DINGO_NOINLINE void finish() noexcept {
    if (std::uncaught_exceptions() > unwinding_) {
      for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        it->undo(it->target);
      }
    }
    entries_.clear();
  }

  std::vector<entry> entries_;
  std::size_t depth_ = 0;
  int unwinding_ = 0;
};

// The transaction of a container whose bindings construct no object they
// keep: there is nothing to undo.
struct no_transaction_log {
  template <typename Request> decltype(auto) run(Request &&request) {
    return request();
  }

  void record(void * /*target*/,
              transaction_log::undo_function /*undo*/) noexcept {}

  bool active() const { return false; }
};

} // namespace dingo::core
