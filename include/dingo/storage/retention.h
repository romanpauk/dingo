//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/construction_scope.h>

#include <cstddef>
#include <memory>
#include <new>
#include <utility>

namespace dingo {
namespace detail {
// What a binding keeps of what it converts or composes of what it keeps, so
// that a reference or a pointer to it lives as long as the binding: one
// object per type, under the key of the type, made the first time it is
// asked for. The objects of a runtime container are made in its persistent
// memory and forgotten when the transaction that made them rolls back; those
// of a static container, which may have no persistent memory, are owned here.
class retention {
public:
  retention() = default;
  retention(const retention &) = delete;
  retention &operator=(const retention &) = delete;
  ~retention() {
    // What keeps nothing is destroyed at once, where it is known to.
    if (head_ != nullptr) {
      clear();
    }
  }

  // Keeps a T, made of make, owned here: of a static container.
  template <typename T, typename Make> T &keep(const void *key, Make &&make) {
    if (void *object = find(key)) {
      return *static_cast<T *>(object);
    }
    auto made = std::make_unique<owned<T>>(std::forward<Make>(make));
    made->entry = node{key, std::addressof(made->object), head_,
                       +[](node *entry) noexcept {
                         delete static_cast<owned<T> *>(entry->self);
                       }};
    made->entry.self = made.get();
    head_ = std::addressof(made->entry);
    return made.release()->object;
  }

  // Keeps an object of a size and an alignment, which construct constructs
  // at the address it is given and destroy destroys, none where it has no
  // destructor, in the persistent memory of a runtime container.
  template <typename Context, typename ConstructFn>
  void *keep_erased(const void *key, Context &context, std::size_t size,
                    std::size_t alignment, void (*destroy)(void *) noexcept,
                    ConstructFn &&construct) {
    if (void *object = find(key)) {
      return object;
    }
    void *object =
        context.construct_erased(persistent_scope, size, alignment, destroy,
                                 std::forward<ConstructFn>(construct));
    head_ = std::addressof(context.template construct<node>(
        persistent_scope, node{key, object, head_, nullptr}));
    context.defer_rollback(
        +[](void *self, void *) noexcept {
          auto &kept = *static_cast<retention *>(self);
          kept.head_ = kept.head_->next;
        },
        this);
    return object;
  }

  // Forgets what is kept, and destroys what is owned here.
  void clear() noexcept {
    while (head_ != nullptr) {
      node *entry = head_;
      head_ = entry->next;
      if (entry->destroy != nullptr) {
        entry->destroy(entry);
      }
    }
  }

private:
  void *find(const void *key) const noexcept {
    for (node *entry = head_; entry != nullptr; entry = entry->next) {
      if (entry->key == key) {
        return entry->object;
      }
    }
    return nullptr;
  }

  struct node {
    const void *key;
    void *object;
    node *next;
    void (*destroy)(node *) noexcept;
    void *self = nullptr;
  };

  template <typename T> struct owned {
    template <typename Make>
    explicit owned(Make &&make) : object(std::forward<Make>(make)()) {}
    T object;
    node entry{};
  };

  node *head_ = nullptr;
};
} // namespace detail
} // namespace dingo
