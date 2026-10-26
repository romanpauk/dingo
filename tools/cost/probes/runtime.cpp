// A runtime-only compile-cost probe. The static probe also compiles runtime
// paths, so a runtime layer change is otherwise measured only through a mixed
// translation unit. Keep this file free of static bindings and of a main.

#include <dingo/container.h>
#include <dingo/factory/function.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

using namespace dingo;

namespace {

struct config {
  int value;
};

config make_config() { return {7}; }

struct consumer {
  explicit consumer(config &cfg) : value(cfg.value) {}

  int read() const { return value; }

  int value;
};

struct iface {
  virtual ~iface() = default;
  virtual int read() const = 0;
};

template <int Value> struct iface_impl : iface {
  int read() const override { return Value; }
};

struct external_config {
  int value;
};

external_config make_external_config() { return {13}; }

template <typename Container>
void register_shared_config(Container &container) {
  container.template register_type<scope<shared>, storage<config>,
                                   factory<function<make_config>>>();
}

template <typename Container> void register_consumer(Container &container) {
  register_shared_config(container);
  container.template register_type<scope<shared>, storage<consumer>>();
}

template <typename Container> void register_interface(Container &container) {
  container.template register_type<scope<shared>,
                                   storage<std::shared_ptr<iface_impl<11>>>,
                                   interfaces<iface>>();
}

template <typename Container> void register_collection(Container &container) {
  container.template register_type<scope<shared>,
                                   storage<std::shared_ptr<iface_impl<3>>>,
                                   interfaces<iface>>();
  container.template register_type<scope<shared>,
                                   storage<std::shared_ptr<iface_impl<5>>>,
                                   interfaces<iface>>();
}

template <typename Container> void register_external(Container &container) {
  container.template register_type<scope<external>, storage<external_config>>(
      make_external_config());
}

template <typename Container> void register_unique(Container &container) {
  container.template register_type<scope<unique>, storage<config>,
                                   factory<function<make_config>>>();
}

} // namespace

extern "C" [[gnu::noinline]] int probe_runtime_singular_reference() {
  container<> runtime;
  register_shared_config(runtime);
  return runtime.resolve<config &>().value;
}

extern "C" [[gnu::noinline]] int probe_runtime_singular_consumer() {
  container<> runtime;
  register_consumer(runtime);
  return runtime.resolve<consumer &>().read();
}

extern "C" [[gnu::noinline]] int probe_runtime_construct_shared_value() {
  container<> runtime;
  register_shared_config(runtime);
  return runtime.construct<config>().value;
}

extern "C" [[gnu::noinline]] int probe_runtime_construct_shared_handle() {
  container<> runtime;
  register_shared_config(runtime);
  return runtime.construct<std::shared_ptr<config>>()->value;
}

extern "C" [[gnu::noinline]] int probe_runtime_construct_optional() {
  container<> runtime;
  register_shared_config(runtime);
  return runtime.construct<std::optional<config>>()->value;
}

extern "C" [[gnu::noinline]] int probe_runtime_construct_unique() {
  container<> runtime;
  register_unique(runtime);
  return runtime.construct<std::unique_ptr<config>>()->value;
}

extern "C" [[gnu::noinline]] int probe_runtime_interface() {
  container<> runtime;
  register_interface(runtime);
  return runtime.resolve<iface &>().read();
}

extern "C" [[gnu::noinline]] int probe_runtime_collection() {
  container<> runtime;
  register_collection(runtime);
  return static_cast<int>(
      runtime.construct_collection<std::vector<iface *>>().size());
}

extern "C" [[gnu::noinline]] int probe_runtime_external() {
  container<> runtime;
  register_external(runtime);
  return runtime.resolve<external_config &>().value;
}

extern "C" [[gnu::noinline]] int probe_runtime_registration() {
  container<> runtime;
  register_consumer(runtime);
  register_interface(runtime);
  register_collection(runtime);
  register_external(runtime);
  register_unique(runtime);
  return runtime.resolve<config &>().value;
}
