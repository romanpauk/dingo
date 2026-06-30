//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>

////
struct IProcessor {
  virtual ~IProcessor() = default;
};

template <size_t N> struct Processor : IProcessor {};

////
int main() {
  using namespace dingo;
  ////
  struct container_traits : dynamic_container_traits {
    using query_definition_type = queries<collection<IProcessor>>;
  };

  container<container_traits> container;

  // Register types under the same interface
  container.template register_type<scope<shared>, storage<Processor<0>>,
                                   interfaces<IProcessor>>();
  container.template register_type<scope<shared>, storage<Processor<1>>,
                                   interfaces<IProcessor>>();

  // Resolve the collection
  container.template resolve<std::vector<IProcessor *>>();
  ////
}
