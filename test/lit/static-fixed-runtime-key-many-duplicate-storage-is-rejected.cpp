// A domain declared many holds any number of bindings of a key, two of one
// implementation too, so the static container is formed and a collection of
// the key holds both.
// (behavior change duplicates_rejected_in_both_domains; the baseline rejected
// two bindings of one implementation and key)
// REQUIRES: linux, can-run-built-executables
// RUN: %dingo_next_cxx %s -o %t && %t

#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <cstddef>
#include <vector>

struct processor {
  virtual ~processor() = default;
};

struct processor_impl : processor {};

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<processor_impl>,
                dingo::interfaces<processor>, dingo::key_type<std::size_t, 0>>,
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<processor_impl>,
                dingo::interfaces<processor>,
                dingo::key_type<std::size_t, std::size_t{0}>>>;

struct traits : dingo::static_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, processor, dingo::many>>;
};

int main() {
  dingo::static_container<source, traits> container;
  auto all = container.construct_collection<std::vector<processor *>>(
      dingo::key_type<std::size_t, 0>{});
  return all.size() == 2 && all[0] != all[1] ? 0 : 1;
}
