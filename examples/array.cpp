//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <memory>

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

////
using namespace dingo;

struct cell {
    cell() = default;
};

struct row_consumer {
    cell (*rows)[3];

    explicit row_consumer(cell (*init)[3]) : rows(init) {}
};

////
int main() {
    ////
    container<> raw_container;
    // Register a raw N-D array in shared scope.
    raw_container.register_type<scope<shared>, storage<cell[2][3]>>();

    // Resolve row view, exact pointer view and inject the row view.
    /*auto* rows =*/ raw_container.resolve<cell(*)[3]>();
    /*auto& exact =*/ raw_container.resolve<cell(&)[2][3]>();
    /*row_consumer borrowed =*/
        raw_container.construct<row_consumer, constructor<row_consumer(cell(*)[3])>>();

    container<> unique_container;
    // Register a fixed-size array in unique scope and resolve it as an owning
    // dynamic array handle.
    unique_container.register_type<scope<unique>, storage<cell[4]>>();
    /*auto owned =*/ unique_container.resolve<std::unique_ptr<cell[]>>();

    container<> shared_container;
    // Register a shared smart array directly.
    shared_container
        .register_type<scope<shared>, storage<std::shared_ptr<cell[]>>>(
            callable([] { return std::shared_ptr<cell[]>(new cell[4]); }));
    /*auto shared =*/ shared_container.resolve<std::shared_ptr<cell[]>>();
    ////
}
