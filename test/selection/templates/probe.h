//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The types of the selection probes and the reporting of one case as one JSON
// line: the outcome of every registration and of the request.

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <type_traits>
#include <vector>

struct iface {
  virtual ~iface() = default;
  virtual int id() const = 0;
};

template <int Index> struct impl : iface {
  int id() const override { return Index; }
};

using impl_0 = impl<0>;
using impl_1 = impl<1>;
using impl_2 = impl<2>;
using impl_3 = impl<3>;

struct tag_a {};
struct tag_b {};

// A request by a fixed value: a constructor parameter of a consumer.
template <int Value> struct consumer {
  explicit consumer(dingo::dependency<iface &, dingo::key_type<int, Value>> dep)
      : selected(dep) {}

  iface &selected;
};

namespace probe {
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

// The outcomes of the registrations of one case, in binding order.
struct registrations {
  std::vector<std::string> outcomes;

  template <typename Fn> void attempt(Fn &&fn) {
    try {
      fn();
      outcomes.push_back("ok");
    } catch (...) {
      outcomes.push_back("throws:" + classify());
    }
  }

  void fixed() { outcomes.push_back("ok"); }
};

inline void print_registrations(int index, const registrations &regs) {
  std::printf("{\"i\":%d,\"registrations\":[", index);
  for (std::size_t i = 0; i < regs.outcomes.size(); ++i)
    std::printf("%s\"%s\"", i ? "," : "", regs.outcomes[i].c_str());
  std::printf("]");
}

// Reports the registrations of a case without a request.
inline void report_registrations(const registrations &regs) {
  print_registrations(-1, regs);
  std::printf("}\n");
  std::fflush(stdout);
}

// Runs a singular request returning the selected interface reference.
template <typename Fn>
void singular(int index, const registrations &regs, Fn &&fn) {
  print_registrations(index, regs);
  try {
    int selected = fn();
    std::printf(",\"selection\":\"ok\",\"selected\":%d}\n", selected);
  } catch (...) {
    std::printf(",\"selection\":\"throws:%s\"}\n", classify().c_str());
  }
  std::fflush(stdout);
}

// Runs a collection request returning a std::vector<iface*>.
template <typename Fn>
void collection(int index, const registrations &regs, Fn &&fn) {
  print_registrations(index, regs);
  try {
    std::vector<iface *> selected = fn();
    std::printf(",\"selection\":\"ok\",\"selected\":[");
    for (std::size_t i = 0; i < selected.size(); ++i)
      std::printf("%s%d", i ? "," : "", selected[i]->id());
    std::printf("]}\n");
  } catch (...) {
    std::printf(",\"selection\":\"throws:%s\"}\n", classify().c_str());
  }
  std::fflush(stdout);
}
} // namespace probe
