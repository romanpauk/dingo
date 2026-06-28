// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

struct a {};
struct b {};

using source =
    dingo::bindings<dingo::bind<dingo::scope<dingo::shared>, dingo::storage<a>,
                                dingo::dependencies<b &>>,
                    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<b>,
                                dingo::dependencies<a &>>>;

int main() {
  dingo::static_container<source> instance;
  (void)instance;
}

// CHECK: static_container requires a resolvable compile-time binding graph
