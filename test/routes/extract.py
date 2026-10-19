#!/usr/bin/env python3
"""Extracts the conversion ("route") behavior of the current implementation.

For every combination of domain, leaf kind, scope, stored shape, and request the
extractor records what the library does: whether the registration compiles,
whether the resolution compiles, throws, or succeeds, and for a successful
resolution the constructions it performed and the identity of the leaf it
handed out. The table is ground truth for a reference implementation of the
route rules; it describes the code as it is, not as it is documented.

Usage:

    extract.py --work-dir build/routes [--jobs N] [--cxx-command CMD]
               [--include-dir DIR] [--domains runtime,static,mixed]
               [--output FILE]

    CXX=g++ extract.py --work-dir build/routes
    extract.py --work-dir build/routes \\
        --include-dir build/differential/baseline-source/include

The table describes the library whose headers are in --include-dir, include/
by default; the differential workflow extracts it from the pinned baseline.
The default output is <work-dir>/observed/<toolchain>.jsonl, where the
toolchain key is the one test/contracts/probe.py uses, such as
gcc16-x86_64-c++20. The table is not committed: it is regenerated from the
headers. The file is JSON Lines sorted by case id; every line is one case.

Universe (the tables below):

- domains: "runtime" is dingo::container<> with register_type, "static" is
  dingo::static_container over a bindings list, "mixed" is dingo::container
  over a bindings list (static binding, resolved through the container).
- leaf kinds: "leaf" (copyable, movable) and "move_only_leaf"; both count
  default, copy, and move constructions and destructions and register every
  live instance address with a creation sequence number. "impl" is a third,
  instrumented the same way, that derives from the polymorphic interfaces
  "iface" and "other_iface"; it is registered in two variants, with
  dingo::interfaces<iface> and with dingo::interfaces<iface, other_iface>.
  The addresses of its interface subobjects are registered as aliases of the
  instance, so an interface reference reaches the same identity.
- scopes: shared, unique, external, shared_cyclical. External registrations
  pass an instance; the static and mixed containers cannot take one, which
  shows as a registration compile error.
- stored shapes: T, T*, T&, unique_ptr/shared_ptr of T and T[], optional<T>,
  T[2], variant<T, other_leaf>, and the nine {unique_ptr, shared_ptr, optional}
  of {unique_ptr<T>, shared_ptr<T>, optional<T>} combinations. The leaf and
  move_only_leaf kinds have 21 more: the user wrappers user_unique<T>,
  user_shared<T>, user_optional<T> (class templates defined in the prelude and
  made known to dingo through type_traits, storage_traits, and
  type_conversion_traits specializations, as test/support/custom_wrappers.h
  does; their storage_traits are written like the built-in cells of the
  matching std handle, so a wrapped wrapper is published as well, and no
  storage_traits exist for shared_cyclical), and the combinations user_X<std_Y<T>> and std_Y<user_X<T>> for X, Y in
  {unique, shared, optional}.
- requests: the carriers value, &, const &, &&, *, const * applied to T,
  unique_ptr<T>, shared_ptr<T>, optional<T>, the stored type, and every
  wrapper layer of the stored type, deduplicated. The spelling in the case id
  is the one written in the generated C++ with single spaces: "leaf&",
  "const leaf&", "leaf const*" only after a pointer ("leaf* const&"),
  "leaf(&)[2]" for arrays.

A case id is "<domain>/<leaf>/<scope>/<stored>/<request>"; for impl the leaf is
followed by its interface list in brackets, "impl[iface,other_iface]". A record
has the keys

  counts         {"copy", "default", "destroy", "move"} deltas during the first
                 resolve of an "ok" case, else null
  domain, leaf, scope, stored, request, id
  interfaces     the comma-separated interface list of the registration
                 ("iface", "iface,other_iface"), "" for leaf and
                 move_only_leaf
  identity       "existing" (the leaf was live before the call), "new" (created
                 during the call), "none" (empty handle), "unknown" (the
                 address is not live when the call returns: a dangling
                 result), or null for a plain leaf value and for a case that
                 is not "ok"
  registration   "ok", "compile_error", or "throws:<exception>"
  resolve        "ok", "compile_error", "throws:<exception>", "crash:<signal>",
                 or "not_run" when the registration is not "ok"
  stable         whether a second resolve reaches the same leaf address (false
                 for a plain leaf value, null when not "ok")
  warm_counts, warm_identity
                 the same observations for that second resolve, made while the
                 result of the first is still alive

Every case uses a fresh container: the first resolve is a cold resolve.
<exception> is the most specific dingo exception type caught (they are caught
in the order of include/dingo/base/exceptions.h; "exception" is the common
base), else "std" or "unknown".

Method: per (domain, leaf, scope, stored) one generated program contains one
function per request. A registration-only compile decides the registration
outcome; the requests that compile (all at once, else one -fsyntax-only compile
per request, which is the only way to observe a static compile error) are then
built into one executable per group that runs each request on a fresh container
and prints one JSON line. A crash is attributed to the request being run and
the remaining requests continue.
"""

import argparse
import concurrent.futures
import itertools
import json
import os
import platform
import re
import shlex
import signal
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]

# ---------------------------------------------------------------------------
# The universe.
# ---------------------------------------------------------------------------

DOMAINS = ("runtime", "static", "mixed")

# The registered kinds as (leaf, interfaces). The interfaces are the
# dingo::interfaces<...> of the registration; none for the plain leaves.
LEAF_KINDS = (
    ("leaf", ()),
    ("move_only_leaf", ()),
    ("impl", ("iface",)),
    ("impl", ("iface", "other_iface")),
)

# Leaf kinds that also have the user wrapper shapes.
USER_WRAPPER_LEAVES = ("leaf", "move_only_leaf")

SCOPES = ("shared", "unique", "external", "shared_cyclical")

# Carriers applied to a base type. The spelling is built by carrier_spellings.
CARRIERS = ("value", "&", "const &", "&&", "*", "const *")

