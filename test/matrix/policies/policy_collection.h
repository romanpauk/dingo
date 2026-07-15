//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include "matrix/policies/policy_core.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace dingo::matrix::resolution {

template <typename Collection> struct collection {
  template <typename Container> static void check(Container &container) {
    auto elements = container.template resolve<Collection>();
    std::vector<int> ids;
    ids.reserve(elements.size());
    for (auto &element : elements) {
      ids.push_back(element->id());
    }
    std::sort(ids.begin(), ids.end());
    ASSERT_EQ(ids, (std::vector<int>{0, 1}));
  }
};

template <typename Collection> struct construct_collection {
  template <typename Container> static void check(Container &container) {
    auto elements = container.template construct_collection<Collection>(
        [](auto &collection, auto &&value) {
          collection.emplace(value->id(), std::move(value));
        });
    ASSERT_EQ(elements.size(), 2u);
    ASSERT_EQ(elements.at(0)->id(), 0);
    ASSERT_EQ(elements.at(1)->id(), 1);
  }
};

template <typename Collection> struct construct_collection_values {
  template <typename Container> static void check(Container &container) {
    auto elements = container.template construct_collection<Collection>();
    std::vector<int> ids;
    ids.reserve(elements.size());
    for (auto &element : elements) {
      ids.push_back(element->id());
    }
    std::sort(ids.begin(), ids.end());
    ASSERT_EQ(ids, (std::vector<int>{0, 1}));
  }
};

template <typename Interface, typename FirstKey, typename SecondKey>
struct keyed_shared_ptr_refs {
  template <typename Container> static void check(Container &container) {
    auto &first =
        container.template resolve<std::shared_ptr<Interface> &>(FirstKey{});
    auto &second =
        container.template resolve<std::shared_ptr<Interface> &>(SecondKey{});
    ASSERT_EQ(first->id(), 0);
    ASSERT_EQ(second->id(), 1);
  }
};

template <typename Type, typename Key> struct keyed_value {
  template <typename Container> static void check(Container &container) {
    auto instance = container.template resolve<Type>(Key{});
    ASSERT_TRUE(is_constructed_value(instance));
  }
};

template <typename Type, typename Key> struct keyed_ref {
  template <typename Container> static void check(Container &container) {
    auto &instance = container.template resolve<Type &>(Key{});
    ASSERT_TRUE(is_constructed_value(instance));
  }
};

template <typename Collection, typename Key> struct keyed_collection {
  template <typename Container> static void check(Container &container) {
    auto elements = container.template resolve<Collection>(Key{});
    std::vector<int> ids;
    ids.reserve(elements.size());
    for (auto &element : elements) {
      ids.push_back(element->id());
    }
    std::sort(ids.begin(), ids.end());
    ASSERT_EQ(ids, (std::vector<int>{0, 1}));
  }
};

template <typename Interface> struct indexed_shared_ptrs {
  template <typename Container> static void check(Container &container) {
    auto first =
        container.template resolve<std::shared_ptr<Interface>>(std::size_t{0});
    auto second =
        container.template resolve<std::shared_ptr<Interface>>(std::size_t{1});
    ASSERT_EQ(first->id(), 0);
    ASSERT_EQ(second->id(), 1);
  }
};

} // namespace dingo::matrix::resolution
