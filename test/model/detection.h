//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Support for the generated constructor detection probes: the classes whose
// constructor the library detects, generated around a parameter type P, and
// the records of what detection says and what a construction does.

#include "model/probe.h"

#include <dingo/factory/constructor_detection.h>

#include <stdexcept>
#include <string>

namespace dingo::model {

// A class with a constructor that takes P.
template <typename P> struct single {
  single(P arg) : got(pass_on<P>(arg)) {}
  received<P> got;
};

// The same constructor, explicit.
template <typename P> struct explicit_single {
  explicit explicit_single(P arg) : got(pass_on<P>(arg)) {}
  received<P> got;
};

// An aggregate of one P, initialized by the detected argument.
template <typename P> struct aggregate {
  P got;
};

// A class with a constructor of two P.
template <typename P> struct pair {
  pair(P first_arg, P second_arg)
      : first(pass_on<P>(first_arg)), second(pass_on<P>(second_arg)) {}
  received<P> first;
  received<P> second;
};

// A parameter as a function declares it: without the top-level const of a
// value.
template <typename P>
using parameter_t =
    std::conditional_t<std::is_reference_v<P>, P, std::remove_const_t<P>>;

// The parameters the constructor of each kind takes.
template <typename Class> struct parameters_of;
template <template <typename> class Kind, typename P>
struct parameters_of<Kind<P>> {
  using type = type_list<parameter_t<P>>;
};
template <typename P> struct parameters_of<pair<P>> {
  using type = type_list<parameter_t<P>, parameter_t<P>>;
};

// Whether a constructed class reached the registered leaf and read its value
// while it was constructed.
template <typename R> bool reached(const received<R> &got) {
  return got.leaf() && got.seen() == 7;
}

template <typename Class> struct reading;
template <typename P> struct reading<single<P>> {
  static bool reached(single<P> &c) { return model::reached(c.got); }
};
template <typename P> struct reading<explicit_single<P>> {
  static bool reached(explicit_single<P> &c) { return model::reached(c.got); }
};
// An aggregate cannot read its member while it is initialized, so it is read
// after. A reference, an rvalue one too, or a pointer may then refer to an
// instance its resolution made for the construction, which is gone; only a
// member it holds by value is read.
template <typename P> struct reading<aggregate<P>> {
  static bool reached(aggregate<P> &c) {
    if constexpr (received<P>::holds &&
                  !std::is_reference_v<typename received<P>::result_type>) {
      return model::reached(received<P>(pass_on<P>(c.got)));
    } else {
      (void)c;
      return true;
    }
  }
};
template <typename P> struct reading<pair<P>> {
  static bool reached(pair<P> &c) {
    return model::reached(c.first) && model::reached(c.second);
  }
};

// What detection says about Class in Mode, against the parameters of the
// constructor the generator wrote: detected, or misdetected with what it
// found.
template <typename Class, typename Mode>
void record_detection(const char *cell) {
  using detection = constructor_detection<Class, Mode>;
  using expected = typename parameters_of<Class>::type;
  std::string found;
  if constexpr (detection::kind == detail::constructor_kind::concrete) {
    found = "concrete";
  } else if constexpr (detection::kind == detail::constructor_kind::generic) {
    found = "generic";
  } else {
    found = "invalid";
  }
  found += ", arity " + std::to_string(detection::arity);
  bool right = detection::kind == detail::constructor_kind::concrete &&
               detection::arity == type_list_size_v<expected>;
  if constexpr (std::is_same_v<Mode, detail::constructor_signature>) {
    using arguments = typename detection::arguments;
    found += ", arguments " + std::string(type_name<arguments>());
    right = right && std::is_same_v<arguments, expected>;
  }
  record(cell, right ? "detected" : "misdetected",
         right ? nullptr : found.c_str());
}

// The container a class is constructed in: a static container binds what a
// probe registers, a runtime one registers it.
template <typename Container, typename Binding> struct with_binding {
  using type = Container;
};

template <typename... Bindings, typename Traits, typename Binding>
struct with_binding<static_container<bindings<Bindings...>, Traits>, Binding> {
  using type = static_container<bindings<Bindings..., Binding>, Traits>;
};

template <typename Binding> struct registers;
template <typename... Args> struct registers<type_registration<Args...>> {
  template <typename Container> static void in(Container &c) {
    if constexpr (!is_instance<Container, static_container>::value) {
      c.template register_type<Args...>();
    } else {
      (void)c;
    }
  }
};

// Makes a class in a new container, and records whether it reached the
// registered leaf, or the exception the construction threw.
template <typename Container, typename Setup, typename Make>
void construct_probe(const char *cell, Setup setup, Make make) {
  try {
    Container c;
    setup(c);
    if (!make(c)) {
      throw std::logic_error("the class does not reach the registered leaf");
    }
  } catch (const type_not_found_exception &) {
    return record(cell, "type_not_found");
  } catch (const type_not_convertible_exception &) {
    return record(cell, "type_not_convertible");
  } catch (const type_ambiguous_exception &) {
    return record(cell, "type_ambiguous");
  } catch (const std::logic_error &e) {
    return record(cell, "violated", e.what());
  } catch (const std::exception &e) {
    return record(cell, "exception", e.what());
  }
  record(cell, "resolved");
}

} // namespace dingo::model

template <typename P>
struct dingo::is_auto_constructible<dingo::model::single<P>> : std::true_type {
};
template <typename P>
struct dingo::is_auto_constructible<dingo::model::explicit_single<P>>
    : std::true_type {};
template <typename P>
struct dingo::is_auto_constructible<dingo::model::pair<P>> : std::true_type {};
