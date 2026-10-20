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
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace dingo::core {

// Destroys an object, element by element for an array, which std::destroy_at
// handles only since C++20.
template <typename Type> DINGO_ALWAYS_INLINE void destroy_object(Type *object) {
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

  DINGO_ALWAYS_INLINE ~lazy_slot() {
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

// An effect of a transaction on its container, with the function that undoes
// it, which takes the target and the context recorded with it.
struct transaction_entry {
  void *target;
  void (*undo)(void *, void *) noexcept;
  void *context;
};

// Undoes `entries` in the reverse order they happened.
inline void undo_entries(transaction_entry *begin,
                         transaction_entry *end) noexcept {
  while (end != begin) {
    --end;
    end->undo(end->target, end->context);
  }
}

// The transaction of a container: the effects on the container of its
// outermost request in progress, and of the requests it makes of the
// container, each recorded with the function that undoes it. A request that
// fails undoes them in the reverse order they happened, and one that returns
// keeps them. spec: transaction_rules
//
// The effects are recorded in the frame of the outermost request, `Inline` of
// them without allocating, so the container holds only a pointer to them. The
// outermost request catches its own failure, so one made while another
// exception unwinds is told apart, and a request that returns checks nothing.
// Every request calls itself from one place, so a compiler inlines it. A log
// that never `Grows` records at most `Inline` effects, which its container
// proves.
template <std::size_t Inline, bool Grows = true> class basic_transaction_log {
public:
  using undo_function = void (*)(void *, void *) noexcept;

  // A container moves between its requests, when its log holds nothing.
  basic_transaction_log() = default;
  basic_transaction_log(basic_transaction_log && /*other*/) noexcept {}
  basic_transaction_log &
  operator=(basic_transaction_log && /*other*/) noexcept {
    return *this;
  }

  // Runs `request` as a request of the container.
  template <typename Request>
  DINGO_ALWAYS_INLINE decltype(auto) run(Request &&request) {
    journal frame;
    const bool outermost = journal_ == nullptr;
    if (outermost) {
      frame.entries = frame.inline_entries;
      frame.count = 0;
      frame.capacity = Inline;
      // The guard below forgets the frame before it ends.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 12
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-pointer"
#endif
      journal_ = &frame;
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 12
#pragma GCC diagnostic pop
#endif
    }
    struct leave {
      basic_transaction_log &log;
      journal &frame;
      bool outermost;
      ~leave() {
        if (outermost) {
          log.journal_ = nullptr;
          if (frame.entries != frame.inline_entries) {
            release(frame);
          }
        }
      }
    } guard{*this, frame, outermost};
    try {
      return request();
    } catch (...) {
      if (outermost) {
        undo_entries(frame.entries, frame.entries + frame.count);
      }
      throw;
    }
  }

  // Records an effect on the container, which `undo` undoes, when a request
  // is in progress; an effect outside one is kept.
  void record(void *target, undo_function undo, void *context = nullptr) {
    if (journal_ != nullptr) {
      if constexpr (Grows) {
        if (journal_->count == journal_->capacity) {
          grow(*journal_);
        }
      }
      journal_->entries[journal_->count++] =
          transaction_entry{target, undo, context};
    }
  }

  // Records an effect unless the transaction in progress recorded it.
  void record_once(void *target, undo_function undo, void *context = nullptr) {
    if (journal_ == nullptr) {
      return;
    }
    for (std::size_t index = 0; index < journal_->count; ++index) {
      if (journal_->entries[index].target == target &&
          journal_->entries[index].undo == undo) {
        return;
      }
    }
    record(target, undo, context);
  }

  // Whether a request is in progress, whose failure undoes its effects.
  bool active() const { return journal_ != nullptr; }

private:
  struct journal {
    transaction_entry *entries;
    std::size_t count;
    std::size_t capacity;
    transaction_entry inline_entries[Inline];
  };

  DINGO_CORE_COLD DINGO_NOINLINE static void grow(journal &frame) {
    auto *grown = new transaction_entry[frame.capacity * 2];
    for (std::size_t index = 0; index < frame.count; ++index) {
      grown[index] = frame.entries[index];
    }
    release(frame);
    frame.entries = grown;
    frame.capacity *= 2;
  }

  static void release(journal &frame) noexcept {
    if (frame.entries != frame.inline_entries) {
      delete[] frame.entries;
    }
  }

  journal *journal_ = nullptr;
};

// The transaction of a container of runtime bindings.
using transaction_log = basic_transaction_log<8>;

// The transaction of a container whose bindings construct no object they
// keep: there is nothing to undo.
struct no_transaction_log {
  template <typename Request> decltype(auto) run(Request &&request) {
    return request();
  }

  void record(void * /*target*/, void (* /*undo*/)(void *, void *) noexcept,
              void * /*context*/ = nullptr) noexcept {}

  void record_once(void * /*target*/,
                   void (* /*undo*/)(void *, void *) noexcept,
                   void * /*context*/ = nullptr) noexcept {}

  bool active() const { return false; }
};

} // namespace dingo::core
