//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Keyed lookups through the public API of the next implementation: the
// domains a lookup declaration holds one or many bindings in, by the key
// values its backend indexes, and the allocator of the container, which
// allocates the bindings. spec: lookup_declarations, index_structures

#include <dingo/container.h>
#include <dingo/lookup/lookup.h>
#include <dingo/storage/shared.h>
#include <dingo/type/collection_traits.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using dingo::interfaces;
using dingo::scope;
using dingo::shared;
using dingo::storage;

struct processor {
  virtual ~processor() = default;
  virtual int id() const = 0;
};

template <int Id> struct impl : processor {
  int id() const override { return Id; }
};

struct large_impl : processor {
  char payload[1024] = {};
  int id() const override { return 7; }
};

// An implementation that counts its constructions.
struct counted : processor {
  counted() { ++constructions; }
  int id() const override { return 42; }
  inline static int constructions = 0;
};

struct processor_key {};

template <typename Range> std::vector<int> ids(const Range &range) {
  std::vector<int> result;
  for (processor *each : range) {
    result.push_back(each->id());
  }
  return result;
}

// A key type compared by value, of which equal objects are distinct.
struct comparable_key {
  int value;
  bool operator==(const comparable_key &other) const {
    return value == other.value;
  }
  bool operator<(const comparable_key &other) const {
    return value < other.value;
  }
};

// An allocator that counts what it allocates.
struct allocation_state {
  int allocations = 0;
  int deallocations = 0;
  std::size_t largest = 0;
};

template <typename T> class counting_allocator {
public:
  using value_type = T;

  explicit counting_allocator(allocation_state *state = nullptr)
      : state_(state) {}

  template <typename U>
  counting_allocator(const counting_allocator<U> &other) noexcept
      : state_(other.state()) {}

  T *allocate(std::size_t count) {
    if (state_ != nullptr) {
      ++state_->allocations;
      if (count * sizeof(T) > state_->largest) {
        state_->largest = count * sizeof(T);
      }
    }
    return static_cast<T *>(::operator new(count * sizeof(T)));
  }

  void deallocate(T *pointer, std::size_t /*count*/) noexcept {
    if (state_ != nullptr) {
      ++state_->deallocations;
    }
    ::operator delete(pointer);
  }

  allocation_state *state() const { return state_; }

private:
  allocation_state *state_;
};

template <typename T, typename U>
bool operator==(const counting_allocator<T> &left,
                const counting_allocator<U> &right) noexcept {
  return left.state() == right.state();
}

template <typename T, typename U>
bool operator!=(const counting_allocator<T> &left,
                const counting_allocator<U> &right) noexcept {
  return !(left == right);
}

template <typename Traits>
using allocating_container = dingo::container<Traits, counting_allocator<char>>;

// A collection of the user that records the size it is reserved for.
template <typename T> struct tracked_collection {
  std::vector<T> values;
  std::size_t reserved = 0;
};

} // namespace

template <typename T> struct dingo::collection_traits<tracked_collection<T>> {
  static const bool is_collection = true;
  static const bool has_fixed_size_construct = false;
  using resolve_type = T;

  static void reserve(tracked_collection<T> &collection, std::size_t size) {
    collection.reserved = size;
    collection.values.reserve(size);
  }

  template <typename U>
  static void add(tracked_collection<T> &collection, U &&value) {
    collection.values.emplace_back(std::forward<U>(value));
  }
};

