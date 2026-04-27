// RUN: not %dingo_cxx -c %s 2>&1 | grep -F "static resolution cannot satisfy a request the storage does not publish"

#include <dingo/container.h>
#include <dingo/static/injector.h>

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

std::shared_ptr<impl> make_impl() {
    return std::make_shared<impl>();
}

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<std::shared_ptr<impl>>,
                dingo::interfaces<iface>, dingo::factory<dingo::function<make_impl>>>>;

void test_static() {
    dingo::static_injector<source> injector;
    (void)injector.resolve<std::shared_ptr<iface>&&>();
}

void test_hybrid() {
    dingo::container<source> hybrid;
    (void)hybrid.resolve<std::shared_ptr<iface>&&>();
}
