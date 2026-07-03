//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <string>
#include <type_traits>

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
  // Declare traits with a std::string based lookup
  struct container_traits : dynamic_container_traits {
    using lookup_definition_type = lookups<associative<std::string, IAnimal>>;
  };

  container<container_traits> container;
  container
      .template register_type<scope<shared>, storage<Dog>, interfaces<IAnimal>>(
          dingo::key_value{std::string("dog")});

  container
      .template register_type<scope<shared>, storage<Cat>, interfaces<IAnimal>>(
          dingo::key_value{std::string("cat")});

  // Resolve an instance of a dog
  auto dog = container.template resolve<IAnimal>(std::string("dog"));
  ////
  (void)dog;
}
