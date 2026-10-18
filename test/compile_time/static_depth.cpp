#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <memory>
#include <utility>

#ifndef DINGO_COMPILE_TIME_WIDTH
#define DINGO_COMPILE_TIME_WIDTH 16
#endif

template <std::size_t Index> struct service {
  explicit service(service<Index - 1> &) {}
};

template <> struct service<0> {};

template <std::size_t Index>
using chain_binding =
    dingo::bind<dingo::scope<dingo::shared>,
                dingo::storage<std::shared_ptr<service<Index>>>,
                dingo::dependencies<service<Index - 1> &>>;

template <typename> struct make_source;

template <std::size_t... Indexes>
struct make_source<std::index_sequence<Indexes...>> {
  using type =
      dingo::bindings<dingo::bind<dingo::scope<dingo::shared>,
                                  dingo::storage<std::shared_ptr<service<0>>>>,
                      chain_binding<Indexes + 1>...>;
};

using source = typename make_source<
    std::make_index_sequence<DINGO_COMPILE_TIME_WIDTH - 1>>::type;

int main() {
  dingo::container<source> container;
  return container.resolve<service<DINGO_COMPILE_TIME_WIDTH - 1> *>() ==
         nullptr;
}
