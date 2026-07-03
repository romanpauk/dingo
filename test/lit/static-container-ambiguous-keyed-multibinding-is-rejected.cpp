// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <memory>

struct duplicate_key : std::integral_constant<int, 0> {};

struct interface {
  virtual ~interface() = default;
};

struct first : interface {};
struct second : interface {};

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>,
                dingo::storage<std::shared_ptr<first>>,
                dingo::interfaces<interface>, dingo::key_type<duplicate_key>>,
    dingo::bind<dingo::scope<dingo::shared>,
                dingo::storage<std::shared_ptr<second>>,
                dingo::interfaces<interface>, dingo::key_type<duplicate_key>>>;

void test() {
  dingo::static_container<source> container;
  (void)container.resolve<std::shared_ptr<interface>>(
      dingo::key_type<duplicate_key>{});
}

// CHECK: static_container cannot resolve an ambiguously bound type