# The dingo exception types, most derived first. All are siblings derived from
# dingo::exception; arena_allocation_exception exists in debug builds only.
EXCEPTIONS = (
    "type_not_found_exception",
    "type_not_convertible_exception",
    "type_ambiguous_exception",
    "type_recursion_exception",
    "lookup_already_registered_exception",
    "type_index_out_of_range_exception",
    "virtual_pointer_exception",
    "arena_allocation_exception",
)


@dataclass(frozen=True)
class Shape:
    """A stored shape. {T} is the leaf; the external expression builds the
    instance an external registration is given; declarations are objects the
    probe owns for it."""

    name: str
    spelling: str
    external: str
    layers: tuple = ()
    declarations: tuple = ()
    # Registered without an instance in a scope that constructs.
    external_note: str = ""
    # A user wrapper shape: requested through the user wrappers of T.
    user: bool = False


# The wrapper families: the short name, the spelling of the wrapper of {inner},
# and the expression building one around the expression {arg} of an {inner}
# (empty for a default constructed inner leaf).
WRAPPERS = {
    "std::unique_ptr": (
        "unique_ptr",
        "std::unique_ptr<{inner}>",
        lambda inner, arg: f"std::make_unique<{inner}>({arg})",
    ),
    "std::shared_ptr": (
        "shared_ptr",
        "std::shared_ptr<{inner}>",
        lambda inner, arg: f"std::make_shared<{inner}>({arg})",
    ),
    "std::optional": (
        "optional",
        "std::optional<{inner}>",
        lambda inner, arg: f"std::optional<{inner}>(std::in_place"
        + (f", {arg})" if arg else ")"),
    ),
    "user_unique": (
        "user_unique",
        "user_unique<{inner}>",
        lambda inner, arg: f"user_unique<{inner}>(std::make_unique<{inner}>({arg}))",
    ),
    "user_shared": (
        "user_shared",
        "user_shared<{inner}>",
        lambda inner, arg: f"user_shared<{inner}>(std::make_shared<{inner}>({arg}))",
    ),
    "user_optional": (
        "user_optional",
        "user_optional<{inner}>",
        lambda inner, arg: f"user_optional<{inner}>(std::in_place"
        + (f", {arg})" if arg else ")"),
    ),
}
STD_WRAPPERS = ("std::unique_ptr", "std::shared_ptr", "std::optional")
USER_WRAPPERS = ("user_unique", "user_shared", "user_optional")


def _wrapper_shapes(outers, inners, user):
    """The shapes outer<inner<T>> for every outer and inner wrapper."""
    for outer, inner in itertools.product(outers, inners):
        o_name, o_spelling, o_make = WRAPPERS[outer]
        i_name, i_spelling, i_make = WRAPPERS[inner]
        inner_type = i_spelling.format(inner="{T}")
        inner_expression = i_make("{T}", "")
        yield Shape(
            name=f"{o_name}_of_{i_name}",
            spelling=o_spelling.format(inner=inner_type),
            external=o_make(inner_type, inner_expression),
            layers=(inner_type,),
            user=user,
        )


def _single_wrapper_shapes(wrappers, user):
    for wrapper in wrappers:
        name, spelling, make = WRAPPERS[wrapper]
        yield Shape(
            name=name,
            spelling=spelling.format(inner="{T}"),
            external=make("{T}", ""),
            user=user,
        )


SHAPES = (
    Shape("value", "{T}", "{T}{{}}"),
    Shape("pointer", "{T}*", "&owned", declarations=("{T} owned;",)),
    Shape("reference", "{T}&", "owned", declarations=("{T} owned;",)),
    Shape("unique_ptr", "std::unique_ptr<{T}>", "std::make_unique<{T}>()"),
    Shape("shared_ptr", "std::shared_ptr<{T}>", "std::make_shared<{T}>()"),
    Shape("optional", "std::optional<{T}>", "std::optional<{T}>(std::in_place)"),
    Shape("array", "{T}[2]", "owned", declarations=("{T} owned[2];",)),
    Shape("unique_ptr_array", "std::unique_ptr<{T}[]>", "std::make_unique<{T}[]>(2)"),
    Shape(
        "shared_ptr_array",
        "std::shared_ptr<{T}[]>",
        "std::shared_ptr<{T}[]>(new {T}[2])",
    ),
    Shape(
        "variant",
        "std::variant<{T}, other_leaf>",
        "std::variant<{T}, other_leaf>(std::in_place_index<0>)",
    ),
    *_wrapper_shapes(STD_WRAPPERS, STD_WRAPPERS, user=False),
)

# The user wrappers of T and their combinations with the std wrappers, both
# ways round.
USER_SHAPES = (
    *_single_wrapper_shapes(USER_WRAPPERS, user=True),
    *_wrapper_shapes(USER_WRAPPERS, STD_WRAPPERS, user=True),
    *_wrapper_shapes(STD_WRAPPERS, USER_WRAPPERS, user=True),
)


def spell(template, leaf):
    return template.replace("{T}", leaf).replace("{{}}", "{}")


def stored_spelling(shape, leaf):
    return spell(shape.spelling, leaf)


ARRAY_RE = re.compile(r"^(.*)\[(\d+)\]$")


def carrier_spellings(base, abstract=False):
    """Valid spellings of the carriers applied to a base type, as
    (carrier, spelling) pairs. Invalid compositions are skipped: an abstract
    class has no value."""
    array = ARRAY_RE.match(base)
    if array:
        element, size = array.groups()
        return [
            ("&", f"{element}(&)[{size}]"),
            ("const &", f"const {element}(&)[{size}]"),
            ("&&", f"{element}(&&)[{size}]"),
            ("*", f"{element}(*)[{size}]"),
            ("const *", f"const {element}(*)[{size}]"),
        ]
    if base.endswith("&"):
        return [("value", base)]
    const = (lambda b: f"{b} const") if base.endswith("*") else (lambda b: f"const {b}")
    spellings = [
        ("value", base),
        ("&", f"{base}&"),
        ("const &", f"{const(base)}&"),
        ("&&", f"{base}&&"),
        ("*", f"{base}*"),
        ("const *", f"{const(base)}*"),
    ]
    return spellings[1:] if abstract else spellings


