//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Objects of any alignment allocated from an allocator of bytes, which need
// not align them: an allocation is padded for the alignment, and a header
// before the object records the bytes it took.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

namespace dingo::core {

template <typename Allocator>
using byte_allocator_t = typename std::allocator_traits<
    Allocator>::template rebind_alloc<unsigned char>;

namespace detail {
struct allocation_header {
  unsigned char *bytes;
  std::size_t size;
};
} // namespace detail

// Allocates `size` bytes aligned to `alignment` from `bytes`.
template <typename Bytes>
void *allocate_aligned(Bytes &bytes, std::size_t size, std::size_t alignment) {
  using header = detail::allocation_header;
  if (alignment < alignof(header)) {
    alignment = alignof(header);
  }
  const std::size_t total = size + alignment + sizeof(header);
  unsigned char *start = std::allocator_traits<Bytes>::allocate(bytes, total);
  const auto first = reinterpret_cast<std::uintptr_t>(start + sizeof(header));
  const std::uintptr_t aligned =
      (first + alignment - 1) & ~static_cast<std::uintptr_t>(alignment - 1);
  auto *recorded = reinterpret_cast<header *>(aligned) - 1;
  ::new (static_cast<void *>(recorded)) header{start, total};
  return reinterpret_cast<void *>(aligned);
}

// Frees the bytes of an object allocate_aligned allocated.
template <typename Bytes>
void deallocate_aligned(Bytes &bytes, void *object) noexcept {
  auto *recorded = static_cast<detail::allocation_header *>(object) - 1;
  std::allocator_traits<Bytes>::deallocate(bytes, recorded->bytes,
                                           recorded->size);
}

// Destroys an object of a type derived from `Base` that allocate_aligned
// allocated, and frees its bytes; `release` knows its type.
template <typename Base, typename Bytes> struct allocated_deleter {
  Bytes bytes;
  void (*release)(Base *, Bytes &) noexcept;

  void operator()(Base *object) noexcept { release(object, bytes); }
};

// Constructs an object of `Type` in bytes of `bytes`, owned by a pointer to
// `Base` that frees them.
template <typename Type, typename Base, typename Bytes, typename... Args>
std::unique_ptr<Base, allocated_deleter<Base, Bytes>>
allocate_object(Bytes &bytes, Args &&...args) {
  void *memory = allocate_aligned(bytes, sizeof(Type), alignof(Type));
  Type *object = nullptr;
  try {
    object = ::new (memory) Type(std::forward<Args>(args)...);
  } catch (...) {
    deallocate_aligned(bytes, memory);
    throw;
  }
  auto release = [](Base *base, Bytes &from) noexcept {
    Type *derived = static_cast<Type *>(base);
    derived->~Type();
    deallocate_aligned(from, derived);
  };
  return std::unique_ptr<Base, allocated_deleter<Base, Bytes>>(
      object, allocated_deleter<Base, Bytes>{bytes, release});
}

} // namespace dingo::core
