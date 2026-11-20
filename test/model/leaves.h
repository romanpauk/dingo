//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The objects of the generated programs, of no container and no scope: the
// leaf types the cells store, the instances made of them, the shapes a
// request is spelled in, and the outcome a program records for a cell.

#include <dingo/registration/constructor.h>
#include <dingo/type/type_traits.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>

namespace dingo::model {

// Keeps the leaves alive, so that a probe detects what it leaks and what it
// refers to after it is gone. A leaf moved from loses its value, so that a
// probe detects a kept leaf moved out of. Counts the copies and the moves of
// leaves while a probe counts them.
struct leaf_interface {
  leaf_interface() { born(this); }
  leaf_interface(const leaf_interface &other) : state(other.state) {
    copied();
    born(this);
  }
  leaf_interface(leaf_interface &&other) noexcept : state(other.state) {
    moved();
    other.state = 0;
    born(this);
  }
  leaf_interface &operator=(const leaf_interface &other) {
    copied();
    state = other.state;
    return *this;
  }
  leaf_interface &operator=(leaf_interface &&other) noexcept {
    moved();
    state = other.state;
    other.state = 0;
    return *this;
  }
  virtual ~leaf_interface() { died(this); }
  virtual int value() const = 0;

  static long live() { return static_cast<long>(alive_count); }
  static bool is_alive(const leaf_interface *leaf) {
    for (std::size_t index = 0; index < alive_count; ++index) {
      if (alive[index] == leaf) {
        return true;
      }
    }
    return false;
  }

  // The leaves alive, which a program holds few of: more is a defect of the
  // library, which the program stops at.
  static void born(const leaf_interface *leaf) {
    if (alive_count == alive_capacity) {
      std::abort();
    }
    alive[alive_count++] = leaf;
  }
  static void died(const leaf_interface *leaf) {
    for (std::size_t index = 0; index < alive_count; ++index) {
      if (alive[index] == leaf) {
        alive[index] = alive[--alive_count];
        return;
      }
    }
  }

  static constexpr std::size_t alive_capacity = 4096;
  inline static const leaf_interface *alive[alive_capacity] = {};
  inline static std::size_t alive_count = 0;
  inline static bool counting = false;
  inline static long copies = 0;
  inline static long moves = 0;

protected:
  static void copied() { copies += counting ? 1 : 0; }
  static void moved() { moves += counting ? 1 : 0; }

