//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

struct Logger {};

struct Service {
    explicit Service(Logger& logger) : logger_(logger) {}
    Logger& logger_;
};

int main() {
    using namespace dingo;

    using application =
        component<registration<scope<shared>, storage<Logger>>,
                  registration<scope<unique>, storage<Service>>>;

    container<> container;
    install<application>(container);

    auto service = container.resolve<Service>();
    auto& logger = container.resolve<Logger&>();

    return &service.logger_ == &logger ? 0 : 1;
}
