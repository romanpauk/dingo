// A shared binding never serves an rvalue reference, also not to the handle
// of an interface converted from its handle, so the request is rejected when
// it is compiled, in a static and in a mixed container.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// RUN: not %dingo_next_cxx -fsyntax-only -DMIXED_ONLY %s 2>&1 | %filecheck %s
// CHECK: type is not convertible to

#include <dingo/container.h>
#include <dingo/static_container.h>

#include <dingo/factory/function.h>
#include <dingo/storage/shared.h>

#include <memory>

struct iface {
  virtual ~iface() = default;
  virtual int read() const = 0;
};

struct impl : iface {
  int read() const override { return 7; }
};

std::shared_ptr<impl> make_impl() { return std::make_shared<impl>(); }

using source = dingo::bindings<dingo::bind<
    dingo::scope<dingo::shared>, dingo::storage<std::shared_ptr<impl>>,
    dingo::interfaces<iface>, dingo::factory<dingo::function<make_impl>>>>;

#ifndef MIXED_ONLY
void test_static() {
  dingo::static_container<source> container;
  (void)container.resolve<std::shared_ptr<iface> &&>();
}
#endif

void test_static_runtime() {
  dingo::container<source> static_runtime;
  (void)static_runtime.resolve<std::shared_ptr<iface> &&>();
}
