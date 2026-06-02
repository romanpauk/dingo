// clang-format off
// RUN: not %dingo_cxx -c %s 2>&1 | grep -F "static resolution cannot satisfy a request the storage does not publish"
// clang-format on

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

void test_static() {
    dingo::static_container<source> container;
    (void)container.resolve<std::shared_ptr<iface>&&>();
}

void test_static_runtime() {
    dingo::container<source> static_runtime;
    (void)static_runtime.resolve<std::shared_ptr<iface>&&>();
}
