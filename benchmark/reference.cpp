//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Resolvers written by hand, of no container: the least a resolution of a
// unique or a shared object costs, made by a virtual call, by a call through
// a pointer, kept in a pointer, or found in a map. The run suite of
// benchmark/nbench_conf.py measures these cases beside those of the
// container.

#include <nbench/bench.hpp>

#include <cstdlib>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <type_traits>
#include <typeindex>

namespace {

struct reference_factory {
  virtual ~reference_factory() {}
  virtual void *resolve() = 0;
};

template <typename T> struct reference_unique_factory : reference_factory {
  void *resolve() override { return make(); }
  void *make() { return new (&storage_) T; }

private:
  alignas(T) unsigned char storage_[sizeof(T)];
};

template <typename T> struct reference_shared_factory : reference_factory {
  void *resolve() override { return make(); }
  void *make() {
    if (!storage_) {
      storage_.emplace();
    }
    return &*storage_;
  }

private:
  std::optional<T> storage_;
};

template <typename Factory> void *call_factory(void *factory) {
  return static_cast<Factory *>(factory)->make();
}

// A value moved out of what a factory made, which is destroyed.
template <typename T> T take(void *made) {
  T *object = static_cast<T *>(made);
  T value = std::move(*object);
  object->~T();
  return value;
}

template <typename T, typename Factory> struct reference_resolver {
  reference_resolver() : factory_(std::make_unique<Factory>()) {}

  void *by_virtual() { return factory_->resolve(); }
  void *by_pointer() { return call_(factory_.get()); }
  void *kept() {
    if (kept_ == nullptr) {
      kept_ = call_(factory_.get());
    }
    return kept_;
  }
  void *mapped() {
    auto [found, made] = map_.try_emplace(std::type_index(typeid(T *)));
    if (made) {
      found->second = call_(factory_.get());
    }
    return found->second;
  }
  // A unique object is never in the map: the lookup is the cost.
  void *missed() {
    if (map_.find(std::type_index(typeid(T *))) != map_.end()) {
      std::abort();
    }
    return call_(factory_.get());
  }

private:
  std::unique_ptr<reference_factory> factory_;
  void *(*call_)(void *) = &call_factory<Factory>;
  void *kept_ = nullptr;
  std::map<std::type_index, void *> map_;
};

template <typename T, void *(reference_resolver<
                             T, reference_unique_factory<T>>::*Resolve)()>
void unique_resolve(nbench::State &state) {
  reference_resolver<T, reference_unique_factory<T>> resolver;
  while (state.keep_running()) {
    nbench::keep(take<T>((resolver.*Resolve)()));
  }
}

template <typename T, void *(reference_resolver<
                             T, reference_shared_factory<T>>::*Resolve)()>
void shared_resolve(nbench::State &state) {
  reference_resolver<T, reference_shared_factory<T>> resolver;
  while (state.keep_running()) {
    nbench::keep(*static_cast<T *>((resolver.*Resolve)()));
  }
}

template <typename T>
using unique_of = reference_resolver<T, reference_unique_factory<T>>;
template <typename T>
using shared_of = reference_resolver<T, reference_shared_factory<T>>;

NBENCH((unique_resolve<int, &unique_of<int>::by_virtual>),
       "reference/unique/virtual/int");
NBENCH((unique_resolve<int, &unique_of<int>::by_pointer>),
       "reference/unique/pointer/int");
NBENCH((unique_resolve<int, &unique_of<int>::missed>),
       "reference/unique/map/int");
NBENCH((unique_resolve<std::string, &unique_of<std::string>::by_virtual>),
       "reference/unique/virtual/string");
NBENCH((unique_resolve<std::string, &unique_of<std::string>::by_pointer>),
       "reference/unique/pointer/string");
NBENCH((unique_resolve<std::string, &unique_of<std::string>::missed>),
       "reference/unique/map/string");
NBENCH((shared_resolve<int, &shared_of<int>::by_virtual>),
       "reference/shared/virtual/int");
NBENCH((shared_resolve<int, &shared_of<int>::by_pointer>),
       "reference/shared/pointer/int");
NBENCH((shared_resolve<int, &shared_of<int>::kept>),
       "reference/shared/kept/int");
NBENCH((shared_resolve<int, &shared_of<int>::mapped>),
       "reference/shared/map/int");
NBENCH((shared_resolve<std::string, &shared_of<std::string>::by_virtual>),
       "reference/shared/virtual/string");
NBENCH((shared_resolve<std::string, &shared_of<std::string>::by_pointer>),
       "reference/shared/pointer/string");
NBENCH((shared_resolve<std::string, &shared_of<std::string>::kept>),
       "reference/shared/kept/string");
NBENCH((shared_resolve<std::string, &shared_of<std::string>::mapped>),
       "reference/shared/map/string");

} // namespace
