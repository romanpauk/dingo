//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Support for the generated model probes: the leaf types the cells store, the
// instances an external scope is given, whether the library accepts a
// registration, and the outcome a probe records for a request, with the
// checks of a resolved one.

#include <dingo/container.h>
#include <dingo/static/binding.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <cstdio>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

namespace dingo::model {

// Counts the leaves alive, so that a probe sees what it leaks.
struct leaf_interface {
  leaf_interface() { ++live; }
  leaf_interface(const leaf_interface &) { ++live; }
  leaf_interface(leaf_interface &&) noexcept { ++live; }
  leaf_interface &operator=(const leaf_interface &) = default;
  leaf_interface &operator=(leaf_interface &&) = default;
  virtual ~leaf_interface() { --live; }
  virtual int value() const = 0;

  inline static long live = 0;
};

// The object every cell stores, directly or inside its stored type.
struct leaf : leaf_interface {
  int value() const override { return 7; }
};

// A leaf that can be moved and not copied.
struct move_only_leaf : leaf_interface {
  move_only_leaf() = default;
  move_only_leaf(move_only_leaf &&) = default;
  move_only_leaf &operator=(move_only_leaf &&) = default;
  move_only_leaf(const move_only_leaf &) = delete;
  move_only_leaf &operator=(const move_only_leaf &) = delete;
  int value() const override { return 7; }
};

// The second alternative of a stored variant.
struct other {};

// The tag of an annotated binding.
struct tag {};

// The traits of a container with a lookup by key for T.
template <typename T, typename Base> struct keyed_traits : Base {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, T>>;
};

// An instance of a stored type, for the scopes that are given one.
template <typename Stored> struct instance {
  static Stored make() {
    if constexpr (std::is_pointer_v<Stored>) {
      static std::remove_pointer_t<Stored> object{};
      return &object;
    } else if constexpr (std::is_array_v<Stored>) {
      static_assert(!std::is_array_v<Stored>, "an array is given by reference");
    } else if constexpr (is_alternative_type_v<Stored>) {
      return Stored(std::in_place_index<0>);
    } else {
      return Stored{};
    }
  }
};

template <typename T> struct instance<std::shared_ptr<T>> {
  static std::shared_ptr<T> make() {
    if constexpr (std::is_array_v<T>) {
      return std::shared_ptr<T>(new std::remove_extent_t<T>[2]);
    } else {
      return std::make_shared<T>();
    }
  }
};

template <typename T> struct instance<std::unique_ptr<T>> {
  static std::unique_ptr<T> make() {
    if constexpr (std::is_array_v<T>) {
      return std::unique_ptr<T>(new std::remove_extent_t<T>[2]);
    } else {
      return std::make_unique<T>();
    }
  }
};

template <typename T> struct instance<std::optional<T>> {
  static std::optional<T> make() { return std::optional<T>(std::in_place); }
};

// An object an external scope refers to.
template <typename T> T &external_object() {
  static T object{};
  return object;
}

// Whether the library accepts registering Registration. A registration it
// rejects does not compile, so the probe asks before registering.
// A static container also constructs the storage and the factory of a
// binding itself.
template <typename Registration, bool Static> constexpr bool registrable() {
  using model = detail::binding_model<Registration>;
  if constexpr (!model::storage_tag_is_complete) {
    return false;
  } else if constexpr (!model::valid) {
    return false;
  } else if constexpr (Static) {
    return detail::binding_storage_is_default_constructible<model>::value &&
           detail::binding_factory_is_default_constructible<model>::value;
  } else {
    return true;
  }
}

template <typename T, template <typename...> class Template>
struct is_instance : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_instance<Template<Args...>, Template> : std::true_type {};

// The leaf a result refers to or holds, or null.
template <typename T> const leaf_interface *leaf_of(const T &result) {
  if constexpr (std::is_base_of_v<leaf_interface, T>) {
    return &result;
  } else if constexpr (std::is_pointer_v<T>) {
    return result ? leaf_of(*result) : nullptr;
  } else if constexpr (is_instance<T, std::shared_ptr>::value ||
                       is_instance<T, std::unique_ptr>::value) {
    return result ? leaf_of(*result.get()) : nullptr;
  } else if constexpr (is_instance<T, std::optional>::value) {
    return result ? leaf_of(*result) : nullptr;
  } else if constexpr (is_instance<T, std::variant>::value) {
    auto *alternative = std::get_if<0>(&result);
    return alternative ? leaf_of(*alternative) : nullptr;
  } else {
    static_assert(!sizeof(T *), "a result the probe does not read");
    return nullptr;
  }
}

// One line of the outcome table: the cell and its outcome.
inline void record(const char *cell, const char *outcome,
                   const char *error = nullptr) {
  if (error) {
    std::printf("{\"cell\": \"%s\", \"outcome\": \"%s\", \"error\": "
                "\"%s\"}\n",
                cell, outcome, error);
  } else {
    std::printf("{\"cell\": \"%s\", \"outcome\": \"%s\"}\n", cell, outcome);
  }
}

// How the scope of a cell keeps what it resolves.
enum class keeps {
  // the scope keeps its instance: an address request refers to it
  instance,
  // every resolution makes another instance
  nothing,
};

// Whether a result refers to the instance of its binding rather than holding
// a copy of it.
template <typename R>
inline constexpr bool refers_v =
    std::is_lvalue_reference_v<R> || std::is_pointer_v<R> ||
    is_instance<std::remove_cv_t<std::remove_reference_t<R>>,
                std::shared_ptr>::value;

// What is wrong with two results of one request, or null.
template <typename R, typename First, typename Second>
const char *violation(keeps scope, const First &first, const Second &second) {
  const leaf_interface *a = leaf_of(first);
  const leaf_interface *b = leaf_of(second);
  if (!a || !b || a->value() != 7 || b->value() != 7) {
    return "the result does not reach the registered leaf";
  }
  if (scope == keeps::instance && refers_v<R> && a != b) {
    return "two requests of a kept instance refer to different leaves";
  }
  if (scope == keeps::nothing && a == b) {
    return "two requests of a unique scope reach one leaf";
  }
  return nullptr;
}

// Resolves a request twice in one container and checks the results. A raw
// pointer resolved from a scope that keeps nothing hands the leaf to the
// caller, so it is not counted as leaked.
template <typename Container, typename R, typename Setup, typename Resolve>
void probe(const char *cell, keeps scope, Setup setup, Resolve resolve) {
  const long live = leaf_interface::live;
  const char *error = nullptr;
  try {
    {
      Container c;
      setup(c);
      R first = resolve(c);
      R second = resolve(c);
      error = violation<R>(scope, first, second);
    }
    if (!error && leaf_interface::live != live &&
        !(scope == keeps::nothing && std::is_pointer_v<R>)) {
      error = "the leaves alive differ after the container is destroyed";
    }
  } catch (const type_not_found_exception &) {
    return record(cell, "type_not_found");
  } catch (const type_not_convertible_exception &) {
    return record(cell, "type_not_convertible");
  } catch (const type_ambiguous_exception &) {
    return record(cell, "type_ambiguous");
  } catch (const std::exception &e) {
    return record(cell, "exception", e.what());
  }
  record(cell, error ? "violated" : "resolved", error);
}

} // namespace dingo::model
