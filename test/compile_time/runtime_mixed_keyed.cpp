#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <memory>
#include <utility>

#ifndef DINGO_COMPILE_TIME_WIDTH
#define DINGO_COMPILE_TIME_WIDTH 20
#endif

template <std::size_t Index> struct service {};
template <std::size_t Index> struct service_key {};

template <std::size_t Index, bool Shared = Index % 2 == 0>
struct register_service;

template <std::size_t Index> struct register_service<Index, true> {
  static void apply(dingo::container<> &container) {
    container
        .template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<std::shared_ptr<service<Index>>>,
                                dingo::interfaces<service<Index>>,
                                dingo::key_type<service_key<Index>>>();
  }
};

template <std::size_t Index> struct register_service<Index, false> {
  static void apply(dingo::container<> &container) {
    container
        .template register_type<dingo::scope<dingo::unique>,
                                dingo::storage<std::unique_ptr<service<Index>>>,
                                dingo::interfaces<service<Index>>,
                                dingo::key_type<service_key<Index>>>();
  }
};

template <std::size_t... Indexes>
void register_services(dingo::container<> &container,
                       std::index_sequence<Indexes...>) {
  (register_service<Indexes>::apply(container), ...);
}

int main() {
  dingo::container<> container;
  register_services(container,
                    std::make_index_sequence<DINGO_COMPILE_TIME_WIDTH>{});
  return 0;
}