def request_bases(shape, leaf, interfaces):
    """The types the carriers are applied to, as (base, abstract) pairs."""
    stored = stored_spelling(shape, leaf)
    layers = [spell(layer, leaf) for layer in shape.layers]
    if interfaces:
        return [
            *((i, True) for i in interfaces),
            *((f"std::unique_ptr<{i}>", False) for i in interfaces),
            *((f"std::shared_ptr<{i}>", False) for i in interfaces),
            (leaf, False),
            (stored, False),
            *((layer, False) for layer in layers),
        ]
    if shape.user:
        return [
            (leaf, False),
            *((f"{wrapper}<{leaf}>", False) for wrapper in USER_WRAPPERS),
            (stored, False),
            *((layer, False) for layer in layers),
        ]
    return [
        (leaf, False),
        (f"std::unique_ptr<{leaf}>", False),
        (f"std::shared_ptr<{leaf}>", False),
        (f"std::optional<{leaf}>", False),
        (stored, False),
        *((layer, False) for layer in layers),
    ]


def requests_for(shape, leaf, interfaces=()):
    """The deduplicated request spellings for a (leaf, stored shape)."""
    requests = []
    for base, abstract in request_bases(shape, leaf, interfaces):
        for _, request in carrier_spellings(base, abstract):
            if request not in requests:
                requests.append(request)
    return requests


def leaf_label(leaf, interfaces=()):
    """The leaf as written in a case id: impl[iface,other_iface]."""
    return f"{leaf}[{','.join(interfaces)}]" if interfaces else leaf


def case_id(domain, leaf, scope, stored, request, interfaces=()):
    return f"{domain}/{leaf_label(leaf, interfaces)}/{scope}/{stored}/{request}"


# ---------------------------------------------------------------------------
# Generated C++.
# ---------------------------------------------------------------------------

