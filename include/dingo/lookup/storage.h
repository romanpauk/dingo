//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <memory>
#include <type_traits>
#include <utility>

namespace dingo::detail {

template <typename Value, typename Allocator>
using lookup_storage_allocator_t =
    typename std::allocator_traits<Allocator>::template rebind_alloc<Value>;

template <typename StorageAllocator, typename Allocator>
StorageAllocator make_lookup_storage_allocator(Allocator &allocator) {
  return StorageAllocator(allocator);
}

template <typename Storage, typename Allocator> struct lookup_storage_factory {
  static Storage make(Allocator &allocator) { return Storage(allocator); }
};

template <typename Storage, typename Allocator>
Storage make_lookup_storage(Allocator &allocator) {
  return lookup_storage_factory<Storage, Allocator>::make(allocator);
}

template <typename Storage, typename Visitor> struct lookup_storage_visitor {
private:
  template <typename S = Storage>
  static auto visit_impl(const S &storage, Visitor &visitor, int)
      -> decltype(storage.for_each(visitor), bool()) {
    storage.for_each(visitor);
    return true;
  }

  template <typename S = Storage>
  static auto visit_impl(const S &storage, Visitor &visitor, long)
      -> decltype(visitor((*storage.begin()).first, (*storage.begin()).second),
                  bool()) {
    for (const auto &entry : storage) {
      visitor(entry.first, entry.second);
    }
    return true;
  }

  static bool visit_impl(const Storage &, Visitor &, ...) { return false; }

public:
  static bool visit(const Storage &storage, Visitor &visitor) {
    return visit_impl(storage, visitor, 0);
  }
};

template <typename Storage, typename Visitor>
bool visit_lookup_storage(const Storage &storage, Visitor &&visitor) {
  auto &&visitor_ref = visitor;
  return lookup_storage_visitor<
      Storage,
      std::remove_reference_t<decltype(visitor_ref)>>::visit(storage,
                                                             visitor_ref);
}

} // namespace dingo::detail
