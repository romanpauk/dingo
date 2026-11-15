//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Support for the generated model probes, of no container and no scope:
// whether the library accepts a registration, how a probe reads and checks
// what a request resolves, and the outcome it records. A program includes the
// container and the scopes of its cells itself, and model/static.h for a
// static container, so that it reads only the headers it uses.

#include "model/leaves.h"

#include <dingo/base/exceptions.h>
#include <dingo/binding/model.h>
#include <dingo/factory/auto_constructible.h>
#include <dingo/factory/constructor.h>
#include <dingo/lookup/lookup.h>
#include <dingo/registration/type_registration.h>
#include <dingo/request/decomposition.h>
#include <dingo/type/annotated.h>

#include <cstdio>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>

namespace dingo::model {

// The traits of a container with a lookup by key for each of Interfaces.
// The keyed lookups of interfaces, one for each interface: an array and its
// element are two.
template <typename Domains, typename... Interfaces> struct keyed_lookups;

template <typename... Domains>
struct keyed_lookups<dingo::type_list<Domains...>> {
  using type = dingo::lookups<dingo::associative<std::size_t, Domains>...>;
};

template <typename... Domains, typename Interface, typename... Interfaces>
struct keyed_lookups<dingo::type_list<Domains...>, Interface, Interfaces...>
    : std::conditional_t<
          (std::is_same_v<Domains, std::remove_cv_t<Interface>> || ...),
          keyed_lookups<dingo::type_list<Domains...>, Interfaces...>,
          keyed_lookups<
              dingo::type_list<Domains..., std::remove_cv_t<Interface>>,
              Interfaces...>> {};

template <typename Base, typename... Interfaces> struct keyed_traits : Base {
  using lookup_definition_type =
      typename keyed_lookups<dingo::type_list<>, Interfaces...>::type;
};

// Whether a static container constructs the storage and the factory of a
// binding of a model itself; model/static.h tells.
template <typename Model> struct statically_constructible;

// A container with a binding more: a static container binds it in its type,
// which binds_in_type is true of; a runtime container is the same, and
// registers it once it is made. model/static.h tells of a static container.
template <typename Container, typename Binding> struct with_binding {
  using type = Container;
  static constexpr bool binds_in_type = false;
};

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
    return statically_constructible<model>::value;
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
  } else if constexpr (std::is_same_v<T, other>) {
    return nullptr;
  } else if constexpr (std::is_array_v<T>) {
    // The first element stands for the array when every element is a leaf
    // of the value registered.
    for (const auto &element : result) {
      const leaf_interface *found = leaf_of(element);
      if (!found || found->value() != 7) {
        return nullptr;
      }
    }
    return leaf_of(result[0]);
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

// The name of a type as the compiler spells it.
template <typename T> std::string_view type_name() {
  std::string_view text = __PRETTY_FUNCTION__;
  const auto begin = text.find("T = ") + 4;
  auto end = text.find(';', begin);
  if (end == std::string_view::npos) {
    end = text.size() - 1;
  }
  return text.substr(begin, end - begin);
}

template <typename T, typename... Types>
inline constexpr bool is_one_of_v = (std::is_same_v<T, Types> || ...);

// Whether the interfaces a registration binds, annotated or not, are the
// expected ones, in any order.
template <typename... Expected, typename... Bound>
constexpr bool binds_exactly(type_list<Bound...> *) {
  return sizeof...(Bound) == sizeof...(Expected) &&
         (is_one_of_v<unannotated_t<Bound>, Expected...> && ...);
}

// Prints whether a registration the library accepts binds exactly the
// interfaces the specification expects.
template <typename Registration, typename... Expected>
void survey(const char *registration) {
  using model = detail::binding_model<Registration>;
  if constexpr (model::storage_tag_is_complete && model::stored &&
                model::valid) {
    std::printf("{\"registration\": \"%s\", \"binds\": %s}\n", registration,
                binds_exactly<Expected...>(
                    static_cast<typename model::interface_types *>(nullptr))
                    ? "true"
                    : "false");
  }
}

// A request selects the binding of its normalized type, or of its exact type.
template <typename Request, typename Interface>
inline constexpr bool selects_v =
    std::is_same_v<typename request_type<Request>::value_type, Interface> ||
    std::is_same_v<typename request_type<Request>::exact_type, Interface>;

// How the scope of a cell keeps what it resolves.
enum class keeps {
  // the scope keeps its instance: an address request refers to it
  instance,
  // the scope keeps its instance and the container constructs what it does
  // not refer to: a reference or a raw pointer refers to the instance, a
  // handle or a value is new
  addresses,
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

// A dependency or a parameter as the receiver passes it on: a reference as
// itself, a value moved, also when it is const.
template <typename R, typename Arg> decltype(auto) pass_on(Arg &arg) {
  if constexpr (std::is_reference_v<R>) {
    return static_cast<R>(arg);
  } else {
    return std::move(const_cast<std::remove_const_t<R> &>(arg));
  }
}

// The request a dependency or a parameter carries, without its annotation or
// selector. A selector of a reference converts to an lvalue of it.
template <typename U, typename Arg> decltype(auto) unwrap(Arg &&arg) {
  if constexpr (std::is_same_v<dingo::detail::remove_cvref_t<Arg>,
                               dingo::detail::remove_cvref_t<U>>) {
    return std::forward<Arg>(arg);
  } else if constexpr (std::is_reference_v<U>) {
    return static_cast<U>(static_cast<std::remove_reference_t<U> &>(arg));
  } else {
    return static_cast<std::remove_cv_t<U>>(arg);
  }
}

// What a dependency or a callable received as R: the leaf it reached and its
// value while it ran, and what it holds by value, which outlives the call. A
// value it cannot take, such as a const move-only one, is only read. An
// annotated or keyed rvalue reference is unwrapped first: it converts to both
// an lvalue and an rvalue, so a value cannot be constructed from it directly.
template <typename R> class received {
public:
  using result_type = dingo::unannotated_t<R>;
  using held_type = std::remove_cv_t<std::remove_reference_t<result_type>>;
  using passed_type = decltype(pass_on<R>(std::declval<R &>()));
  static constexpr bool unwraps =
      std::is_rvalue_reference_v<result_type> &&
      !std::is_constructible_v<held_type, passed_type>;
  static constexpr bool holds =
      !std::is_lvalue_reference_v<result_type> &&
      !std::is_pointer_v<result_type> &&
      std::is_constructible_v<
          held_type, std::conditional_t<unwraps, result_type, passed_type>>;
  static constexpr bool refers = refers_v<result_type>;
  static constexpr bool hands_over = std::is_pointer_v<result_type>;

  // What the library does to resolve R ends where it receives it.
  template <typename Arg, typename = std::enable_if_t<!std::is_same_v<
                              dingo::detail::remove_cvref_t<Arg>, received>>>
  explicit received(Arg &&arg) {
    leaf_interface::counting = false;
    if constexpr (holds && unwraps) {
      value_.emplace(unwrap<result_type>(arg));
      address_ = leaf_of(*value_);
    } else if constexpr (holds) {
      value_.emplace(std::forward<Arg>(arg));
      address_ = leaf_of(*value_);
    } else if constexpr (std::is_pointer_v<result_type>) {
      result_type pointer = unwrap<result_type>(std::forward<Arg>(arg));
      null_ = pointer == nullptr;
      address_ = leaf_of(pointer);
    } else {
      address_ = leaf_of(
          static_cast<const std::remove_reference_t<result_type> &>(arg));
    }
    seen_ = address_ ? address_->value() : 0;
  }

  const leaf_interface *leaf() const {
    if constexpr (holds) {
      return leaf_of(*value_);
    } else {
      return address_;
    }
  }
  int seen() const { return seen_; }
  // Whether it is a null pointer.
  bool is_null() const { return null_; }
  // Whether what it refers to is gone.
  bool dangles() const {
    if constexpr (holds) {
      return false;
    } else {
      return address_ && !leaf_interface::is_alive(address_);
    }
  }

private:
  std::conditional_t<holds, std::optional<held_type>, std::nullptr_t> value_{};
  const leaf_interface *address_ = nullptr;
  int seen_ = 0;
  bool null_ = false;
};

// A class that takes R as the dependency of its constructor. A container
// constructs it unregistered, as a user's class it may auto-construct, or
// registered in a scope that keeps it.
template <typename R> struct consumer {
  using dingo_constructor_type = constructor<consumer(R)>;
  explicit consumer(R arg) : got(pass_on<R>(arg)) {}
  received<R> got;
};
} // namespace dingo::model

template <typename R>
struct dingo::is_auto_constructible<dingo::model::consumer<R>>
    : std::true_type {};

namespace dingo::model {

// The registration of a consumer of R in Scope.
template <typename R, typename Scope>
using consumer_registration =
    type_registration<scope<Scope>, storage<consumer<R>>,
                      factory<constructor<consumer<R>(R)>>>;

// The container a consumer of R in Scope is resolved from: a static container
// binds it too, a runtime one registers it.
template <typename Container, typename R, typename Scope>
using with_consumer = with_binding<Container, consumer_registration<R, Scope>>;

template <typename R, typename Scope, typename Container>
void register_consumer(Container &c) {
  if constexpr (with_binding<Container,
                             consumer_registration<R, Scope>>::binds_in_type) {
    (void)c;
  } else {
    c.template register_type<scope<Scope>, storage<consumer<R>>,
                             factory<constructor<consumer<R>(R)>>>();
  }
}

// How a probe reads a result: the leaf it reaches, the value of the leaf,
// whether what it refers to is gone, whether it refers to an instance a scope
// keeps, whether it keeps what it reached, whether it hands a raw pointer to
// the caller, and whether it is kept by its scope, so that it has to keep what
// it refers to.
template <typename R> struct observation {
  using result_type = std::remove_reference_t<R>;
  static const leaf_interface *leaf(const result_type &result) {
    return leaf_of(result);
  }
  static int value(const result_type &result) {
    const leaf_interface *found = leaf_of(result);
    return found ? found->value() : 0;
  }
  static bool dangles(const result_type &) { return false; }
  static bool is_null(const result_type &result) {
    if constexpr (std::is_pointer_v<result_type>) {
      return result == nullptr;
    } else {
      return false;
    }
  }
  static constexpr bool refers = refers_v<R>;
  static constexpr bool keeps_leaf = true;
  static constexpr bool hands_over = std::is_pointer_v<R>;
  static constexpr bool kept = false;
};

template <typename R> struct received_observation {
  static const leaf_interface *leaf(const received<R> &got) {
    return got.leaf();
  }
  static int value(const received<R> &got) { return got.seen(); }
  static bool dangles(const received<R> &got) { return got.dangles(); }
  static bool is_null(const received<R> &got) { return got.is_null(); }
  static constexpr bool refers = received<R>::refers;
  static constexpr bool keeps_leaf = received<R>::holds;
  static constexpr bool hands_over = received<R>::hands_over;
  static constexpr bool kept = false;
};

template <typename R>
struct observation<received<R>> : received_observation<R> {};

template <typename R> struct observation<consumer<R>> {
  static const leaf_interface *leaf(const consumer<R> &c) {
    return c.got.leaf();
  }
  static int value(const consumer<R> &c) { return c.got.seen(); }
  static bool dangles(const consumer<R> &c) { return c.got.dangles(); }
  static bool is_null(const consumer<R> &c) { return c.got.is_null(); }
  static constexpr bool refers = received<R>::refers;
  static constexpr bool keeps_leaf = received<R>::holds;
  static constexpr bool hands_over = received<R>::hands_over;
  static constexpr bool kept = false;
};

// A consumer its scope keeps is constructed once, so its two requests receive
// one dependency; what it refers to has to live as long as it does.
template <typename R>
struct observation<consumer<R> &> : observation<consumer<R>> {
  static constexpr bool refers = false;
  static constexpr bool keeps_leaf = false;
  static constexpr bool kept = true;
};

// The request a result is of: of a dependency a callable or a class
// received, the dependency, without its annotation.
template <typename R> struct requested_of {
  using type = dingo::unannotated_t<R>;
};
template <typename R> struct requested_of<received<R>> : requested_of<R> {};
template <typename R> struct requested_of<consumer<R>> : requested_of<R> {};
template <typename R> struct requested_of<consumer<R> &> : requested_of<R> {};

// Whether a result is of a pointer to the other alternative of an
// alternative type, which the instance of a probe never holds.
template <typename R>
inline constexpr bool requests_other_alternative_v =
    std::is_same_v<
        std::remove_cv_t<std::remove_pointer_t<typename requested_of<R>::type>>,
        other> &&
    std::is_pointer_v<typename requested_of<R>::type>;

// What is wrong with two results of one request, or null. A leaf that was not
// kept may be gone, so it is not compared. A pointer to the other alternative
// of an alternative type that holds the leaf is null.
template <typename R, typename First, typename Second>
const char *violation(keeps scope, const First &first, const Second &second) {
  using read = observation<R>;
  if constexpr (requests_other_alternative_v<R>) {
    (void)scope;
    return read::is_null(first) && read::is_null(second)
               ? nullptr
               : "a pointer to the alternative the instance does not hold is "
                 "not null";
  }
  const leaf_interface *a = read::leaf(first);
  const leaf_interface *b = read::leaf(second);
  if (!a || !b || read::value(first) != 7 || read::value(second) != 7) {
    return "the result does not reach the registered leaf";
  }
  if ((scope != keeps::nothing || read::kept) &&
      (read::dangles(first) || read::dangles(second))) {
    return "the result refers to a leaf that is gone";
  }
  constexpr bool address =
      std::is_lvalue_reference_v<R> || std::is_pointer_v<R>;
  if (((scope == keeps::instance && read::refers) ||
       (scope == keeps::addresses && address)) &&
      a != b) {
    return "two requests of a kept instance refer to different leaves";
  }
  if (scope == keeps::nothing && read::keeps_leaf && a == b) {
    return "two requests of a unique scope reach one leaf";
  }
  return nullptr;
}
// The copies and the moves of leaves of each of the two resolutions of a
// probe.
struct counts {
  long copies[2];
  long moves[2];
};

// Counts the copies and the moves of leaves a resolution makes into one of
// counts, while the library resolves it.
template <typename Resolve>
decltype(auto) counted(int index, counts &into, Resolve &&resolve) {
  leaf_interface::copies = leaf_interface::moves = 0;
  leaf_interface::counting = true;
  struct stop {
    int index;
    counts &into;
    ~stop() {
      leaf_interface::counting = false;
      into.copies[index] = leaf_interface::copies;
      into.moves[index] = leaf_interface::moves;
    }
  } stopping{index, into};
  return resolve();
}

// Records a resolved probe with the copies and the moves of its resolutions.
inline void record_resolved(const char *cell, const counts &made) {
  std::printf("{\"cell\": \"");
  print_json_text(cell);
  std::printf("\", \"outcome\": \"resolved\", \"copies\": "
              "[%ld, %ld], \"moves\": [%ld, %ld]}\n",
              made.copies[0], made.copies[1], made.moves[0], made.moves[1]);
}

// Runs the resolutions of a probe and records its outcome: what is wrong
// with them, or the exception they threw. A raw pointer resolved from a scope
// that keeps nothing hands the leaf to the caller, so it is not counted as
// leaked. It is not a template: only what resolves depends on the request.
inline void run_probe(const char *cell, keeps scope, bool hands_over,
                      const char *(*resolutions)(const void *, keeps, counts &),
                      const void *state) {
  const long live = leaf_interface::live();
  const char *error = nullptr;
  counts made{};
  try {
    error = resolutions(state, scope, made);
    if (!error && leaf_interface::live() != live &&
        !(scope == keeps::nothing && hands_over)) {
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
  if (error) {
    return record(cell, "violated", error);
  }
  record_resolved(cell, made);
}

// Resolves a request twice in one container, which is destroyed before the
// leaves alive are counted, and checks the results.
template <typename Container, typename Setup, typename Resolve>
void probe(const char *cell, keeps scope, Setup setup, Resolve resolve) {
  using R = decltype(resolve(std::declval<Container &>()));
  struct calls {
    Setup setup;
    Resolve resolve;
  } state{setup, resolve};
  run_probe(
      cell, scope, observation<R>::hands_over,
      [](const void *state, keeps scope, counts &into) -> const char * {
        const calls &made = *static_cast<const calls *>(state);
        Container c;
        made.setup(c);
        R first = counted(0, into, [&]() -> R { return made.resolve(c); });
        R second = counted(1, into, [&]() -> R { return made.resolve(c); });
        return violation<R>(scope, first, second);
      },
      &state);
}

} // namespace dingo::model
