//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/memory/object_lifetime.h>

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {
// The source of a resolution: a value it owns, an lvalue it borrows, or a
// pointer it borrows. Each yields its instance with get() and a reference to
// it with *source; an owned value is moved out of an rvalue source.
template <typename Source> class rvalue_source {
public:
  using value_type = Source;

  // The source exists once the builder returns; a builder that throws leaves
  // nothing to destroy.
  template <typename Builder>
  explicit rvalue_source(std::in_place_t, Builder &&builder) {
    builder(&storage_);
  }

  rvalue_source(const rvalue_source &) = delete;
  rvalue_source &operator=(const rvalue_source &) = delete;
  rvalue_source(rvalue_source &&) = delete;
  rvalue_source &operator=(rvalue_source &&) = delete;

  ~rvalue_source() { destroy_object_value(get()); }

  Source &get() & { return *pointer(); }
  const Source &get() const & { return *pointer(); }
  Source get() && { return std::move(*pointer()); }

  Source &operator*() { return get(); }
  const Source &operator*() const { return get(); }

private:
  Source *pointer() const {
    return std::launder(reinterpret_cast<Source *>(&storage_));
  }

  alignas(Source) mutable std::byte storage_[sizeof(Source)];
};

// An array a scope made for a request, which the request owns: deleted when
// the request ends, unless a step took it over, as handing it over or a
// handle taking it over does.
template <typename Element> class made_array {
public:
  explicit made_array(Element *array) : array_(array) {}
  made_array(made_array &&other) noexcept : array_(other.array_) {
    other.array_ = nullptr;
  }
  made_array(const made_array &) = delete;
  made_array &operator=(const made_array &) = delete;
  made_array &operator=(made_array &&) = delete;
  ~made_array() { delete[] array_; }

  Element *get() const { return array_; }

  // Gives the array up to the step that took it over.
  static void release(void *owner) {
    static_cast<made_array *>(owner)->array_ = nullptr;
  }

private:
  Element *array_;
};

template <typename T> inline constexpr bool is_made_array_v = false;
template <typename Element>
inline constexpr bool is_made_array_v<made_array<Element>> = true;

template <typename Source> struct lvalue_source {
  using value_type = Source;

  explicit lvalue_source(Source &source) : value_(std::addressof(source)) {}

  Source &get() const { return *value_; }
  Source &operator*() const { return *value_; }

private:
  Source *value_;
};

template <typename Source> struct pointer_source {
  using value_type = Source;

  explicit pointer_source(Source *source) : value_(source) {}

  Source *get() const { return value_; }
  Source &operator*() const { return *value_; }

private:
  Source *value_;
};

template <typename Source> inline constexpr bool is_pointer_source_v = false;

template <typename Source>
inline constexpr bool is_pointer_source_v<pointer_source<Source>> = true;

template <typename Source, typename Builder>
rvalue_source<Source> make_rvalue_source(std::in_place_t, Builder &&builder) {
  return rvalue_source<Source>(std::in_place, std::forward<Builder>(builder));
}

template <typename Source>
lvalue_source<Source> make_resolved_source(Source &source) {
  return lvalue_source<Source>(source);
}

template <typename Source>
pointer_source<Source> make_resolved_source(Source *source) {
  return pointer_source<Source>(source);
}
} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
