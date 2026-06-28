// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/index/array.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct processor {};

struct traits : dingo::dynamic_container_traits {
    using index_definition_type = dingo::indexes<
        dingo::index<processor, std::size_t, dingo::index_type::array<2>>,
        dingo::index<processor, dingo::key<std::size_t>,
                     dingo::index_type::array<4>>>;
};

int main() {
    dingo::container<traits> container;
    container.register_indexed_type<dingo::scope<dingo::shared>,
                                    dingo::storage<processor>,
                                    dingo::interfaces<processor>>(
        std::size_t(1));
}

// CHECK: duplicate dingo index definition for interface/key
