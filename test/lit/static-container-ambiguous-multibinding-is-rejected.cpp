// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>
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
    dingo::static_container<source> container;
    (void)container.resolve<std::shared_ptr<interface>>();
}

// CHECK: static_container cannot resolve an ambiguously bound type
