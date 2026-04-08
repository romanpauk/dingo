// RUN: %dingo_cxx -c %s

#include <dingo/storage/storage_traits.h>

struct custom_storage_tag {};

template <>
struct dingo::storage_traits<custom_storage_tag, int, int> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = true;

    using value_types = dingo::type_list<int>;
    using lvalue_reference_types = dingo::type_list<int&>;
    using rvalue_reference_types = dingo::type_list<>;
    using pointer_types = dingo::type_list<int*>;
    using conversion_types = dingo::type_list<>;
};

struct test_storage {
    int value = 42;

    int& resolve(int&, int&) { return value; }
};

int main() {
    using conversions = dingo::storage_traits<custom_storage_tag, int, int>;
    using materialization =
        dingo::detail::storage_materialization<custom_storage_tag, conversions>;

    int context = 0;
    int container = 0;
    test_storage storage;

    auto guard = materialization::template make_guard<int>(context, storage);
    (void)guard;

    auto source =
        materialization::materialize_source(context, storage, container);

    return materialization::preserves_closure(storage) ||
           &source.get() != &storage.value;
}
