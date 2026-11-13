//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/construction_scope.h>
#include <dingo/context/resolution.h>

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
  ~retention() { clear(); }

  template <typename T, typename Context, typename Make>
  T &keep(const void *key, Context &context, Make &&make) {
    for (node *entry = head_; entry != nullptr; entry = entry->next) {
      if (entry->key == key) {
        return *static_cast<T *>(entry->object);
      }
    }
    if constexpr (is_runtime_context_v<Context>) {
      auto &object = context.template construct_from<T>(
          persistent_scope, std::forward<Make>(make));
      head_ = std::addressof(context.template construct<node>(
          persistent_scope, node{key, std::addressof(object), head_, nullptr}));
      context.on_rollback(
          +[](void *self, void *) noexcept {
            auto &kept = *static_cast<retention *>(self);
            kept.head_ = kept.head_->next;
          },
          this);
      return object;
    } else {
      auto made = std::make_unique<owned<T>>(std::forward<Make>(make));
      made->entry = node{key, std::addressof(made->object), head_,
                         +[](node *entry) noexcept {
                           delete static_cast<owned<T> *>(entry->self);
                         }};
      made->entry.self = made.get();
      head_ = std::addressof(made->entry);
      return made.release()->object;
    }
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
