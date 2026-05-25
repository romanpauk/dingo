//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <cassert>
#include <memory>

////
class config {
  public:
    int retries() const { return retries_; }

  private:
    int retries_ = 3;
};

struct service_interface {
    virtual ~service_interface() {}
    virtual int retries() const = 0;
};

struct service : service_interface {
    explicit service(config& cfg) : cfg_(cfg) {}

    int retries() const override { return cfg_.retries(); }

  private:
    config& cfg_;
};

void compile_time_registration_example() {
    using namespace dingo;
    using app_bindings = dingo::bindings<
        dingo::bind<scope<shared>, storage<config>>,
        dingo::bind<scope<shared>, storage<std::shared_ptr<service>>,
                    interfaces<service_interface>>>;

    container<app_bindings> container;

    assert(container.resolve<service_interface&>().retries() == 3);
    assert(container.resolve<service_interface*>()->retries() == 3);
    assert(
        container.resolve<std::shared_ptr<service_interface>&>()->retries() ==
        3);

    static_container<app_bindings> static_only;
    assert(static_only.resolve<service_interface&>().retries() == 3);
}
////

int main() {
    compile_time_registration_example();
    return 0;
}