namespace {

TEST(next_lookups, a_binding_is_allocated_from_the_container_allocator) {
  for (bool keyed : {false, true}) {
    allocation_state state;
    {
      allocating_container<dingo::dynamic_container_traits> container{
          counting_allocator<char>(&state)};
      if (keyed) {
        container.register_type<scope<shared>, storage<large_impl>,
                                interfaces<processor>,
                                dingo::key_type<processor_key>>();
        EXPECT_EQ(
            container.resolve<processor &>(dingo::key_type<processor_key>())
                .id(),
            7);
      } else {
        container.register_type<scope<shared>, storage<large_impl>,
                                interfaces<processor>>();
        EXPECT_EQ(container.resolve<processor &>().id(), 7);
      }
      EXPECT_GT(state.allocations, 0) << "keyed " << keyed;
      EXPECT_GE(state.largest, sizeof(large_impl)) << "keyed " << keyed;
    }
    EXPECT_EQ(state.deallocations, state.allocations) << "keyed " << keyed;
  }
}

TEST(next_lookups, resolving_a_shared_binding_allocates_nothing_more) {
  struct service {};
  allocation_state state;
  {
    allocating_container<dingo::dynamic_container_traits> container{
        counting_allocator<char>(&state)};
    container.register_type<scope<shared>, storage<service>>();
    const int registered = state.allocations;
    (void)container.resolve<service &>();
    (void)container.resolve<service &>();
    EXPECT_EQ(state.allocations, registered);
  }
  EXPECT_EQ(state.deallocations, state.allocations);
}

template <typename Key>
void check_second_binding_rejected(dingo::container<> &container,
                                   const Key &key) {
  container.register_type<scope<shared>, storage<counted>,
                          interfaces<processor>, Key>();
  processor &before = container.resolve<processor &>(key);
  EXPECT_THROW((container.register_type<scope<shared>, storage<counted>,
                                        interfaces<processor>, Key>()),
               dingo::lookup_already_registered_exception);
  EXPECT_THROW((container.register_type<scope<shared>, storage<impl<2>>,
                                        interfaces<processor>, Key>()),
               dingo::lookup_already_registered_exception);
  EXPECT_EQ(&container.resolve<processor &>(key), &before);
  auto collected = container.resolve<tracked_collection<processor *>>(key);
  ASSERT_EQ(collected.values.size(), 1u);
  EXPECT_EQ(collected.values[0], &before);
}

TEST(next_lookups, a_second_binding_of_a_key_of_a_domain_of_one_is_rejected) {
  counted::constructions = 0;
  dingo::container<> unkeyed;
  check_second_binding_rejected(unkeyed, dingo::key_type<dingo::none_t>());
  dingo::container<> tagged;
  check_second_binding_rejected(tagged, dingo::key_type<processor_key>());
  EXPECT_EQ(counted::constructions, 2);
}

// The baseline reserves a collection for the bindings it gathers, which the
// next implementation does not do; see the ledger finding.
TEST(next_lookups, DISABLED_a_collection_is_reserved_for_its_bindings) {
  struct traits : dingo::dynamic_container_traits {
    using lookup_definition_type =
        dingo::lookups<dingo::typed<processor_key, processor, dingo::many>>;
  };
  dingo::container<traits> container;
  container
      .register_type<scope<shared>, storage<impl<1>>, interfaces<processor>,
                     dingo::key_type<processor_key>>();
  container
      .register_type<scope<shared>, storage<impl<2>>, interfaces<processor>,
                     dingo::key_type<processor_key>>();
  auto collected = container.resolve<tracked_collection<processor *>>(
      dingo::key_type<processor_key>());
  EXPECT_EQ(collected.reserved, 2u);
  EXPECT_EQ(ids(collected.values), (std::vector<int>{1, 2}));
}

TEST(next_lookups, a_typed_domain_of_many_fills_a_collection_of_the_user) {
  struct traits : dingo::dynamic_container_traits {
    using lookup_definition_type =
        dingo::lookups<dingo::typed<processor_key, processor, dingo::many>>;
  };
  dingo::container<traits> container;
  container
      .register_type<scope<shared>, storage<impl<1>>, interfaces<processor>,
                     dingo::key_type<processor_key>>();
  container
      .register_type<scope<shared>, storage<impl<2>>, interfaces<processor>,
                     dingo::key_type<processor_key>>();
  auto collected = container.resolve<tracked_collection<processor *>>(
      dingo::key_type<processor_key>());
  EXPECT_EQ(ids(collected.values), (std::vector<int>{1, 2}));
}

TEST(next_lookups, a_collection_of_a_declared_domain_of_one_holds_its_binding) {
  counted::constructions = 0;
  struct single_traits : dingo::dynamic_container_traits {
    using lookup_definition_type = dingo::lookups<dingo::single<processor>>;
  };
  dingo::container<single_traits> single;
  single
      .register_type<scope<shared>, storage<counted>, interfaces<processor>>();
  processor &before = single.resolve<processor &>();
  EXPECT_THROW((single.register_type<scope<shared>, storage<counted>,
                                     interfaces<processor>>()),
               dingo::lookup_already_registered_exception);
  EXPECT_EQ(&single.resolve<processor &>(), &before);
  auto collected = single.resolve<tracked_collection<processor *>>();
  ASSERT_EQ(collected.values.size(), 1u);
  EXPECT_EQ(collected.values[0], &before);
  EXPECT_EQ(counted::constructions, 1);

  struct value_traits : dingo::dynamic_container_traits {
    using lookup_definition_type =
        dingo::lookups<dingo::associative<std::size_t, processor>>;
  };
  dingo::container<value_traits> by_value;
  by_value
      .register_type<scope<shared>, storage<impl<1>>, interfaces<processor>>(
          dingo::key_value{std::size_t(7)});
  EXPECT_EQ(by_value.resolve<processor &>(std::size_t(7)).id(), 1);
  EXPECT_THROW((by_value.register_type<scope<shared>, storage<impl<2>>,
                                       interfaces<processor>>(
                   dingo::key_value{std::size_t(7)})),
               dingo::lookup_already_registered_exception);
  EXPECT_EQ(ids(by_value.resolve<std::vector<processor *>>(std::size_t(7))),
            (std::vector<int>{1}));
  EXPECT_TRUE(
      by_value.resolve<std::vector<processor *>>(std::size_t(8)).empty());
}

TEST(next_lookups, a_key_value_is_found_by_an_equal_distinct_object) {
  counted::constructions = 0;
  struct traits : dingo::dynamic_container_traits {
    using lookup_definition_type =
        dingo::lookups<dingo::associative<comparable_key, processor>>;
  };
  dingo::container<traits> container;
  comparable_key registered{7};
  container
      .register_type<scope<shared>, storage<counted>, interfaces<processor>>(
          dingo::key_value{registered});
  comparable_key first_key{7};
  comparable_key second_key{7};
  processor &first = container.resolve<processor &>(first_key);
  processor &second = container.resolve<processor &>(second_key);
  EXPECT_EQ(&first, &second);
  EXPECT_EQ(first.id(), 42);
  EXPECT_EQ(counted::constructions, 1);
  EXPECT_THROW(container.resolve<processor &>(comparable_key{8}),
               dingo::type_not_found_exception);
}

template <typename Key, typename Backend>
struct many_by_value_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<Key, processor, dingo::many, Backend>>;
};

