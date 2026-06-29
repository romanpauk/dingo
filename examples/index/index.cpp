//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <string>

////
struct IAnimal {
  virtual ~IAnimal() = default;
};

struct Dog : IAnimal {};
struct Cat : IAnimal {};

////
int main() {
  using namespace dingo;
  ////
  // Declare traits with a std::string based selector
  struct container_traits : dynamic_container_traits {
    using index_definition_type = selectors<associative<IAnimal, std::string>>;
  };

  container<container_traits> container;
  container.template register_indexed_type<scope<shared>, storage<Dog>,
                                           interfaces<IAnimal>>(
      std::string("dog"));

  container.template register_indexed_type<scope<shared>, storage<Cat>,
                                           interfaces<IAnimal>>(
      std::string("cat"));

  // Resolve an instance of a dog
  auto dog = container.template resolve<IAnimal>(std::string("dog"));
  ////
  (void)dog;
}
