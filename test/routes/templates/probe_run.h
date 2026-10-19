//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Resolving a request in a fresh container and reporting what it does as one
// JSON line.

#include "probe_types.h"

#include <dingo/container.h>

#include <cstdio>
#include <exception>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

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

// The message of the exception being handled, as a JSON string.
inline std::string message() {
  std::string text;
  try {
    throw;
  } catch (const std::exception &exception) {
    text = exception.what();
  } catch (...) {
  }
  std::string quoted = "\"";
  for (char c : text) {
    if (c == '"' || c == '\\') {
      quoted += '\\';
      quoted += c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      quoted += ' ';
    } else {
      quoted += c;
    }
  }
  return quoted + "\"";
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
inline void print_counts(const char *key, const snapshot &a,
                         const snapshot &b) {
  std::printf(
      ",\"%s\":{\"default\":%ld,\"copy\":%ld,\"move\":%ld,\"destroy\":%ld}",
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
inline void print_identity(const char *key, const void *address,
                           unsigned long before, bool applicable) {
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
      std::printf(",\"stable\":%s",
                  (!plain_leaf && address == address2) ? "true" : "false");
      print_counts("warm_counts", before2, after2);
      print_identity("warm_identity", address2, before2.seq, !plain_leaf);
    } catch (...) {
      std::printf(
          ",\"stable\":false,\"warm_counts\":null,\"warm_identity\":null,"
          "\"warm_resolve\":\"throws:%s\"",
          classify().c_str());
    }
  } catch (...) {
    std::printf(",\"resolve\":\"throws:%s\",\"message\":%s", classify().c_str(),
                message().c_str());
  }
  std::printf("}\n");
  std::fflush(stdout);
}
} // namespace probe