template <typename Key, typename Backend> void check_many_by_value() {
  dingo::container<many_by_value_traits<Key, Backend>> container;
  container.template register_type<scope<shared>, storage<impl<1>>,
                                   interfaces<processor>>(
      dingo::key_value{Key{7}});
  container.template register_type<scope<shared>, storage<impl<2>>,
                                   interfaces<processor>>(
      dingo::key_value{Key{7}});
  container.template register_type<scope<shared>, storage<impl<3>>,
                                   interfaces<processor>>(
      dingo::key_value{Key{8}});
  EXPECT_EQ(ids(container.template resolve<std::vector<processor *>>(Key{7})),
            (std::vector<int>{1, 2}));
  EXPECT_THROW(container.template resolve<processor &>(Key{7}),
               dingo::type_ambiguous_exception);
  EXPECT_EQ(container.template resolve<processor &>(Key{8}).id(), 3);
  EXPECT_THROW(container.template resolve<processor &>(Key{9}),
               dingo::type_not_found_exception);
  EXPECT_TRUE(
      container.template resolve<std::vector<processor *>>(Key{9}).empty());
}

TEST(next_lookups, a_domain_of_many_holds_every_binding_of_a_key_in_order) {
  check_many_by_value<comparable_key, dingo::ordered>();
  check_many_by_value<std::size_t, dingo::ordered>();
  check_many_by_value<std::size_t, dingo::unordered>();
  check_many_by_value<std::size_t, dingo::array<16>>();
}

TEST(next_lookups, an_unordered_domain_of_one_selects_by_key_value) {
  struct traits : dingo::dynamic_container_traits {
    using lookup_definition_type =
        dingo::lookups<dingo::associative<std::size_t, processor, dingo::one,
                                          dingo::unordered>>;
  };
  dingo::container<traits> container;
  container
      .register_type<scope<shared>, storage<impl<1>>, interfaces<processor>>(
          dingo::key_value{std::size_t(7)});
  processor &first = container.resolve<processor &>(std::size_t(7));
  EXPECT_EQ(first.id(), 1);
  EXPECT_THROW((container.register_type<scope<shared>, storage<impl<2>>,
                                        interfaces<processor>>(
                   dingo::key_value{std::size_t(7)})),
               dingo::lookup_already_registered_exception);
  EXPECT_EQ(&container.resolve<processor &>(std::size_t(7)), &first);
  container
      .register_type<scope<shared>, storage<impl<2>>, interfaces<processor>>(
          dingo::key_value{std::size_t(8)});
  EXPECT_EQ(container.resolve<processor &>(std::size_t(8)).id(), 2);
  EXPECT_EQ(container.resolve<processor &>(std::size_t(7)).id(), 1);
}