PRELUDE = r"""#pragma once
#include <dingo/base/exceptions.h>
#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/storage/unique.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_traits.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace probe {
struct state {
  unsigned long def = 0, copy = 0, move = 0, dtor = 0, seq = 0;
  std::map<const void *, unsigned long> live;
};
inline state &st() {
  static state &s = *new state;
  return s;
}
inline void created(const void *p) { st().live[p] = ++st().seq; }
// An address that reaches the same instance as p, such as an interface subobject.
inline void alias(const void *a, const void *p) { st().live[a] = st().live[p]; }
inline void destroyed(const void *p) {
  st().live.erase(p);
  ++st().dtor;
}
inline void unalias(const void *a) { st().live.erase(a); }
} // namespace probe

struct other_leaf {
  other_leaf() {}
};

struct leaf {
  leaf() { probe::created(this); ++probe::st().def; }
  leaf(const leaf &) { probe::created(this); ++probe::st().copy; }
  leaf(leaf &&) noexcept { probe::created(this); ++probe::st().move; }
  leaf &operator=(const leaf &) = default;
  leaf &operator=(leaf &&) = default;
  ~leaf() { probe::destroyed(this); }
};

struct move_only_leaf {
  move_only_leaf() { probe::created(this); ++probe::st().def; }
  move_only_leaf(const move_only_leaf &) = delete;
  move_only_leaf(move_only_leaf &&) noexcept { probe::created(this); ++probe::st().move; }
  move_only_leaf &operator=(const move_only_leaf &) = delete;
  move_only_leaf &operator=(move_only_leaf &&) = default;
  ~move_only_leaf() { probe::destroyed(this); }
};

// Two polymorphic interfaces and the instrumented implementation of both.
struct iface {
  virtual ~iface() = default;
  virtual int first() const = 0;
};

struct other_iface {
  virtual ~other_iface() = default;
  virtual int second() const = 0;
};

struct impl : iface, other_iface {
  impl() {
    probe::created(this);
    alias();
    ++probe::st().def;
  }
  impl(const impl &) : iface(), other_iface() {
    probe::created(this);
    alias();
    ++probe::st().copy;
  }
  impl(impl &&) noexcept : iface(), other_iface() {
    probe::created(this);
    alias();
    ++probe::st().move;
  }
  impl &operator=(const impl &) = default;
  impl &operator=(impl &&) = default;
  ~impl() override {
    probe::unalias(static_cast<other_iface *>(this));
    probe::destroyed(this);
  }
  int first() const override { return 1; }
  int second() const override { return 2; }

private:
  void alias() {
    probe::alias(static_cast<iface *>(this), this);
    probe::alias(static_cast<other_iface *>(this), this);
  }
};

// User wrappers, made known to dingo the way test/support/custom_wrappers.h
// does: a sole-owner handle, a sharing handle, and an optional-like value.
template <typename T> class user_unique;
template <typename T> class user_shared;
template <typename T> class user_optional;

template <typename T> class user_shared {
public:
  user_shared() = default;
  explicit user_shared(std::shared_ptr<T> ptr) : ptr_(std::move(ptr)) {}

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  user_shared(user_unique<U> &&other) : ptr_(std::move(other).native()) {}

  T *get() const { return ptr_.get(); }
  void reset() { ptr_.reset(); }
  explicit operator bool() const { return static_cast<bool>(ptr_); }
  const std::shared_ptr<T> &native() const & { return ptr_; }
  std::shared_ptr<T> &&native() && { return std::move(ptr_); }

private:
  std::shared_ptr<T> ptr_;
};

template <typename T> class user_unique {
public:
  user_unique() = default;
  explicit user_unique(std::unique_ptr<T> ptr) : ptr_(std::move(ptr)) {}

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U *, T *>>>
  user_unique(user_unique<U> &&other) : ptr_(std::move(other).native()) {}

  T *get() const { return ptr_.get(); }
  void reset() { ptr_.reset(); }
  explicit operator bool() const { return static_cast<bool>(ptr_); }
  std::unique_ptr<T> &&native() && { return std::move(ptr_); }

private:
  std::unique_ptr<T> ptr_;
};

template <typename T> class user_optional {
public:
  user_optional() = default;
  explicit user_optional(T value) : value_(std::move(value)) {}
  template <typename... Args>
  explicit user_optional(std::in_place_t, Args &&...args)
      : value_(std::in_place, std::forward<Args>(args)...) {}
  user_optional(const user_optional &) = default;
  user_optional(user_optional &&) = default;

  user_optional &operator=(const user_optional &other) {
    if (other.value_) {
      value_.emplace(*other.value_);
    } else {
      value_.reset();
    }
    return *this;
  }

  user_optional &operator=(user_optional &&other) {
    if (other.value_) {
      value_.emplace(std::move(*other.value_));
    } else {
      value_.reset();
    }
    return *this;
  }

  T *get() { return value_ ? std::addressof(*value_) : nullptr; }
  const T *get() const { return value_ ? std::addressof(*value_) : nullptr; }
  void reset() { value_.reset(); }
  bool has_value() const { return value_.has_value(); }

private:
  std::optional<T> value_;
};

namespace dingo {
namespace probe_detail {
template <typename T, template <typename...> class Template>
struct is_instance : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_instance<Template<Args...>, Template> : std::true_type {};
} // namespace probe_detail

template <typename T> struct type_traits<user_shared<T>> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = true;
  static constexpr bool is_owning_handle = true;

  template <typename> static constexpr bool is_handle_rebindable = true;

  using value_type = T;

  template <typename U> using rebind_t = user_shared<U>;

  template <typename Target>
  static constexpr bool is_rebindable =
      probe_detail::is_instance<std::remove_cv_t<Target>, user_shared>::value;

  static T *get(user_shared<T> &wrapper) { return wrapper.get(); }
  static const T *get(const user_shared<T> &wrapper) { return wrapper.get(); }
  static T &borrow(user_shared<T> &wrapper) { return *wrapper.get(); }
  static const T &borrow(const user_shared<T> &wrapper) {
    return *wrapper.get();
  }
  static bool empty(const user_shared<T> &wrapper) { return !wrapper.get(); }
  static void reset(user_shared<T> &wrapper) { wrapper.reset(); }

  // A wrapped wrapper is made through its own traits, as the std handles do;
  // a plain new T would leave it empty.
  template <typename... Args> static user_shared<T> make(Args &&...args) {
    if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
      return user_shared<T>(
          std::make_shared<T>(type_traits<T>::make(std::forward<Args>(args)...)));
    else if constexpr (std::is_constructible_v<T, Args...>)
      return user_shared<T>(std::make_shared<T>(std::forward<Args>(args)...));
    else
      return user_shared<T>(
          std::shared_ptr<T>(new T{std::forward<Args>(args)...}));
  }

  static user_shared<T> from_pointer(T *ptr) {
    return user_shared<T>(std::shared_ptr<T>(ptr));
  }

  template <typename TargetWrapper, typename Factory, typename Context>
  static TargetWrapper &resolve_type(Factory &factory, Context &context,
                                     type_descriptor, type_descriptor) {
    return factory.template resolve<TargetWrapper>(context);
  }
};

template <typename T> struct type_traits<user_unique<T>> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = true;
  static constexpr bool is_value_borrowable = true;
  static constexpr bool is_owning_handle = true;

  template <typename> static constexpr bool is_handle_rebindable = true;

  using value_type = T;

  template <typename U> using rebind_t = user_unique<U>;

  template <typename Target>
  static constexpr bool is_rebindable = std::is_same_v<Target, rebind_t<T>>;

  static T *get(user_unique<T> &wrapper) { return wrapper.get(); }
  static const T *get(const user_unique<T> &wrapper) { return wrapper.get(); }
  static T &borrow(user_unique<T> &wrapper) { return *wrapper.get(); }
  static const T &borrow(const user_unique<T> &wrapper) {
    return *wrapper.get();
  }
  static bool empty(const user_unique<T> &wrapper) { return !wrapper.get(); }
  static void reset(user_unique<T> &wrapper) { wrapper.reset(); }

  template <typename... Args> static user_unique<T> make(Args &&...args) {
    if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
      return user_unique<T>(std::unique_ptr<T>(
          new T(type_traits<T>::make(std::forward<Args>(args)...))));
    else if constexpr (std::is_constructible_v<T, Args...>)
      return user_unique<T>(
          std::unique_ptr<T>(new T(std::forward<Args>(args)...)));
    else
      return user_unique<T>(
          std::unique_ptr<T>(new T{std::forward<Args>(args)...}));
  }

  static user_unique<T> from_pointer(T *ptr) {
    return user_unique<T>(std::unique_ptr<T>(ptr));
  }

  template <typename TargetWrapper, typename Factory, typename Context>
  static TargetWrapper &resolve_type(Factory &factory, Context &context,
                                     type_descriptor requested_type,
                                     type_descriptor registered_type) {
    if constexpr (std::is_same_v<TargetWrapper, rebind_t<T>>) {
      return factory.resolve(context);
    } else {
      throw detail::make_type_not_convertible_exception(
          requested_type, registered_type, context);
    }
  }
};

template <typename T> struct type_traits<user_optional<T>> {
  static constexpr bool enabled = true;
  static constexpr bool is_pointer_like = false;
  static constexpr bool is_value_borrowable = true;
  static constexpr bool is_owning_handle = false;

  template <typename> static constexpr bool is_handle_rebindable = false;

  using value_type = T;

  template <typename U> using rebind_t = user_optional<U>;

  template <typename Target>
  static constexpr bool is_rebindable = std::is_same_v<Target, rebind_t<T>>;

  static T *get(user_optional<T> &wrapper) { return wrapper.get(); }
  static const T *get(const user_optional<T> &wrapper) { return wrapper.get(); }
  static T &borrow(user_optional<T> &wrapper) { return *wrapper.get(); }
  static const T &borrow(const user_optional<T> &wrapper) {
    return *wrapper.get();
  }
  static bool empty(const user_optional<T> &wrapper) {
    return !wrapper.has_value();
  }
  static void reset(user_optional<T> &wrapper) { wrapper.reset(); }

  template <typename Value> static user_optional<T> wrap(Value &&value) {
    return user_optional<T>(std::in_place, std::forward<Value>(value));
  }

  static user_optional<T> make_empty() { return {}; }

  template <typename... Args> static user_optional<T> make(Args &&...args) {
    if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>)
      return user_optional<T>(
          std::in_place, type_traits<T>::make(std::forward<Args>(args)...));
    else if constexpr (std::is_constructible_v<T, Args...>)
      return user_optional<T>(std::in_place, std::forward<Args>(args)...);
    else
      return user_optional<T>(std::in_place, T{std::forward<Args>(args)...});
  }

  template <typename TargetWrapper, typename Factory, typename Context>
  static TargetWrapper &resolve_type(Factory &factory, Context &context,
                                     type_descriptor requested_type,
                                     type_descriptor registered_type) {
    if constexpr (std::is_same_v<TargetWrapper, rebind_t<T>>) {
      return factory.resolve(context);
    } else {
      throw detail::make_type_not_convertible_exception(
          requested_type, registered_type, context);
    }
  }
};

// What each scope publishes for a user wrapper is written the way the built-in
// cell of the matching handle form is: user_shared like std::shared_ptr,
// user_unique like std::unique_ptr, user_optional like std::optional. The stored
// handle and the handles it wraps are published through rebind_leaf, so a
// wrapped wrapper is reachable as it is for the std handles.
namespace probe_detail {
template <typename Handle, typename U> struct handle_cell {
private:
  using rebound = detail::wrapper_rebind_leaf_t<Handle, U>;
  using types = detail::wrapper_storage_types<rebound>;

public:
  using rebound_type = rebound;
  using value_types = type_list_unique_t<
      type_list_cat_t<type_list<U>, typename types::copyable_value_types>>;
  using lvalue_reference_types = typename types::lvalue_reference_types;
  using pointer_types = typename types::pointer_types;
};
} // namespace probe_detail

template <typename T, typename U>
struct storage_traits<unique, user_shared<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = false;

  using value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using rvalue_reference_types =
      type_list<detail::wrapper_rebind_leaf_t<user_shared<T>, U> &&>;
  using pointer_types = type_list<>;
};

template <typename T, typename U>
struct storage_traits<shared, user_shared<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using cell = probe_detail::handle_cell<user_shared<T>, U>;
  using value_types = typename cell::value_types;
  using lvalue_reference_types = typename cell::lvalue_reference_types;
  using rvalue_reference_types = type_list<>;
  using pointer_types = typename cell::pointer_types;
};

template <typename T, typename U>
struct storage_traits<external, user_shared<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using cell = probe_detail::handle_cell<user_shared<T>, U>;
  using value_types = typename cell::value_types;
  using lvalue_reference_types = typename cell::lvalue_reference_types;
  using rvalue_reference_types = type_list<>;
  using pointer_types = typename cell::pointer_types;
};

template <typename T, typename U>
struct storage_traits<unique, user_unique<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = false;

  using value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using rvalue_reference_types =
      type_list<detail::wrapper_rebind_leaf_t<user_unique<T>, U> &&,
                user_shared<detail::wrapper_rebind_leaf_t<T, U>> &&>;
  using pointer_types = type_list<>;
};

template <typename T, typename U>
struct storage_traits<shared, user_unique<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using cell = probe_detail::handle_cell<user_unique<T>, U>;
  using value_types = type_list<U>;
  using lvalue_reference_types = typename cell::lvalue_reference_types;
  using rvalue_reference_types = type_list<>;
  using pointer_types = typename cell::pointer_types;
};

template <typename T, typename U>
struct storage_traits<external, user_unique<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using cell = probe_detail::handle_cell<user_unique<T>, U>;
  using value_types = type_list<U>;
  using lvalue_reference_types = typename cell::lvalue_reference_types;
  using rvalue_reference_types = type_list<>;
  using pointer_types = typename cell::pointer_types;
};

template <typename T, typename U>
struct storage_traits<unique, user_optional<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = false;

  using value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using rvalue_reference_types = type_list<user_optional<T> &&>;
  using pointer_types = type_list<>;
};

template <typename T, typename U>
struct storage_traits<shared, user_optional<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using value_types = type_list<U>;
  using lvalue_reference_types =
      type_list<U &, exact_lookup<user_optional<T>> &>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<U *, exact_lookup<user_optional<T>> *>;
};

template <typename T, typename U>
struct storage_traits<external, user_optional<T>, U> {
  static constexpr bool enabled = true;
  static constexpr bool is_stable = true;

  using value_types = type_list<U>;
  using lvalue_reference_types =
      type_list<U &, exact_lookup<user_optional<T>> &>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<U *, exact_lookup<user_optional<T>> *>;
};

template <typename T, typename U>
struct type_conversion_traits<
    user_shared<U>, user_shared<T>,
    std::enable_if_t<std::is_convertible_v<T *, U *>>> {
  template <typename Source>
  using required_access =
      std::conditional_t<std::is_rvalue_reference_v<Source>, consume, borrow>;

  static user_shared<U> convert(const user_shared<T> &source) {
    return user_shared<U>(std::static_pointer_cast<U>(source.native()));
  }

  static user_shared<U> convert(user_shared<T> &&source) {
    return user_shared<U>(
        std::static_pointer_cast<U>(std::move(source).native()));
  }
};
} // namespace dingo

namespace probe {
template <typename T>
inline constexpr bool is_leaf_v =
    std::is_same_v<std::remove_cv_t<T>, leaf> ||
    std::is_same_v<std::remove_cv_t<T>, move_only_leaf> ||
    std::is_same_v<std::remove_cv_t<T>, impl>;

// An interface reference reaches an instance through its aliased address.
template <typename T>
inline constexpr bool is_interface_v =
    std::is_same_v<std::remove_cv_t<T>, iface> ||
    std::is_same_v<std::remove_cv_t<T>, other_iface>;

// The address of the leaf a result reaches, or null for an empty handle.
template <typename X> const void *reach(const X &x) {
  using U = std::remove_cv_t<X>;
  if constexpr (is_leaf_v<U> || is_interface_v<U>) {
    return &x;
  } else if constexpr (std::is_array_v<U>) {
    return reach(x[0]);
  } else if constexpr (std::is_pointer_v<U>) {
    return x ? reach(*x) : nullptr;
  } else if constexpr (std::is_same_v<U, std::optional<leaf>> ||
                       std::is_same_v<U, std::optional<move_only_leaf>> ||
                       std::is_same_v<U, std::optional<impl>>) {
    return x ? reach(*x) : nullptr;
  } else if constexpr (requires { x.get(); }) {
    return reach(x.get());
  } else if constexpr (requires { x.has_value(); }) {
    return x ? reach(*x) : nullptr;
  } else {
    return std::visit(
        [](const auto &v) -> const void * {
          if constexpr (is_leaf_v<std::remove_cvref_t<decltype(v)>>)
            return &v;
          else
            return nullptr;
        },
        x);
  }
}

inline std::string classify() {
  try {
    throw;
  } catch (const dingo::type_not_found_exception &) {
    return "type_not_found_exception";
  } catch (const dingo::type_not_convertible_exception &) {
    return "type_not_convertible_exception";
  } catch (const dingo::type_ambiguous_exception &) {
    return "type_ambiguous_exception";
  } catch (const dingo::type_recursion_exception &) {
    return "type_recursion_exception";
  } catch (const dingo::lookup_already_registered_exception &) {
    return "lookup_already_registered_exception";
  } catch (const dingo::type_index_out_of_range_exception &) {
    return "type_index_out_of_range_exception";
  } catch (const dingo::virtual_pointer_exception &) {
    return "virtual_pointer_exception";
#ifdef _DEBUG
  } catch (const dingo::arena_allocation_exception &) {
    return "arena_allocation_exception";
#endif
  } catch (const dingo::exception &) {
    return "exception";
  } catch (const std::exception &) {
    return "std";
  } catch (...) {
    return "unknown";
  }
}

struct snapshot {
  unsigned long def, copy, move, dtor, seq;
};
inline snapshot take() {
  auto &s = st();
  return {s.def, s.copy, s.move, s.dtor, s.seq};
}
inline void print_counts(const char *key, const snapshot &a, const snapshot &b) {
  std::printf(",\"%s\":{\"default\":%ld,\"copy\":%ld,\"move\":%ld,\"destroy\":%ld}",
              key, (long)(b.def - a.def), (long)(b.copy - a.copy),
              (long)(b.move - a.move), (long)(b.dtor - a.dtor));
}
inline const char *identity(const void *address, unsigned long before) {
  if (!address)
    return "none";
  auto it = st().live.find(address);
  if (it == st().live.end())
    return "unknown";
  return it->second > before ? "new" : "existing";
}
inline void print_identity(const char *key, const void *address, unsigned long before,
                           bool applicable) {
  if (applicable)
    std::printf(",\"%s\":\"%s\"", key, identity(address, before));
  else
    std::printf(",\"%s\":null", key);
}

// Resolves R twice from a fresh container and prints one JSON line.
template <typename R, typename Container>
void run(int index, Container &container, const std::string &registration) {
  std::printf("{\"i\":%d,\"registration\":\"%s\"", index, registration.c_str());
  if (registration != "ok") {
    std::printf(",\"resolve\":\"not_run\"}\n");
    std::fflush(stdout);
    return;
  }
  constexpr bool plain_leaf =
      !std::is_reference_v<R> && is_leaf_v<std::remove_cvref_t<R>>;
  snapshot before = take();
  try {
    decltype(auto) first = container.template resolve<R>();
    snapshot after = take();
    const void *address = plain_leaf ? nullptr : reach(first);
    std::printf(",\"resolve\":\"ok\"");
    print_counts("counts", before, after);
    print_identity("identity", address, before.seq, !plain_leaf);
    snapshot before2 = take();
    try {
      decltype(auto) second = container.template resolve<R>();
      snapshot after2 = take();
      const void *address2 = plain_leaf ? nullptr : reach(second);
      std::printf(",\"stable\":%s", (!plain_leaf && address == address2) ? "true" : "false");
      print_counts("warm_counts", before2, after2);
      print_identity("warm_identity", address2, before2.seq, !plain_leaf);
    } catch (...) {
      std::printf(",\"stable\":false,\"warm_counts\":null,\"warm_identity\":null,"
                  "\"warm_resolve\":\"throws:%s\"", classify().c_str());
    }
  } catch (...) {
    std::printf(",\"resolve\":\"throws:%s\"", classify().c_str());
  }
  std::printf("}\n");
  std::fflush(stdout);
}
} // namespace probe
"""


