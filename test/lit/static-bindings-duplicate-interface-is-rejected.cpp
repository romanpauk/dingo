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
                dingo::interfaces<interface>, dingo::key_type<duplicate_key>>,
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<service_b>,
                dingo::interfaces<interface>, dingo::key_type<duplicate_key>>>;
using registry_type = typename source::type;
using lookup_key_type =
    dingo::detail::make_lookup_key_t<dingo::key_type<duplicate_key>>;
using selection = dingo::detail::static_binding_t<
    typename registry_type::template bindings<interface, lookup_key_type>>;

static_assert(registry_type::valid);
static_assert(dingo::type_list_size_v<typename registry_type::template bindings<
                  interface, lookup_key_type>> == 2);
static_assert(selection::status == dingo::detail::binding_status::ambiguous);

int main() {}
