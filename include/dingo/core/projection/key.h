//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The keys of bindings and requests: no key, a tag type, a value fixed when
// the program is compiled, or a value supplied at runtime, and the keyed
// request a constructor parameter declares.

#include <dingo/core/primitives/shape.h>
#include <dingo/core/primitives/type_name.h>
#include <dingo/core/vocabulary/compile_diagnostics.gen.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <string>
#include <type_traits>
#include <utility>

namespace dingo {

struct none_t;

// key_type<Tag> is a tag key, key_type<Key, Value> a fixed key of the value
// Key{Value}, and key_type<none_t> no key.
template <typename Type, auto... Values> struct key_type {
  static_assert(core::require<sizeof...(Values) <= 1,
                              core::diag::key_type_values, Type>());
  using type = Type;
};

// A key value supplied at runtime.
template <typename Type, typename Interface = void> class key_value {
public:
  using type = Type;

  template <typename Value, typename = std::enable_if_t<
                                std::is_constructible_v<Type, Value &&>>>
  explicit key_value(Value &&value) : value_(std::forward<Value>(value)) {}

  const Type &value() const { return value_; }

private:
  Type value_;
};

template <typename Type> key_value(Type &&) -> key_value<std::decay_t<Type>>;

} // namespace dingo

namespace dingo::core {

// The domain of a key: its kind and the type of its tag or value. The value
// of a key is compared by the backend, which holds it.
struct key_descriptor {
  key_kind kind = key_kind::none;
  type_id type = nullptr;
};

// Whether a key carries a value, which a request compares after the domain.
constexpr bool key_has_value(const key_descriptor &key) {
  return key.kind == key_kind::fixed || key.kind == key_kind::runtime;
}

struct no_key {
  static constexpr key_descriptor descriptor{};
};

template <typename Tag> struct tag_key {
  static constexpr key_descriptor descriptor{key_kind::tag, type_id_v<Tag>};
};

template <typename Key, auto Value> struct fixed_key {
  using value_type = Key;
  static constexpr key_descriptor descriptor{key_kind::fixed, type_id_v<Key>};
  static constexpr Key value() {
    if constexpr (std::is_same_v<std::remove_cv_t<decltype(Value)>, Key>) {
      return Value;
    } else {
      return Key{Value};
    }
  }
};

template <typename Key> struct runtime_key {
  using value_type = Key;
  static constexpr key_descriptor descriptor{key_kind::runtime, type_id_v<Key>};
};

// Whether `value`, a key value, is 0 up to `bound`, excluded: the index of a
// dense array of `bound` entries. A value of a type that is neither integral
// nor an enumeration indexes none.
template <typename Value>
constexpr bool key_within(const Value &value, std::size_t bound) {
  if constexpr (std::is_enum_v<Value>) {
    return key_within(static_cast<std::underlying_type_t<Value>>(value), bound);
  } else if constexpr (std::is_same_v<Value, bool>) {
    return static_cast<std::size_t>(value) < bound;
  } else if constexpr (std::is_integral_v<Value>) {
    if constexpr (std::is_signed_v<Value>) {
      if (value < 0) {
        return false;
      }
    }
    return static_cast<std::make_unsigned_t<Value>>(value) < bound;
  } else {
    return false;
  }
}

// The value a key holds, by address for comparison: nothing for a key without
// a value, the constant of a fixed key, or the value of a runtime key.
template <typename Key> class key_store {
public:
  const void *value() const { return nullptr; }

  static bool equal(const void * /*left*/, const void * /*right*/) {
    return true;
  }

  static bool within(const void * /*value*/, std::size_t /*bound*/) {
    return true;
  }
};

template <typename Key, auto Value> class key_store<fixed_key<Key, Value>> {
public:
  const void *value() const { return &constant; }

  static bool equal(const void *left, const void *right) {
    return *static_cast<const Key *>(left) == *static_cast<const Key *>(right);
  }

  static bool within(const void *value, std::size_t bound) {
    return key_within(*static_cast<const Key *>(value), bound);
  }

  static constexpr Key constant = fixed_key<Key, Value>::value();
};

template <typename Key> class key_store<runtime_key<Key>> {
public:
  explicit key_store(const Key &value) : value_(value) {}

  const void *value() const { return &value_; }

  static bool equal(const void *left, const void *right) {
    return *static_cast<const Key *>(left) == *static_cast<const Key *>(right);
  }

  static bool within(const void *value, std::size_t bound) {
    return key_within(*static_cast<const Key *>(value), bound);
  }

private:
  Key value_;
};

namespace detail {
template <typename Key> struct key_type_of {
  using type = ::dingo::none_t;
};

template <typename Tag> struct key_type_of<tag_key<Tag>> {
  using type = Tag;
};

// A fixed key is named by its key_type, as the baseline names it.
template <typename Key, auto Value> struct key_type_of<fixed_key<Key, Value>> {
  using type = ::dingo::key_type<Key, Value>;
};

template <typename Key> struct key_type_of<runtime_key<Key>> {
  using type = Key;
};
} // namespace detail

// The type diagnostics name for a key: its tag or value type, or none_t.
template <typename Key> std::string key_type_name() {
  return type_name_of<typename detail::key_type_of<Key>::type>();
}

// The key a key_type declares.
template <typename KeyType> struct key_of;

template <> struct key_of<::dingo::key_type<::dingo::none_t>> {
  using type = no_key;
};

template <typename Tag> struct key_of<::dingo::key_type<Tag>> {
  using type = tag_key<Tag>;
};

template <typename Key, auto Value>
struct key_of<::dingo::key_type<Key, Value>> {
  using type = fixed_key<Key, Value>;
};

template <typename Key, auto First, auto Second, auto... Values>
struct key_of<::dingo::key_type<Key, First, Second, Values...>> {
  static_assert(
      require<sizeof...(Values) + 2 <= 1, diag::key_type_values, Key>());
  using type = no_key;
};

template <typename KeyType> using key_of_t = typename key_of<KeyType>::type;

template <typename Type> struct is_key_type : std::false_type {};

template <typename Type, auto... Values>
struct is_key_type<::dingo::key_type<Type, Values...>> : std::true_type {};

// The interface a key value names the key of, void for every interface.
template <typename Type> struct key_value_interface {
  using type = void;
};

template <typename Type, typename Interface>
struct key_value_interface<::dingo::key_value<Type, Interface>> {
  using type = Interface;
};

template <typename Type> struct is_key_value : std::false_type {};

template <typename Type, typename Interface>
struct is_key_value<::dingo::key_value<Type, Interface>> : std::true_type {};

// The runtime key of a key value: a key_value, or a value of the key type.
template <typename Value> struct runtime_key_of {
  using type = runtime_key<std::decay_t<Value>>;
  static const std::decay_t<Value> &value(const std::decay_t<Value> &value) {
    return value;
  }
};

template <typename Type, typename Interface>
struct runtime_key_of<::dingo::key_value<Type, Interface>> {
  using type = runtime_key<Type>;
  static const Type &value(const ::dingo::key_value<Type, Interface> &value) {
    return value.value();
  }
};

// The key of a request argument: a key_type object names its key, and a
// key_value or a value of the key type is a runtime key. `value` is the
// address of the value of the key, or nullptr.
template <typename Argument, bool = is_key_type<Argument>::value>
struct request_key_of {
  using type = typename runtime_key_of<Argument>::type;
  static const void *value(const Argument &argument) {
    return &runtime_key_of<Argument>::value(argument);
  }
};

template <typename Argument> struct request_key_of<Argument, true> {
  using type = key_of_t<Argument>;
  static const void *value(const Argument & /*argument*/) {
    return key_store<type>().value();
  }
};

// A request of `Type` by the key `Key`, which converts to the object it
// resolved.
template <typename Type, typename Key> class keyed {
public:
  using type = Type;
  using key = Key;

  // A value that cannot be moved is copied.
  template <
      typename Value = Type,
      std::enable_if_t<std::is_constructible_v<Value, const Value &>, int> = 0>
  explicit keyed(const Type &value) : value_(value) {}

  template <typename Value = Type,
            std::enable_if_t<std::is_constructible_v<Value, Value &&>, int> = 0>
  explicit keyed(Type &&value) : value_(std::move(value)) {}

  operator Type() && {
    if constexpr (std::is_move_constructible_v<Type>) {
      return std::move(value_);
    } else {
      return value_;
    }
  }
  operator Type() const & { return value_; }

private:
  Type value_;
};

template <typename Type, typename Key> class keyed<Type &, Key> {
public:
  using type = Type &;
  using key = Key;

  explicit keyed(Type &value) : value_(&value) {}

  operator Type &() const { return *value_; }

private:
  Type *value_;
};

template <typename Type> struct is_keyed : std::false_type {};

template <typename Type, typename Key>
struct is_keyed<keyed<Type, Key>> : std::true_type {};

} // namespace dingo::core
