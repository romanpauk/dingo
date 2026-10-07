#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <memory>
#include <utility>

#ifndef DINGO_COMPILE_TIME_WIDTH
#define DINGO_COMPILE_TIME_WIDTH 10
#endif

template <std::size_t Index> struct first_interface {
  virtual ~first_interface() = default;
};

template <std::size_t Index> struct second_interface {
  virtual ~second_interface() = default;
};

template <std::size_t Index>
struct implementation : first_interface<Index>, second_interface<Index> {};

template <std::size_t... Indexes>
void register_services(dingo::container<> &container,
                       std::index_sequence<Indexes...>) {
  (container.template register_type<
       dingo::scope<dingo::shared>,
       dingo::storage<std::shared_ptr<implementation<Indexes>>>,
       dingo::interfaces<first_interface<Indexes>,
                         second_interface<Indexes>>>(),
   ...);
}

int main() {
  dingo::container<> container;
  register_services(container,
                    std::make_index_sequence<DINGO_COMPILE_TIME_WIDTH>{});
  return container.resolve<first_interface<DINGO_COMPILE_TIME_WIDTH - 1> *>() ==
         nullptr;
}
