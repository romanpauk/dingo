// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static/injector.h>
#include <dingo/storage/shared.h>

#include <memory>
#include <type_traits>

struct interface {
    virtual ~interface() = default;
};

struct first : interface {};
struct second : interface {};

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<std::shared_ptr<first>>,
                dingo::interfaces<interface>>,
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<std::shared_ptr<second>>,
                dingo::interfaces<interface>>>;

void test() {
    dingo::static_injector<source> injector;
    (void)injector.resolve<std::shared_ptr<interface>>();
}

// CHECK: static_injector cannot resolve an ambiguously bound type