@dataclass
class Group:
    domain: str
    leaf: str
    scope: str
    shape: Shape
    interfaces: tuple = ()
    requests: list = field(default_factory=list)

    @property
    def stored(self):
        return stored_spelling(self.shape, self.leaf)

    @property
    def label(self):
        return leaf_label(self.leaf, self.interfaces)

    @property
    def key(self):
        """The group as written in a case id up to the request."""
        return f"{self.domain}/{self.label}/{self.scope}/{self.stored}"

    @property
    def name(self):
        leaf = "+".join((self.leaf, *self.interfaces))
        return f"{leaf}-{self.scope}-{self.shape.name}"

    @property
    def directory(self):
        return self.domain

    def case(self, index):
        return case_id(
            self.domain,
            self.leaf,
            self.scope,
            self.stored,
            self.requests[index],
            self.interfaces,
        )


def container_code(group):
    """Lines declaring the container of a case, the registration, and the
    registration exception handling. The container is named c."""
    stored = group.stored
    scope = f"dingo::scope<dingo::{group.scope}>"
    interfaces = (
        f", dingo::interfaces<{', '.join(group.interfaces)}>"
        if group.interfaces
        else ""
    )
    if group.domain == "runtime":
        arguments = ""
        if group.scope == "external":
            arguments = spell(group.shape.external, group.leaf)
        return [
            "dingo::container<> c;",
            "try {",
            f"  c.register_type<{scope}, dingo::storage<{stored}>{interfaces}>"
            f"({arguments});",
            "} catch (...) { registration = probe::classify(); }",
        ]
    template = "dingo::static_container" if group.domain == "static" else "dingo::container"
    return [f"{template}<source> c;"]