TEST(next_lookups, a_dense_array_domain_of_one_keeps_its_bindings_on_a_reject) {
  struct traits : dingo::dynamic_container_traits {
    using lookup_definition_type =
        dingo::lookups<dingo::associative<std::size_t, processor, dingo::one,
                                          dingo::array<1>>>;
  };
  dingo::container<traits> container;
  EXPECT_THROW((container.register_type<scope<shared>, storage<impl<2>>,
                                        interfaces<processor>>(
                   dingo::key_value{std::size_t(1)})),
               dingo::type_index_out_of_range_exception);
  EXPECT_THROW(container.resolve<processor &>(std::size_t(1)),
               dingo::type_not_found_exception);
  container
      .register_type<scope<shared>, storage<impl<1>>, interfaces<processor>>(
          dingo::key_value{std::size_t(0)});
  EXPECT_EQ(container.resolve<processor &>(std::size_t(0)).id(), 1);
  EXPECT_THROW((container.register_type<scope<shared>, storage<impl<2>>,
                                        interfaces<processor>>(
                   dingo::key_value{std::size_t(1)})),
               dingo::type_index_out_of_range_exception);
  EXPECT_EQ(container.resolve<processor &>(std::size_t(0)).id(), 1);
  EXPECT_THROW(container.resolve<processor &>(std::size_t(1)),
               dingo::type_not_found_exception);
}

TEST(next_lookups,
     a_dense_array_domain_of_many_keeps_its_bindings_on_a_reject) {
  struct traits : dingo::dynamic_container_traits {
    using lookup_definition_type =
        dingo::lookups<dingo::associative<std::size_t, processor, dingo::many,
                                          dingo::array<4>>>;
  };
  dingo::container<traits> container;
  EXPECT_THROW((container.register_type<scope<shared>, storage<impl<3>>,
                                        interfaces<processor>>(
                   dingo::key_value{std::size_t(4)})),
               dingo::type_index_out_of_range_exception);
  EXPECT_TRUE(
      container.resolve<std::vector<processor *>>(std::size_t(4)).empty());
  container
      .register_type<scope<shared>, storage<impl<1>>, interfaces<processor>>(
          dingo::key_value{std::size_t(2)});
  container
      .register_type<scope<shared>, storage<impl<2>>, interfaces<processor>>(
          dingo::key_value{std::size_t(2)});
  EXPECT_THROW(container.resolve<processor &>(std::size_t(2)),
               dingo::type_ambiguous_exception);
  container
      .register_type<scope<shared>, storage<impl<1>>, interfaces<processor>>(
          dingo::key_value{std::size_t(2)});
  EXPECT_EQ(ids(container.resolve<std::vector<processor *>>(std::size_t(2))),
            (std::vector<int>{1, 2, 1}));
  container
      .register_type<scope<shared>, storage<impl<1>>, interfaces<processor>>(
          dingo::key_value{std::size_t(3)});
  EXPECT_EQ(container.resolve<processor &>(std::size_t(3)).id(), 1);
  EXPECT_THROW((container.register_type<scope<shared>, storage<impl<3>>,
                                        interfaces<processor>>(
                   dingo::key_value{std::size_t(4)})),
               dingo::type_index_out_of_range_exception);
  EXPECT_EQ(ids(container.resolve<std::vector<processor *>>(std::size_t(2))),
            (std::vector<int>{1, 2, 1}));
  EXPECT_TRUE(
      container.resolve<std::vector<processor *>>(std::size_t(4)).empty());
}

