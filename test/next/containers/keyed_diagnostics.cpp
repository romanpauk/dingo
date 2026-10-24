//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The message of a request with a key that is not found while an object is
// constructed names the key and the path of the types being constructed.
// spec: diagnostics.type_not_found_keyed_required_by

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <string>

namespace {

using dingo::scope;
using dingo::storage;
using dingo::core::type_name_of;

struct missing {
  missing() {}
};

using key = dingo::key_type<int, 3>;

struct consumer {
  explicit consumer(dingo::dependency<missing &, key>) {}
};

TEST(next_diagnostics, names_the_key_and_the_path_of_a_missing_dependency) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<consumer>>();
  std::string message;
  try {
    (void)container.resolve<consumer &>();
  } catch (const dingo::type_not_found_exception &exception) {
    message = exception.what();
  }
  // A fixed key is named by its key type.
  EXPECT_EQ(message, "type not found: " + type_name_of<missing &>() +
                         " (index type: " + type_name_of<key>() +
                         ") (required by " + type_name_of<consumer>() + ")");
}

} // namespace