def render(group, indices, guarded):
    """The program of a group. With guarded the program contains every request
    but a request is compiled only when ONLY is undefined or its index; the
    registration is always compiled. Otherwise it contains exactly the given
    requests and runs them, starting at argv[1]."""
    leaf = group.leaf
    lines = ['#include "prelude.h"', ""]
    if group.domain != "runtime":
        interfaces = (
            f", dingo::interfaces<{', '.join(group.interfaces)}>"
            if group.interfaces
            else ""
        )
        lines += [
            "using source = dingo::bindings<dingo::bind<"
            f"dingo::scope<dingo::{group.scope}>, dingo::storage<{group.stored}>"
            f"{interfaces}>>;",
            "",
        ]
    declarations = [spell(d, leaf) for d in group.shape.declarations]
    selected = range(len(group.requests)) if guarded else indices
    for index in selected:
        request = group.requests[index]
        lines.append(f"void run_case_{index}() {{")
        lines += [f"  {line}" for line in declarations]
        lines.append('  std::string registration = "ok";')
        lines += [f"  {line}" for line in container_code(group)]
        if guarded:
            lines.append(f"#if !defined(ONLY) || ONLY == {index}")
        lines.append(f"  probe::run<{request}>({index}, c, registration);")
        if guarded:
            lines.append("#endif")
        lines.append("}")
        lines.append("")
    lines.append("int main(int argc, char **argv) {")
    lines.append("  int from = argc > 1 ? std::atoi(argv[1]) : 0;")
    for index in selected:
        lines.append(f"  if (from <= {index}) run_case_{index}();")
    lines.append("  return 0;")
    lines.append("}")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Toolchain.