TEST(next_lookups, a_key_outside_a_dense_array_names_its_key_type) {
  struct traits : dingo::dynamic_container_traits {
    using lookup_definition_type = dingo::lookups<
        dingo::associative<int, processor, dingo::one, dingo::array<2>>>;
  };
  dingo::container<traits> container;
  try {
    container
        .register_type<scope<shared>, storage<impl<1>>, interfaces<processor>>(
            dingo::key_value{-1});
    FAIL() << "a negative key is outside the dense array";
  } catch (const dingo::type_index_out_of_range_exception &error) {
    EXPECT_STREQ(error.what(), "type index out of range: key type int");
  }
}

// A backend of the user that names a map as its storage, and counts how the
// map is used.
struct map_use {
  int constructions = 0;
  int finds = 0;
  int emplaces = 0;
  int subscripts = 0;
};

inline map_use map_uses;

struct counting_backend {
  template <typename Key, typename Value, typename Cardinality,
            typename Allocator>
  class storage {
    using map_type =
        std::conditional_t<std::is_same_v<Cardinality, dingo::one>,
                           std::map<Key, Value>, std::multimap<Key, Value>>;

  public:
    using iterator = typename map_type::iterator;
    using const_iterator = typename map_type::const_iterator;

    storage() { ++map_uses.constructions; }

    iterator find(const Key &key) {
      ++map_uses.finds;
      return values_.find(key);
    }
    const_iterator find(const Key &key) const {
      ++map_uses.finds;
      return values_.find(key);
    }
    iterator end() { return values_.end(); }
    const_iterator end() const { return values_.end(); }

    template <typename... Args> auto emplace(Args &&...args) {
      ++map_uses.emplaces;
      return values_.emplace(std::forward<Args>(args)...);
    }

    Value &operator[](const Key &key) {
      ++map_uses.subscripts;
      return values_[key];
    }

  private:
    map_type values_;
  };
};

TEST(next_lookups, a_map_of_the_user_indexes_its_domain) {
  struct traits : dingo::dynamic_container_traits {
    using lookup_definition_type = dingo::lookups<
        dingo::associative<int, processor, dingo::one, counting_backend>>;
  };
  map_uses = {};
  dingo::container<traits> container;
  container
      .register_type<scope<shared>, storage<impl<1>>, interfaces<processor>>(
          dingo::key_value{7});
  container
      .register_type<scope<shared>, storage<impl<2>>, interfaces<processor>>(
          dingo::key_value{8});
  EXPECT_EQ(container.resolve<processor &>(7).id(), 1);
  EXPECT_EQ(container.resolve<processor &>(8).id(), 2);
  EXPECT_THROW(container.resolve<processor &>(9),
               dingo::type_not_found_exception);
  EXPECT_GT(map_uses.constructions, 0);
  EXPECT_GT(map_uses.emplaces, 0);
  EXPECT_GE(map_uses.finds, 3);
  EXPECT_EQ(map_uses.subscripts, 0);
}

TEST(next_lookups, a_default_declaration_names_the_backend_of_other_domains) {
  struct traits : dingo::dynamic_container_traits {
    using lookup_definition_type =
        dingo::lookups<dingo::base<dingo::one, counting_backend>>;
  };
  map_uses = {};
  dingo::container<traits> container;
  container
      .register_type<scope<shared>, storage<impl<3>>, interfaces<processor>>();
  container
      .register_type<scope<shared>, storage<impl<4>>, interfaces<processor>>(
          dingo::key_value{7});
  EXPECT_EQ(container.resolve<processor &>().id(), 3);
  EXPECT_EQ(map_uses.finds, 0);
  EXPECT_EQ(container.resolve<processor &>(7).id(), 4);
  EXPECT_GT(map_uses.constructions, 0);
  EXPECT_GT(map_uses.finds, 0);
}

// The baseline searches the index of each container of a chain of parents
// once for a request; the next implementation searches an ancestor again for
// each container between it and the request (here the grandparent three
// times and the parent twice); see the ledger finding.
TEST(next_lookups, DISABLED_a_parent_fallback_searches_each_container_once) {
  struct traits : dingo::dynamic_container_traits {
    using lookup_definition_type = dingo::lookups<
        dingo::associative<int, processor, dingo::one, counting_backend>>;
  };
  using grandparent_type = dingo::container<traits>;
  using parent_type =
      dingo::container<traits, std::allocator<char>, grandparent_type>;
  using child_type =
      dingo::container<traits, std::allocator<char>, parent_type>;
  grandparent_type grandparent;
  parent_type parent(&grandparent);
  child_type child(&parent);
  grandparent
      .register_type<scope<shared>, storage<impl<1>>, interfaces<processor>>(
          dingo::key_value{7});
  processor &first = child.resolve<processor &>(7);
  map_uses = {};
  EXPECT_EQ(&child.resolve<processor &>(7), &first);
  EXPECT_EQ(map_uses.finds, 3);
}

