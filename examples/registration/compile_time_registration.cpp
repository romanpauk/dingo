//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <cassert>
#include <memory>

////
class settings {
public:
  int seed() const { return seed_; }

private:
  int seed_ = 7;
};

struct cache {
  explicit cache(settings &cfg) : seed(cfg.seed()) {}

  int seed;
};

struct job {
  job(settings &cfg, cache &shared_cache)
      : total(cfg.seed() + shared_cache.seed) {}

  int total;
};

struct report {
  report(settings &cfg, job transient_job)
      : total(cfg.seed() + transient_job.total) {}

  int total;
};

void compile_time_registration_example() {
  using namespace dingo;
  using app_bindings = dingo::bindings<
      dingo::bind<scope<shared>, storage<std::shared_ptr<settings>>>,
      dingo::bind<scope<shared>, storage<std::unique_ptr<cache>>>,
      dingo::bind<scope<unique>, storage<job>>>;

  container<app_bindings> container;

  [[maybe_unused]] auto &cfg = container.resolve<settings &>();
  [[maybe_unused]] auto &cfg_handle =
      container.resolve<std::shared_ptr<settings> &>();
  assert(&cfg == cfg_handle.get());

  assert(container.resolve<cache *>()->seed == 7);
  assert(container.resolve<job>().total == 14);
  assert(container.construct<report>().total == 21);

  static_container<app_bindings> static_only;
  assert(static_only.resolve<job>().total == 14);
}
////

int main() {
  compile_time_registration_example();
  return 0;
}
