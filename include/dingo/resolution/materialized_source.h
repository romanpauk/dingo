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
// The storage and lifetime halves of an owning source. The lifetime half only
// registers a destructor when the value actually needs one, so a source of a
// trivially destructible value is itself trivially destructible and costs no
// context destructor slot.
template <typename Source> class rvalue_source_storage {
public:
  using value_type = Source;

  Source *get_ptr() {
    return std::launder(reinterpret_cast<Source *>(&storage_));
  }
  const Source *get_ptr() const {
    return std::launder(reinterpret_cast<const Source *>(&storage_));
  }

  Source &get() & { return *get_ptr(); }
  const Source &get() const & { return *get_ptr(); }
  Source get() && { return std::move(*get_ptr()); }

  Source &operator*() { return get(); }
  const Source &operator*() const { return get(); }

  Source *operator->() { return get_ptr(); }
  const Source *operator->() const { return get_ptr(); }

protected:
  ~rvalue_source_storage() = default;

  void *storage_address() { return &storage_; }
  void mark_initialized() { initialized_ = true; }
  bool is_initialized() const { return initialized_; }

private:
  alignas(Source) std::byte storage_[sizeof(Source)];
  bool initialized_ = false;
};

template <typename Source, bool Trivial> class rvalue_source_lifetime;

template <typename Source>
class rvalue_source_lifetime<Source, true>
    : public rvalue_source_storage<Source> {
protected:
  ~rvalue_source_lifetime() = default;
};

template <typename Source>
class rvalue_source_lifetime<Source, false>
    : public rvalue_source_storage<Source> {
protected:
  ~rvalue_source_lifetime() {
    if (this->is_initialized()) {
      destroy_object_value(this->get());
    }
  }
};

template <typename Source>
class rvalue_source
    : public rvalue_source_lifetime<Source,
                                    std::is_trivially_destructible_v<Source>> {
  using lifetime =
      rvalue_source_lifetime<Source, std::is_trivially_destructible_v<Source>>;

public:
  using value_type = Source;

  template <
      typename Value,
      typename = std::enable_if_t<
          std::is_constructible_v<Source, Value &&> &&
          !std::is_same_v<std::remove_cv_t<std::remove_reference_t<Value>>,
                          rvalue_source>>>
  explicit rvalue_source(Value &&value) {
    new (this->storage_address()) Source(std::forward<Value>(value));
    this->mark_initialized();
  }

  template <typename Builder>
  explicit rvalue_source(std::in_place_t, Builder &&builder) {
    builder(this->storage_address());
    this->mark_initialized();
  }

  rvalue_source(const rvalue_source &) = delete;
  rvalue_source &operator=(const rvalue_source &) = delete;
  rvalue_source(rvalue_source &&) = delete;
  rvalue_source &operator=(rvalue_source &&) = delete;

  using lifetime::get;
  using lifetime::get_ptr;
  using lifetime::operator*;
  using lifetime::operator->;
};

// A source that owns its value for as long as the resolution context lives.
// Routes can publish an address into it instead of copying the value into a
// second context slot.
template <typename Source>
class context_rvalue_source : public rvalue_source<Source> {
public:
  using rvalue_source<Source>::rvalue_source;
};

template <typename Source> struct is_context_rvalue_source : std::false_type {};

template <typename Value>
struct is_context_rvalue_source<context_rvalue_source<Value>> : std::true_type {
};

template <typename Source>
inline constexpr bool is_context_rvalue_source_v = is_context_rvalue_source<
    std::remove_cv_t<std::remove_reference_t<Source>>>::value;

template <typename Source> struct lvalue_source {
  using value_type = Source;

  explicit lvalue_source(Source &source) : value_(std::addressof(source)) {}

  Source &get() { return *value_; }
  const Source &get() const { return *value_; }

  Source *get_ptr() { return value_; }
  const Source *get_ptr() const { return value_; }

  Source &operator*() { return get(); }
  const Source &operator*() const { return get(); }

  Source *operator->() { return get_ptr(); }
  const Source *operator->() const { return get_ptr(); }

private:
  Source *value_;
};

template <typename Source> struct pointer_source {
  using value_type = Source;

  explicit pointer_source(Source *source) : value_(source) {}

  Source *get() { return value_; }
  const Source *get() const { return value_; }

  Source &operator*() { return *value_; }
  const Source &operator*() const { return *value_; }

  Source *operator->() { return value_; }
  const Source *operator->() const { return value_; }

private:
  Source *value_;
};

template <typename MaterializedSource> struct materialized_source_traits;

template <typename Source>
struct materialized_source_traits<rvalue_source<Source>> {
  using value_type = Source;
  static constexpr bool reference_like = false;

  template <typename ActualSource>
  static decltype(auto) value(ActualSource &&source) {
    return std::forward<ActualSource>(source).get();
  }

  static Source &reference(rvalue_source<Source> &source) {
    return source.get();
  }

  static const Source &reference(const rvalue_source<Source> &source) {
    return source.get();
  }

  static Source *pointer(rvalue_source<Source> &source) {
    return source.get_ptr();
  }

  static const Source *pointer(const rvalue_source<Source> &source) {
    return source.get_ptr();
  }
};

template <typename Source>
struct materialized_source_traits<context_rvalue_source<Source>>
    : materialized_source_traits<rvalue_source<Source>> {};

template <typename Source>
struct materialized_source_traits<lvalue_source<Source>> {
  using value_type = Source;
  static constexpr bool reference_like = true;

  template <typename ActualSource>
  static decltype(auto) value(ActualSource &&source) {
    return std::forward<ActualSource>(source).get();
  }

  static Source &reference(lvalue_source<Source> &source) {
    return source.get();
  }

  static const Source &reference(const lvalue_source<Source> &source) {
    return source.get();
  }

  static Source *pointer(lvalue_source<Source> &source) {
    return source.get_ptr();
  }

  static const Source *pointer(const lvalue_source<Source> &source) {
    return source.get_ptr();
  }
};

template <typename Source>
struct materialized_source_traits<pointer_source<Source>> {
  using value_type = Source;
  static constexpr bool reference_like = true;

  template <typename ActualSource>
  static decltype(auto) value(ActualSource &&source) {
    return std::forward<ActualSource>(source).get();
  }

  static Source &reference(pointer_source<Source> &source) {
    return *source.get();
  }

  static const Source &reference(const pointer_source<Source> &source) {
    return *source.get();
  }

  static Source *pointer(pointer_source<Source> &source) {
    return source.get();
  }

  static const Source *pointer(const pointer_source<Source> &source) {
    return source.get();
  }
};

template <typename Source, typename Builder>
rvalue_source<Source> make_rvalue_source(std::in_place_t, Builder &&builder) {
  return rvalue_source<Source>(std::in_place, std::forward<Builder>(builder));
}

template <typename Source>
auto make_rvalue_source(Source &&source)
    -> rvalue_source<std::remove_cv_t<std::remove_reference_t<Source>>> {
  using value_type = std::remove_cv_t<std::remove_reference_t<Source>>;
  return rvalue_source<value_type>(std::forward<Source>(source));
}

template <typename Source>
lvalue_source<Source> make_lvalue_source(Source &source) {
  return lvalue_source<Source>(source);
}

template <typename Source>
pointer_source<Source> make_pointer_source(Source *source) {
  return pointer_source<Source>(source);
}

template <typename Source>
lvalue_source<Source> make_resolved_source(Source &source) {
  return make_lvalue_source(source);
}

template <typename Source>
pointer_source<Source> make_resolved_source(Source *source) {
  return make_pointer_source(source);
}

} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
