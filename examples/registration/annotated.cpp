//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/unique.h>

#include <cassert>

////
struct primary_tag {};
struct replica_tag {};

struct database {
    int id;
};

struct repository {
    repository(dingo::annotated<database&, primary_tag> primary_db,
               dingo::annotated<database&, replica_tag> replica_db)
        : primary(primary_db), replica(replica_db) {}

    database& primary;
    database& replica;
};

////
int main() {
    using namespace dingo;

////
    database primary{1};
    database replica{2};
    container<> container;

    container.register_type<scope<external>, storage<database*>,
                            interfaces<annotated<database, primary_tag>>>(
        &primary);
    container.register_type<scope<external>, storage<database*>,
                            interfaces<annotated<database, replica_tag>>>(
        &replica);
    container.register_type<scope<unique>, storage<repository>>();

    [[maybe_unused]] auto repo = container.resolve<repository>();
    assert(&repo.primary == &primary);
    assert(&repo.replica == &replica);
////

    return 0;
}
