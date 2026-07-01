//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

////
struct IProcessor {
  virtual ~IProcessor() = default;
  virtual int id() const = 0;
};

template <int Id> struct Processor : IProcessor {
  int id() const override { return Id; }
};

struct Pipeline {
  explicit Pipeline(
      dingo::request<IProcessor &, dingo::key<std::size_t, 0>> first_processor)
      : first(first_processor) {}

  IProcessor &first;
};

////
int main() {
  ////
  using source = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Processor<0>>,
                  dingo::interfaces<IProcessor>, dingo::key<std::size_t, 0>>,
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<Processor<1>>,
                  dingo::interfaces<IProcessor>, dingo::key<std::size_t, 1>>>;

  struct container_traits : dingo::static_container_traits<> {
    using lookup_definition_type =
        dingo::lookups<dingo::associative<std::size_t, IProcessor>>;
  };

  dingo::static_container<source, container_traits> container;

  auto &first =
      container
          .resolve<dingo::request<IProcessor &, dingo::key<std::size_t, 0>>>();
  auto pipeline = container.construct<Pipeline>();

  return first.id() == 0 && &pipeline.first == &first ? 0 : 1;
  ////
}
