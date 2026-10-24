//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Objects kept until their owner is destroyed: the objects a request of an
// rvalue reference refers to when the request constructs them, and the
// objects a binding derives once so they can be referred to.

#include <dingo/base/config.h>

#include <utility>

namespace dingo {

class retained_objects {
public:
  retained_objects() = default;
  retained_objects(const retained_objects &) = delete;
  retained_objects &operator=(const retained_objects &) = delete;

  retained_objects(retained_objects &&other) noexcept : head_(other.head_) {
    other.head_ = nullptr;
  }

  retained_objects &operator=(retained_objects &&other) noexcept {
    std::swap(head_, other.head_);
    return *this;
  }

  DINGO_ALWAYS_INLINE ~retained_objects() { clear(); }

  // Keeps the object `make()` returns, constructed in place.
  template <typename Type, typename Make> Type &emplace(Make &&make) {
    return emplace_keyed<Type>(nullptr, std::forward<Make>(make));
  }

  // Records in `log` that the objects kept from now on are destroyed when
  // the transaction in progress fails, once per transaction, with the object
  // kept last before it.
  template <typename Log> void enter(Log &log) {
    log.record_once(this, &undo_to_mark, head_);
  }

  // Destroys the objects kept after `mark`, which a failed transaction
  // undoes.
  static void undo_to_mark(void *objects, void *mark) noexcept {
    auto &self = *static_cast<retained_objects *>(objects);
    while (self.head_ != static_cast<node *>(mark)) {
      node *last = self.head_;
      self.head_ = last->next;
      last->destroy(last);
    }
  }

  // Destroys every kept object.
  DINGO_ALWAYS_INLINE void clear() noexcept {
    while (head_ != nullptr) {
      node *next = head_->next;
      head_->destroy(head_);
      head_ = next;
    }
  }

protected:
  template <typename Type, typename Make>
  Type &emplace_keyed(const void *key, Make &&make) {
    auto *kept = new holder<Type>(head_, key, std::forward<Make>(make));
    head_ = kept;
    return kept->value;
  }

  // The object kept under `key`, or nullptr.
  template <typename Type> Type *find(const void *key) {
    for (node *kept = head_; kept != nullptr; kept = kept->next) {
      if (kept->key == key) {
        return &static_cast<holder<Type> *>(kept)->value;
      }
    }
    return nullptr;
  }

private:
  struct node {
    node *next;
    const void *key;
    void (*destroy)(node *);
  };

  template <typename Type> struct holder : node {
    template <typename Make>
    holder(node *following, const void *kept_key, Make &&make)
        : node{following, kept_key, &holder::destroy_holder}, value(make()) {}

    static void destroy_holder(node *kept) {
      delete static_cast<holder *>(kept);
    }

    Type value;
  };

  node *head_ = nullptr;
};

// Objects kept under a key, each derived once.
class kept_objects : private retained_objects {
public:
  using retained_objects::clear;

  // The object kept under `key`, made by `make()` when there is none. Every
  // object kept under one key has the same type.
  template <typename Type, typename Make>
  Type &get_or_emplace(const void *key, Make &&make) {
    if (Type *kept = find<Type>(key)) {
      return *kept;
    }
    return emplace_keyed<Type>(key, std::forward<Make>(make));
  }
};

} // namespace dingo