# ---------------------------------------------------------------------------


class Toolchain:
    def __init__(self, command):
        self.command = shlex.split(command)
        version = self._run([*self.command, "--version"]).lower()
        if "clang" in version:
            self.family = "clang"
        elif "gcc" in version or "g++" in version:
            self.family = "gcc"
        else:
            self.family = "unknown"
        self.major = self._run([*self.command, "-dumpversion"]).split(".", 1)[0]
        machine = platform.machine().lower()
        self.arch = {"amd64": "x86_64", "aarch64": "arm64"}.get(machine, machine)
        std = next(
            (arg for arg in self.command if arg.startswith("-std=")), "-std=default"
        )
        self.std = std.split("=", 1)[1]

    @property
    def key(self):
        return f"{self.family}{self.major}-{self.arch}-{self.std}"

    @staticmethod
    def _run(args):
        return subprocess.run(
            args, check=True, capture_output=True, text=True
        ).stdout.strip()

    def compile(self, args):
        """Returns (succeeded, first error line)."""
        result = subprocess.run(
            [*self.command, *args], capture_output=True, text=True
        )
        if result.returncode == 0:
            return True, ""
        error = next(
            (l for l in result.stderr.splitlines() if "error" in l),
            result.stderr.strip()[:300],
        )
        return False, error.strip()[:400]


# ---------------------------------------------------------------------------
# Extraction.
# ---------------------------------------------------------------------------


def build_groups(domains, pattern=None):
    groups = []
    for domain, (leaf, interfaces), scope in itertools.product(
        domains, LEAF_KINDS, SCOPES
    ):
        shapes = SHAPES + (USER_SHAPES if leaf in USER_WRAPPER_LEAVES else ())
        for shape in shapes:
            group = Group(domain, leaf, scope, shape, interfaces)
            group.requests = requests_for(shape, leaf, interfaces)
            if pattern is None or re.search(pattern, group.key):
                groups.append(group)
    return groups


