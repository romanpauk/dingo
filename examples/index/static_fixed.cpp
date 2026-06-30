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
      dingo::indexed<IProcessor &, dingo::key<std::size_t, 0>> first_processor)
      : first(first_processor) {}

  IProcessor &first;
};

////
int main() {
  using namespace dingo;
  ////
  using source = bindings<bind<scope<shared>, storage<Processor<0>>,
                               interfaces<IProcessor>, key<std::size_t, 0>>,
                          bind<scope<shared>, storage<Processor<1>>,
                               interfaces<IProcessor>, key<std::size_t, 1>>>;

  struct container_traits : static_container_traits<> {
    using index_definition_type =
        selectors<associative<IProcessor, std::size_t>>;
  };

  static_container<source, container_traits> container;

  auto &first = container.resolve<indexed<IProcessor &, key<std::size_t, 0>>>();
  auto pipeline = container.construct<Pipeline>();

  return first.id() == 0 && &pipeline.first == &first ? 0 : 1;
  ////
}
