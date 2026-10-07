#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <cstddef>
#include <utility>

#ifndef DINGO_COMPILE_TIME_WIDTH
#define DINGO_COMPILE_TIME_WIDTH 50
#endif

struct service_interface {
  virtual ~service_interface() = default;
};

template <std::size_t Index> struct service : service_interface {};

template <std::size_t Index>
using keyed_binding =
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<service<Index>>,
                dingo::interfaces<service_interface>,
                dingo::key_type<std::size_t, Index>>;

template <typename> struct make_source;

template <std::size_t... Indexes>
struct make_source<std::index_sequence<Indexes...>> {
  using type = dingo::bindings<keyed_binding<Indexes>...>;
};

using source = typename make_source<
    std::make_index_sequence<DINGO_COMPILE_TIME_WIDTH>>::type;

int main() {
  using static_bindings = typename source::type;
  using interface_bindings = typename static_bindings::interface_bindings;
  using empty_lookup_entries = dingo::type_list<>;
  return dingo::detail::key_value_bindings_are_unique<
      interface_bindings, empty_lookup_entries>::value;
}
