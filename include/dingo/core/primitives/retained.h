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

#include <utility>

namespace dingo::core {

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

  ~retained_objects() {
    while (head_ != nullptr) {
      node *next = head_->next;
      head_->destroy(head_);
      head_ = next;
    }
  }

  // Keeps the object `make()` returns, constructed in place.
  template <typename Type, typename Make> Type &emplace(Make &&make) {
    return emplace_keyed<Type>(nullptr, std::forward<Make>(make));
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

} // namespace dingo::core