template <typename Cardinality, typename Backend>
struct default_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::base<Cardinality, Backend>>;
};

template <typename Backend> void check_default_of_one() {
  struct typed_processor : processor {};
  struct typed_impl : typed_processor {
    int id() const override { return 2; }
  };
  dingo::container<default_traits<dingo::one, Backend>> container;
  container.template register_type<scope<shared>, storage<impl<1>>,
                                   interfaces<processor>>();
  container.template register_type<scope<shared>, storage<typed_impl>,
                                   interfaces<typed_processor>,
                                   dingo::key_type<processor_key>>();
  EXPECT_EQ(container.template resolve<processor &>().id(), 1);
  EXPECT_EQ(
      container
          .template resolve<typed_processor &>(dingo::key_type<processor_key>())
          .id(),
      2);
  EXPECT_EQ(ids(container.template resolve<std::vector<processor *>>()),
            (std::vector<int>{1}));
  std::vector<int> typed;
  for (typed_processor *each :
       container.template resolve<std::vector<typed_processor *>>(
           dingo::key_type<processor_key>())) {
    typed.push_back(each->id());
  }
  EXPECT_EQ(typed, (std::vector<int>{2}));
  EXPECT_THROW(
      (container.template register_type<scope<shared>, storage<impl<3>>,
                                        interfaces<processor>>()),
      dingo::lookup_already_registered_exception);
}

TEST(next_lookups, a_default_declaration_of_one_holds_a_binding_of_each_key) {
  check_default_of_one<dingo::ordered>();
  check_default_of_one<dingo::unordered>();
}

template <typename Backend> void check_default_of_many() {
  dingo::container<default_traits<dingo::many, Backend>> container;
  container.template register_type<scope<shared>, storage<impl<1>>,
                                   interfaces<processor>>();
  container.template register_type<scope<shared>, storage<impl<2>>,
                                   interfaces<processor>>();
  container.template register_type<scope<shared>, storage<impl<3>>,
                                   interfaces<processor>,
                                   dingo::key_type<processor_key>>();
  container.template register_type<scope<shared>, storage<impl<4>>,
                                   interfaces<processor>,
                                   dingo::key_type<processor_key>>();
  container.template register_type<scope<shared>, storage<impl<5>>,
                                   interfaces<processor>>(dingo::key_value{7});
  container.template register_type<scope<shared>, storage<impl<6>>,
                                   interfaces<processor>>(dingo::key_value{7});
  EXPECT_EQ(ids(container.template resolve<std::vector<processor *>>()),
            (std::vector<int>{1, 2}));
  EXPECT_EQ(ids(container.template resolve<std::vector<processor *>>(
                dingo::key_type<processor_key>())),
            (std::vector<int>{3, 4}));
  EXPECT_EQ(ids(container.template resolve<std::vector<processor *>>(7)),
            (std::vector<int>{5, 6}));
  EXPECT_THROW(container.template resolve<processor &>(),
               dingo::type_ambiguous_exception);
  EXPECT_THROW(
      container.template resolve<processor &>(dingo::key_type<processor_key>()),
      dingo::type_ambiguous_exception);
  EXPECT_THROW(container.template resolve<processor &>(7),
               dingo::type_ambiguous_exception);
}

TEST(next_lookups, a_default_declaration_of_many_holds_any_number_of_each_key) {
  check_default_of_many<dingo::ordered>();
  check_default_of_many<dingo::unordered>();
}

enum class processor_id { first, second };

template <processor_id Id> struct enum_consumer {
  explicit enum_consumer(
      dingo::dependency<processor &, dingo::key_type<processor_id, Id>>
          selected)
      : chosen(selected) {}
  processor &chosen;
};

