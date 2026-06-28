// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/index/index.h>
#include <dingo/storage/shared.h>

#include <cstddef>

namespace index_type {
struct invalid_backend {};
} // namespace index_type

struct animal {
  virtual ~animal() = default;
};

struct dog : animal {};

namespace dingo::detail {
template <typename Key, typename Value, typename Allocator>
struct index_storage<::index_type::invalid_backend, Key, Value, Allocator> {
  index_storage(Allocator &) {}

  void emplace(Key, Value) {}
  Value find(const Key &) { return {}; }
};
} // namespace dingo::detail

struct traits : dingo::dynamic_container_traits {
  using index_definition_type = dingo::indexes<
      dingo::index<animal, std::size_t, index_type::invalid_backend>>;
};

int main() {
  dingo::container<traits> container;
  container
      .register_indexed_type<dingo::scope<dingo::shared>, dingo::storage<dog>,
                             dingo::interfaces<animal>>(std::size_t(1));
}

// CHECK: index backend must provide bool emplace(Key, Value) and Value*
// find(Key)
