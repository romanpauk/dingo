// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

struct a {};
struct b {};
struct runtime_only {};

using source =
    dingo::bindings<dingo::bind<dingo::scope<dingo::shared>, dingo::storage<a>,
                                dingo::dependencies<b &, runtime_only &>>,
                    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<b>,
                                dingo::dependencies<a &>>>;

int main() {
  dingo::container<source> instance;
  (void)instance;
}

// CHECK: container requires a resolvable compile-time binding graph