  int state = 7;
};

// The object every cell stores, directly or inside its stored type. The leaves
// a default factory makes declare their constructors, so that a cell does not
// detect them: detection is checked by the detection probes, of classes of
// their own.
struct leaf : leaf_interface {
  DINGO_CONSTRUCTOR(leaf()) = default;
  int value() const override { return state; }
};

// A leaf that can be moved and not copied.
struct move_only_leaf : leaf_interface {
  DINGO_CONSTRUCTOR(move_only_leaf()) = default;
  move_only_leaf(move_only_leaf &&) = default;
  move_only_leaf &operator=(move_only_leaf &&) = default;
  move_only_leaf(const move_only_leaf &) = delete;
  move_only_leaf &operator=(const move_only_leaf &) = delete;
  int value() const override { return state; }
};

// A leaf that can be neither copied nor moved.
struct immovable_leaf : leaf_interface {
  DINGO_CONSTRUCTOR(immovable_leaf()) = default;
  immovable_leaf(const immovable_leaf &) = delete;
  immovable_leaf &operator=(const immovable_leaf &) = delete;
  int value() const override { return state; }
};

// What a factory makes a factory leaf of.
struct made {};

// A leaf without a default constructor: only its factory makes it.
struct factory_leaf : leaf_interface {
  explicit factory_leaf(made) {}
  int value() const override { return state; }
};

// The arguments a leaf is constructed with: none, or what its factory makes it
// of. A stored type around a leaf is constructed from them in place.
template <typename T> struct leaf_arguments {
  template <typename Fn> static decltype(auto) apply(Fn &&fn) { return fn(); }
};

template <> struct leaf_arguments<factory_leaf> {
  template <typename Fn> static decltype(auto) apply(Fn &&fn) {
    return fn(made{});
  }
};

// The leaf of a stored type, without its arrays, handles and wrappers.
template <typename T, typename = void> struct leaf_type {
  using type = std::remove_cv_t<std::remove_all_extents_t<T>>;
};

// A T made in place of the arguments of its leaf.
template <typename T> T made_value() {
  return leaf_arguments<typename leaf_type<T>::type>::apply(
      [](auto... args) { return T(args...); });
}

// The second alternative of a stored variant.
struct other {};

// The tag of an annotated binding.
struct tag {};

template <typename T> inline constexpr bool is_optional_v = false;
template <typename T>
inline constexpr bool is_optional_v<std::optional<T>> = true;

// An instance of a stored type, for the scopes that are given one.
template <typename Stored> struct instance {
  static Stored make() {
    if constexpr (std::is_pointer_v<Stored>) {
      static std::remove_pointer_t<Stored> object =
          made_value<std::remove_pointer_t<Stored>>();
      return &object;
    } else if constexpr (std::is_array_v<Stored>) {
      static_assert(!std::is_array_v<Stored>, "an array is given by reference");
    } else if constexpr (is_alternative_type_v<Stored>) {
      using leaf = std::variant_alternative_t<0, Stored>;
      return leaf_arguments<leaf>::apply(
          [](auto... args) { return Stored(std::in_place_index<0>, args...); });
    } else {
      return made_value<Stored>();
    }
  }
};

template <typename T> struct instance<std::shared_ptr<T>> {
  static std::shared_ptr<T> make() {
    using element = std::remove_extent_t<T>;
    if constexpr (is_optional_v<T>) {
      return leaf_arguments<typename T::value_type>::apply([](auto... args) {
        return std::make_shared<T>(std::in_place, args...);
      });
    } else if constexpr (dingo::type_traits<T>::enabled) {
      return std::make_shared<T>(instance<T>::make());
    } else if constexpr (std::is_array_v<T>) {
      return leaf_arguments<element>::apply([](auto... args) {
        return std::shared_ptr<T>(
            new element[2]{element(args...), element(args...)});
      });
    } else {
      return leaf_arguments<T>::apply(
          [](auto... args) { return std::make_shared<T>(args...); });
    }
  }
};

template <typename T> struct instance<std::unique_ptr<T>> {
  static std::unique_ptr<T> make() {
    using element = std::remove_extent_t<T>;
    if constexpr (is_optional_v<T>) {
      return leaf_arguments<typename T::value_type>::apply([](auto... args) {
        return std::make_unique<T>(std::in_place, args...);
      });
    } else if constexpr (dingo::type_traits<T>::enabled) {
      return std::make_unique<T>(instance<T>::make());
    } else if constexpr (std::is_array_v<T>) {
      return leaf_arguments<element>::apply([](auto... args) {
        return std::unique_ptr<T>(
            new element[2]{element(args...), element(args...)});
      });
    } else {
      return leaf_arguments<T>::apply(
          [](auto... args) { return std::make_unique<T>(args...); });
    }
  }
};

template <typename T> struct instance<std::optional<T>> {
  static std::optional<T> make() {
    if constexpr (is_optional_v<T>) {
      return leaf_arguments<typename T::value_type>::apply([](auto... args) {
        return std::optional<T>(std::in_place, std::in_place, args...);
      });
    } else if constexpr (dingo::type_traits<T>::enabled) {
      return std::optional<T>(instance<T>::make());
    } else {
      return leaf_arguments<T>::apply([](auto... args) {
        return std::optional<T>(std::in_place, args...);
      });
    }
  }
};

// A new instance of a stored type, for the factory of a registration whose
// stored type has no default value: a pointer is to a new object, which the
// caller owns.
template <typename Stored> struct fresh : instance<Stored> {};

template <typename T> struct fresh<T *> {
  static T *make() {
    return leaf_arguments<T>::apply(
        [](auto... args) { return new T(args...); });
  }
};

// An object an external scope refers to.
template <typename T> T &external_object() {
  if constexpr (std::is_array_v<T>) {
    using element = std::remove_extent_t<T>;
    static_assert(std::extent_v<T> == 2, "a probe array has two rows");
    static T object = {made_value<element>(), made_value<element>()};
    return object;
  } else {
    static T object = made_value<T>();
    return object;
  }
}

// The shapes the probe requests a bound interface X in: a value, a reference
// or a pointer of X or of a handle of it. Alias templates spell them, so that
// an array interface is spelled as C++ spells it.
namespace shape {
template <typename X> using value = X;
template <typename X> using constant = const X;
template <typename X> using lref = X &;
template <typename X> using clref = const X &;
template <typename X> using rref = X &&;
template <typename X> using ptr = X *;
template <typename X> using cptr = const X *;
template <typename X> using sp = std::shared_ptr<X>;
template <typename X> using up = std::unique_ptr<X>;
template <typename X> using opt = std::optional<X>;
template <typename X> using sp_array = std::shared_ptr<X[]>;
template <typename X> using up_array = std::unique_ptr<X[]>;
template <typename X> using sp_bounded_array = std::shared_ptr<X[2]>;
} // namespace shape

inline void record(const char *prefix, std::string_view name,
                   const char *outcome, const char *error);
inline void record(const char *cell, const char *outcome, const char *error);

// A text as the inside of a JSON string: its quotes, backslashes and control
// characters escaped.
inline void print_json_text(std::string_view text) {
  for (char c : text) {
    if (c == '"' || c == '\\') {
      std::printf("\\%c", c);
    } else if (static_cast<unsigned char>(c) < 0x20) {
      std::printf("\\u%04x",
                  static_cast<unsigned>(static_cast<unsigned char>(c)));
    } else {
      std::putchar(c);
    }
  }
}

// One line of the outcome table: the cell, of a prefix and a name as the
// callers compile them in, and its outcome.
inline void record(const char *prefix, std::string_view name,
                   const char *outcome, const char *error) {
  std::printf("{\"cell\": \"");
  print_json_text(prefix);
  print_json_text(name);
  std::printf("\", \"outcome\": \"");
  print_json_text(outcome);
  std::printf("\"");
  if (error) {
    std::printf(", \"error\": \"");
    print_json_text(error);
    std::printf("\"");
  }
  std::printf("}\n");
}

inline void record(const char *cell, const char *outcome, const char *error) {
  record(cell, std::string_view(), outcome, error);
}

inline void record(const char *cell, const char *outcome) {
  record(cell, outcome, nullptr);
}

} // namespace dingo::model