TEST(next_lookups, injects_a_fixed_enumeration_key) {
  struct traits : dingo::dynamic_container_traits {
    using lookup_definition_type =
        dingo::lookups<dingo::associative<processor_id, processor>>;
  };
  dingo::container<traits> container;
  container
      .register_type<scope<shared>, storage<impl<1>>, interfaces<processor>>(
          dingo::key_value{processor_id::first});
  container
      .register_type<scope<shared>, storage<impl<2>>, interfaces<processor>>(
          dingo::key_value{processor_id::second});
  EXPECT_EQ(
      container.construct<enum_consumer<processor_id::first>>().chosen.id(), 1);
  EXPECT_EQ(
      container.construct<enum_consumer<processor_id::second>>().chosen.id(),
      2);
}

#if __cplusplus >= 202002L
// A structural string type, the value of a fixed key of std::string.
template <std::size_t N> struct fixed_string {
  char value[N];

  constexpr fixed_string(const char (&text)[N]) {
    for (std::size_t index = 0; index != N; ++index) {
      value[index] = text[index];
    }
  }

  constexpr operator std::string_view() const { return {value, N - 1}; }
};

struct string_consumer {
  explicit string_consumer(
      dingo::dependency<processor &,
                        dingo::key_type<std::string, fixed_string{"json"}>>
          selected)
      : chosen(selected) {}
  processor &chosen;
};

TEST(next_lookups, injects_a_fixed_string_key_into_a_string_domain) {
  struct traits : dingo::dynamic_container_traits {
    using lookup_definition_type =
        dingo::lookups<dingo::associative<std::string, processor>>;
  };
  dingo::container<traits> container;
  container
      .register_type<scope<shared>, storage<impl<1>>, interfaces<processor>>(
          dingo::key_value{std::string("xml")});
  container
      .register_type<scope<shared>, storage<impl<7>>, interfaces<processor>>(
          dingo::key_value{std::string("json")});
  EXPECT_EQ(container.construct<string_consumer>().chosen.id(), 7);
}
#endif

TEST(next_lookups, a_container_allocator_allocates_the_key_values_it_holds) {
  struct large_key {
    explicit large_key(int resolved) : value(resolved) {}
    bool operator==(const large_key &other) const {
      return value == other.value;
    }
    bool operator<(const large_key &other) const { return value < other.value; }
    int value;
    char padding[4096] = {};
  };
  struct string_traits : dingo::dynamic_container_traits {
    using lookup_definition_type =
        dingo::lookups<dingo::associative<std::string, processor>>;
  };
  struct large_traits : dingo::dynamic_container_traits {
    using lookup_definition_type =
        dingo::lookups<dingo::associative<large_key, processor>>;
  };
  allocation_state string_state;
  {
    allocating_container<string_traits> container{
        counting_allocator<char>(&string_state)};
    container
        .register_type<scope<shared>, storage<impl<11>>, interfaces<processor>>(
            dingo::key_value{std::string("dog")});
    EXPECT_EQ(container.resolve<processor &>(std::string("dog")).id(), 11);
    EXPECT_GT(string_state.allocations, 0);
  }
  EXPECT_EQ(string_state.deallocations, string_state.allocations);
  allocation_state large_state;
  {
    allocating_container<large_traits> container{
        counting_allocator<char>(&large_state)};
    container
        .register_type<scope<shared>, storage<impl<11>>, interfaces<processor>>(
            dingo::key_value{large_key{7}});
    EXPECT_EQ(container.resolve<processor &>(large_key{7}).id(), 11);
    EXPECT_GE(large_state.largest, sizeof(large_key));
  }
  EXPECT_EQ(large_state.deallocations, large_state.allocations);
  struct many_traits : dingo::dynamic_container_traits {
    using lookup_definition_type =
        dingo::lookups<dingo::associative<std::size_t, processor, dingo::many>>;
  };
  allocation_state many_state;
  {
    allocating_container<many_traits> container{
        counting_allocator<char>(&many_state)};
    container
        .register_type<scope<shared>, storage<impl<1>>, interfaces<processor>>(
            dingo::key_value{std::size_t(0)});
    container
        .register_type<scope<shared>, storage<impl<2>>, interfaces<processor>>(
            dingo::key_value{std::size_t(0)});
    EXPECT_EQ(ids(container.resolve<std::vector<processor *>>(std::size_t(0))),
              (std::vector<int>{1, 2}));
    EXPECT_GT(many_state.allocations, 0);
  }
  EXPECT_EQ(many_state.deallocations, many_state.allocations);
}

} // namespace