class Extraction:
    def __init__(self, toolchain, work_dir, jobs):
        self.toolchain = toolchain
        self.work_dir = work_dir / toolchain.key
        self.jobs = jobs
        self.work_dir.mkdir(parents=True, exist_ok=True)
        (self.work_dir / "prelude.h").write_text(PRELUDE)
        self.errors = {}

    def path(self, group, suffix):
        directory = self.work_dir / group.domain
        directory.mkdir(exist_ok=True)
        return directory / f"{group.name}{suffix}"

    def syntax(self, group, source, only):
        args = ["-fsyntax-only", f"-I{self.work_dir}", str(source)]
        if only is not None:
            args.insert(0, f"-DONLY={only}")
        return self.toolchain.compile(args)

    def map(self, function, items):
        items = list(items)
        with concurrent.futures.ThreadPoolExecutor(max_workers=self.jobs) as pool:
            return list(pool.map(function, items))

    def run(self, domains, pattern=None):
        groups = build_groups(domains, pattern)
        for group in groups:
            self.path(group, ".cpp").write_text(
                render(group, range(len(group.requests)), guarded=True)
            )

        # Registration outcome: the program without any resolve.
        log("registration compiles", len(groups))
        outcomes = self.map(lambda g: self.syntax(g, self.path(g, ".cpp"), -1), groups)
        compiled = {}
        registration_errors = {}
        for group, (ok, error) in zip(groups, outcomes):
            if ok:
                compiled[id(group)] = True
            else:
                registration_errors[id(group)] = error

        # Resolve compile outcome: all requests at once, else one by one.
        live = [g for g in groups if id(g) in compiled]
        log("whole-group compiles", len(live))
        whole = self.map(lambda g: self.syntax(g, self.path(g, ".cpp"), None), live)
        compile_ok = {}
        singles = []
        for group, (ok, _) in zip(live, whole):
            if ok:
                compile_ok[id(group)] = set(range(len(group.requests)))
            else:
                compile_ok[id(group)] = set()
                singles += [(group, i) for i in range(len(group.requests))]
        log("per-request compiles", len(singles))
        results = self.map(
            lambda task: self.syntax(task[0], self.path(task[0], ".cpp"), task[1]),
            singles,
        )
        for (group, index), (ok, _) in zip(singles, results):
            if ok:
                compile_ok[id(group)].add(index)

        # Executables, one per group with at least one compiling request.
        log("executables", len(live))
        runs = self.map(lambda g: self.build_and_run(g, compile_ok[id(g)]), live)
        observed = {id(g): r for g, r in zip(live, runs)}

        records = []
        for group in groups:
            records += self.records(
                group,
                registration_errors.get(id(group)),
                observed.get(id(group)),
            )
        for group in groups:
            if id(group) in registration_errors:
                self.errors[group.key] = registration_errors[id(group)]
        records.sort(key=lambda r: r["id"])
        return records

    def build_and_run(self, group, indices):
        """Returns {index: parsed line or crash marker}, plus indices that fail
        to build as 'compile_error'."""
        indices = sorted(indices)
        result = {}
        executable = self.path(group, ".exe")
        while indices:
            source = self.path(group, ".run.cpp")
            source.write_text(render(group, indices, guarded=False))
            ok, error = self.toolchain.compile(
                ["-O0", f"-I{self.work_dir}", str(source), "-o", str(executable)]
            )
            if ok:
                break
            # Syntax-only compiles missed an error that code generation finds:
            # find the failing requests by building each alone.
            failing = []
            for index in indices:
                source_one = self.path(group, ".one.cpp")
                source_one.write_text(render(group, [index], guarded=False))
                if not self.toolchain.compile(
                    ["-c", f"-I{self.work_dir}", str(source_one), "-o", os.devnull]
                )[0]:
                    failing.append(index)
            if not failing:
                raise RuntimeError(f"{source} fails to build: {error}")
            for index in failing:
                result[index] = {"resolve": "compile_error", "registration": "ok"}
            indices = [i for i in indices if i not in failing]
        position = 0
        while position < len(indices):
            start = indices[position]
            process = subprocess.run(
                [str(executable), str(start)], capture_output=True, text=True
            )
            printed = []
            for line in process.stdout.splitlines():
                parsed = json.loads(line)
                result[parsed.pop("i")] = parsed
                printed.append(parsed)
            if process.returncode == 0:
                break
            crashed = next(i for i in indices[position:] if i not in result)
            name = (
                signal.Signals(-process.returncode).name
                if process.returncode < 0
                else f"exit{process.returncode}"
            )
            result[crashed] = {"resolve": f"crash:{name}", "registration": "ok"}
            position = indices.index(crashed) + 1
        return result

    def records(self, group, registration_error, observed):
        for index, request in enumerate(group.requests):
            record = {
                "counts": None,
                "domain": group.domain,
                "id": group.case(index),
                "identity": None,
                "interfaces": ",".join(group.interfaces),
                "leaf": group.leaf,
                "registration": "ok",
                "request": request,
                "resolve": "compile_error",
                "scope": group.scope,
                "stable": None,
                "stored": group.stored,
                "warm_counts": None,
                "warm_identity": None,
            }
            if registration_error is not None:
                record["registration"] = "compile_error"
                record["resolve"] = "not_run"
            elif observed is not None and index in observed:
                record.update(observed[index])
                record.pop("warm_resolve", None)
                if record["registration"] not in ("ok", "compile_error"):
                    record["registration"] = "throws:" + record["registration"]
            yield record


def log(message, count):
    print(f"[{time.strftime('%H:%M:%S')}] {message}: {count}", file=sys.stderr, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "--include-dir",
        type=Path,
        default=PROJECT_ROOT / "include",
        help="include directory of the library to extract, include/ by default",
    )
    parser.add_argument(
        "--cxx-command",
        help="compiler command including the standard and include flags; by "
        "default $CXX -std=c++20 -I<include-dir>",
    )
    parser.add_argument(
        "--work-dir",
        type=Path,
        required=True,
        help="directory for generated programs and executables",
    )
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 8))
    parser.add_argument("--domains", default=",".join(DOMAINS))
    parser.add_argument("--output", type=Path, default=None)
    parser.add_argument(
        "--filter",
        help="regular expression selecting groups by domain/leaf/scope/stored "
        "(development aid; the output then covers the selection only)",
    )
    args = parser.parse_args()

    domains = args.domains.split(",")
    unknown = [d for d in domains if d not in DOMAINS]
    if unknown:
        parser.error(f"unknown domains: {unknown}")

    command = args.cxx_command or (
        os.environ.get("CXX", "c++") + f" -std=c++20 -I{args.include_dir.resolve()}"
    )
    toolchain = Toolchain(command)
    started = time.time()
    extraction = Extraction(toolchain, args.work_dir.resolve(), args.jobs)
    records = extraction.run(domains, args.filter)
    output = args.output or args.work_dir / "observed" / f"{toolchain.key}.jsonl"
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w") as stream:
        for record in records:
            stream.write(json.dumps(record, sort_keys=True) + "\n")
    errors = extraction.work_dir / "registration-errors.txt"
    errors.write_text(
        "".join(f"{key}\t{error}\n" for key, error in sorted(extraction.errors.items()))
    )
    per_domain = {d: sum(1 for r in records if r["domain"] == d) for d in domains}
    print(
        f"{toolchain.key}: {len(records)} cases {per_domain} in "
        f"{time.time() - started:.0f}s -> {output}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
