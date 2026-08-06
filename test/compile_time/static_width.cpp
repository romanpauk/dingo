#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <memory>
#include <utility>

#ifndef DINGO_COMPILE_TIME_WIDTH
#define DINGO_COMPILE_TIME_WIDTH 10
#endif

template <std::size_t Index> struct service {};

template <typename> struct make_source;

template <std::size_t... Indexes>
struct make_source<std::index_sequence<Indexes...>> {
  using type = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>,
                  dingo::storage<std::shared_ptr<service<Indexes>>>>...>;
};

using source = typename make_source<
    std::make_index_sequence<DINGO_COMPILE_TIME_WIDTH>>::type;

int main() {
  dingo::container<source> container;
  return container.resolve<service<DINGO_COMPILE_TIME_WIDTH - 1> *>() ==
         nullptr;
}
