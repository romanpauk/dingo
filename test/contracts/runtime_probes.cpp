//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The runtime cost probes of the contract baseline, which probe.py compiles,
// disassembles, and runs.
//
// Each probe_resolve_* function resolves a request of the container it is
// given, which resolved the request before. A probe whose name ends in
// _reference returns the address of an object of a stable binding, so the
// cheapest path through it is the memoized resolution and calls nothing of
// the object.
//
// The program counts the allocations of a first and of a repeated resolution
// of each probe, and prints them as `<probe>.first <count>` and
// `<probe>.memoized <count>`.

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>

namespace {
std::size_t allocations = 0;
} // namespace

void *operator new(std::size_t size) {
  ++allocations;
  if (void *memory = std::malloc(size != 0 ? size : 1)) {
    return memory;
  }
  throw std::bad_alloc();
}

void operator delete(void *memory) noexcept { std::free(memory); }

void operator delete(void *memory, std::size_t /*size*/) noexcept {
  std::free(memory);
}

namespace probes {

struct config {
  config() : value(7) {}
  int value;
};

struct value_config {
  value_config() : value(9) {}
  int value;
};

struct iface {
  virtual ~iface() = default;
  virtual int read() const = 0;
};

struct impl : iface {
  int read() const override { return 11; }
};

struct handle_iface {
  virtual ~handle_iface() = default;
  virtual int read() const = 0;
};

struct handle_impl : handle_iface {
  int read() const override { return 13; }
};

using runtime_container = dingo::container<>;

inline void register_runtime(runtime_container &container) {
  container
      .register_type<dingo::scope<dingo::shared>, dingo::storage<config>>();
  container.register_type<dingo::scope<dingo::unique>,
                          dingo::storage<value_config>>();
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<impl>,
                          dingo::interfaces<iface>>();
  container.register_type<dingo::scope<dingo::shared>,
                          dingo::storage<std::shared_ptr<handle_impl>>,
                          dingo::interfaces<handle_iface>>();
}

// The static bindings allocate no object they store, so a static resolution
// that allocates allocates for itself.

using static_source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<config>>,
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<value_config>>,
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<impl>,
                dingo::interfaces<iface>>>;
using static_container = dingo::static_container<static_source>;

} // namespace probes

extern "C" [[gnu::noinline]] int
probe_resolve_runtime_shared_reference(probes::runtime_container &container) {
  return container.resolve<probes::config &>().value;
}

extern "C" [[gnu::noinline]] const void *
probe_resolve_runtime_interface_reference(
    probes::runtime_container &container) {
  return &container.resolve<probes::iface &>();
}

extern "C" [[gnu::noinline]] const void *
probe_resolve_runtime_handle_reference(probes::runtime_container &container) {
  return container.resolve<std::shared_ptr<probes::handle_iface> &>().get();
}

extern "C" [[gnu::noinline]] int
probe_resolve_runtime_unique_value(probes::runtime_container &container) {
  return container.resolve<probes::value_config>().value;
}

extern "C" [[gnu::noinline]] int
probe_resolve_static_shared_reference(probes::static_container &container) {
  return container.resolve<probes::config &>().value;
}

extern "C" [[gnu::noinline]] const void *
probe_resolve_static_interface_reference(probes::static_container &container) {
  return &container.resolve<probes::iface &>();
}

extern "C" [[gnu::noinline]] int
probe_resolve_static_unique_value(probes::static_container &container) {
  return container.resolve<probes::value_config>().value;
}

namespace {

template <typename Container, typename Probe>
void count(const char *name, Container &container, Probe probe) {
  const std::size_t before = allocations;
  const auto first_result = probe(container);
  const std::size_t first = allocations;
  const auto memoized_result = probe(container);
  const std::size_t memoized = allocations;
  if (first_result != memoized_result) {
    std::abort();
  }
  std::printf("%s.first %zu\n%s.memoized %zu\n", name, first - before, name,
              memoized - first);
}

} // namespace

int main() {
  probes::runtime_container runtime;
  probes::register_runtime(runtime);
  count("runtime_shared_reference", runtime,
        probe_resolve_runtime_shared_reference);
  count("runtime_interface_reference", runtime,
        probe_resolve_runtime_interface_reference);
  count("runtime_handle_reference", runtime,
        probe_resolve_runtime_handle_reference);
  count("runtime_unique_value", runtime, probe_resolve_runtime_unique_value);

  probes::static_container statics;
  count("static_shared_reference", statics,
        probe_resolve_static_shared_reference);
  count("static_interface_reference", statics,
        probe_resolve_static_interface_reference);
  count("static_unique_value", statics, probe_resolve_static_unique_value);
  return 0;
}
