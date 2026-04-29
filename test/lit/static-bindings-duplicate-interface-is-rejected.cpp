// RUN: %dingo_cxx -c %s

#include <dingo/core/binding_selection.h>
#include <dingo/static/registry.h>
#include <dingo/storage/unique.h>

struct duplicate_key : std::integral_constant<int, 0> {};

struct interface {
    virtual ~interface() = default;
};
struct service_a : interface {};
struct service_b : interface {};

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<service_a>,
                dingo::interfaces<interface>, dingo::key<duplicate_key>>,
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<service_b>,
                dingo::interfaces<interface>, dingo::key<duplicate_key>>>;
using registry_type = typename source::type;
using selection = dingo::detail::static_binding_t<
    typename registry_type::template bindings<interface, duplicate_key>>;

static_assert(registry_type::valid);
static_assert(dingo::type_list_size_v<
                  typename registry_type::template bindings<
                      interface, duplicate_key>> == 2);
static_assert(selection::status ==
              dingo::detail::binding_selection_status::ambiguous);

int main() {}
