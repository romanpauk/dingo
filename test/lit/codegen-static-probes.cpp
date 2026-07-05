// REQUIRES: linux
// RUN: %dingo_cxx -O2 -c %s -o %t.o
// RUN: nm -S --size-sort %t.o | c++filt > %t.nm
// RUN: %python %dingo_lit_root/check_codegen_probe_sizes.py %t.nm
// RUN: objdump -d --no-show-raw-insn %t.o | c++filt > %t.objdump
// RUN: %python %dingo_lit_root/check_codegen_probe_instructions.py %t.objdump

#include <dingo/container.h>
#include <dingo/static_container.h>

#include <dingo/factory/function.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <memory>
#include <optional>
#include <type_traits>
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

using static_consumer_source = bindings<
    bind<scope<shared>, storage<config>, factory<function<make_config>>>,
    bind<scope<shared>, storage<consumer>, dependencies<config &>>>;

using static_wrapper_source = bindings<
    bind<scope<shared>, storage<config>, factory<function<make_config>>>>;

using static_unique_source = bindings<
    bind<scope<unique>, storage<config>, factory<function<make_config>>>>;

struct iface {
  virtual ~iface() = default;
  virtual int read() const = 0;
};

template <int Value> struct iface_impl : iface {
  int read() const override { return Value; }
};

using static_interface_source =
    bindings<bind<scope<shared>, storage<std::shared_ptr<iface_impl<11>>>,
                  interfaces<iface>>>;

using static_collection_source =
    bindings<bind<scope<shared>, storage<std::shared_ptr<iface_impl<3>>>,
                  interfaces<iface>>,
             bind<scope<shared>, storage<std::shared_ptr<iface_impl<5>>>,
                  interfaces<iface>>>;

struct external_config {
  int value;
};

external_config make_external_config() { return {13}; }

template <typename Container>
void register_runtime_bindings(Container &container) {
  container.template register_type<scope<shared>, storage<config>,
                                   factory<function<make_config>>>();
  container.template register_type<scope<shared>, storage<consumer>>();
}

template <typename Container>
void register_runtime_wrapper_binding(Container &container) {
  container.template register_type<scope<shared>, storage<config>,
                                   factory<function<make_config>>>();
}

template <typename Container>
void register_runtime_unique_binding(Container &container) {
  container.template register_type<scope<unique>, storage<config>,
                                   factory<function<make_config>>>();
}

template <typename Container>
void register_runtime_interface_binding(Container &container) {
  container.template register_type<scope<shared>,
                                   storage<std::shared_ptr<iface_impl<11>>>,
                                   interfaces<iface>>();
}

template <typename Container>
void register_runtime_collection_bindings(Container &container) {
  container.template register_type<scope<shared>,
                                   storage<std::shared_ptr<iface_impl<3>>>,
                                   interfaces<iface>>();
  container.template register_type<scope<shared>,
                                   storage<std::shared_ptr<iface_impl<5>>>,
                                   interfaces<iface>>();
}

template <typename Container>
void register_runtime_external_value_binding(Container &container) {
  container.template register_type<scope<external>, storage<external_config>>(
      make_external_config());
}

template <typename Container>
void register_runtime_external_reference_binding(Container &container,
                                                 external_config &config) {
  container.template register_type<scope<external>, storage<external_config &>>(
      config);
}

template <typename Container>
void register_runtime_external_wrapper_binding(
    Container &container, std::shared_ptr<external_config> config) {
  container.template register_type<scope<external>,
                                   storage<std::shared_ptr<external_config>>>(
      std::move(config));
}

using shared_config_static_source =
    static_bindings_source_t<static_wrapper_source>;
using shared_config_selection = detail::static_binding_t<
    typename shared_config_static_source::template bindings<
        config, detail::no_lookup_key_t>>;
using shared_config_binding = typename shared_config_selection::binding_type;

static_assert(
    !detail::binding_supports_request_v<config &&, shared_config_binding>);
static_assert(!detail::binding_supports_request_v<std::shared_ptr<config> &&,
                                                  shared_config_binding>);

} // namespace

extern "C" [[gnu::noinline]] int probe_static_resolution_consumer_read() {
  static_container<static_consumer_source> container;
  return container.resolve<consumer &>().read();
}

extern "C" [[gnu::noinline]] int probe_runtime_resolution_consumer_read() {
  container<> runtime;
  register_runtime_bindings(runtime);
  return runtime.resolve<consumer &>().read();
}

extern "C" [[gnu::noinline]] int
probe_static_resolution_mixed_container_consumer_read() {
  container<static_consumer_source> mixed_container;
  return mixed_container.resolve<consumer &>().read();
}

extern "C" [[gnu::noinline]] int probe_static_resolution_shared_config() {
  static_container<static_wrapper_source> container;
  return container.construct<std::shared_ptr<config>>()->value;
}

extern "C" [[gnu::noinline]] int probe_runtime_resolution_shared_config() {
  container<> runtime;
  register_runtime_wrapper_binding(runtime);
  return runtime.construct<std::shared_ptr<config>>()->value;
}

extern "C" [[gnu::noinline]] int
probe_static_resolution_mixed_container_shared_config() {
  container<static_wrapper_source> mixed_container;
  return mixed_container.construct<std::shared_ptr<config>>()->value;
}

extern "C" [[gnu::noinline]] int probe_static_resolution_shared_value_config() {
  static_container<static_wrapper_source> container;
  return container.construct<config>().value;
}

extern "C" [[gnu::noinline]] int
probe_runtime_resolution_shared_value_config() {
  container<> runtime;
  register_runtime_wrapper_binding(runtime);
  return runtime.construct<config>().value;
}

extern "C" [[gnu::noinline]] int
probe_static_resolution_mixed_container_shared_value_config() {
  container<static_wrapper_source> mixed_container;
  return mixed_container.construct<config>().value;
}

extern "C" [[gnu::noinline]] int
probe_static_resolution_shared_reference_config() {
  static_container<static_wrapper_source> container;
  return container.resolve<config &>().value;
}

extern "C" [[gnu::noinline]] int
probe_runtime_resolution_shared_reference_config() {
  container<> runtime;
  register_runtime_wrapper_binding(runtime);
  return runtime.resolve<config &>().value;
}

extern "C" [[gnu::noinline]] int
probe_static_resolution_mixed_container_shared_reference_config() {
  container<static_wrapper_source> mixed_container;
  return mixed_container.resolve<config &>().value;
}

extern "C" [[gnu::noinline]] int probe_static_resolution_optional_config() {
  static_container<static_wrapper_source> container;
  return container.construct<std::optional<config>>()->value;
}

extern "C" [[gnu::noinline]] int probe_runtime_resolution_optional_config() {
  container<> runtime;
  register_runtime_wrapper_binding(runtime);
  return runtime.construct<std::optional<config>>()->value;
}

extern "C" [[gnu::noinline]] int
probe_static_resolution_mixed_container_optional_config() {
  container<static_wrapper_source> mixed_container;
  return mixed_container.construct<std::optional<config>>()->value;
}

extern "C" [[gnu::noinline]] int probe_static_resolution_unique_value_config() {
  static_container<static_unique_source> container;
  return container.construct<config>().value;
}

extern "C" [[gnu::noinline]] int
probe_static_resolution_unique_rvalue_config() {
  static_container<static_unique_source> container;
  return container.resolve<config &&>().value;
}

extern "C" [[gnu::noinline]] int
probe_runtime_resolution_unique_value_config() {
  container<> runtime;
  register_runtime_unique_binding(runtime);
  return runtime.construct<config>().value;
}

extern "C" [[gnu::noinline]] int
probe_runtime_resolution_unique_rvalue_config() {
  container<> runtime;
  register_runtime_unique_binding(runtime);
  return runtime.resolve<config &&>().value;
}

extern "C" [[gnu::noinline]] int
probe_static_resolution_mixed_container_unique_value_config() {
  container<static_unique_source> mixed_container;
  return mixed_container.construct<config>().value;
}

extern "C" [[gnu::noinline]] int
probe_static_resolution_mixed_container_unique_rvalue_config() {
  container<static_unique_source> mixed_container;
  return mixed_container.resolve<config &&>().value;
}

extern "C" [[gnu::noinline]] int
probe_static_resolution_unique_wrapper_config() {
  static_container<static_unique_source> container;
  return container.construct<std::unique_ptr<config>>()->value;
}

extern "C" [[gnu::noinline]] int
probe_runtime_resolution_unique_wrapper_config() {
  container<> runtime;
  register_runtime_unique_binding(runtime);
  return runtime.construct<std::unique_ptr<config>>()->value;
}

extern "C" [[gnu::noinline]] int
probe_static_resolution_mixed_container_unique_wrapper_config() {
  container<static_unique_source> mixed_container;
  return mixed_container.construct<std::unique_ptr<config>>()->value;
}

extern "C" [[gnu::noinline]] int probe_static_resolution_interface_handle() {
  static_container<static_interface_source> container;
  return container.resolve<std::shared_ptr<iface> &>()->read();
}

extern "C" [[gnu::noinline]] int probe_runtime_resolution_interface_handle() {
  container<> runtime;
  register_runtime_interface_binding(runtime);
  return runtime.resolve<std::shared_ptr<iface> &>()->read();
}

extern "C" [[gnu::noinline]] int
probe_static_resolution_mixed_container_interface_handle() {
  container<static_interface_source> mixed_container;
  return mixed_container.resolve<std::shared_ptr<iface> &>()->read();
}

extern "C" [[gnu::noinline]] int
probe_runtime_resolution_external_value_storage() {
  container<> runtime;
  register_runtime_external_value_binding(runtime);
  return runtime.resolve<external_config &>().value;
}

extern "C" [[gnu::noinline]] int
probe_runtime_resolution_mixed_container_external_value_storage() {
  container<static_consumer_source> mixed_container;
  register_runtime_external_value_binding(mixed_container);
  return mixed_container.resolve<external_config &>().value;
}

extern "C" [[gnu::noinline]] int
probe_runtime_resolution_external_reference_storage() {
  container<> runtime;
  auto config = make_external_config();
  register_runtime_external_reference_binding(runtime, config);
  return runtime.resolve<external_config &>().value;
}

extern "C" [[gnu::noinline]] int
probe_runtime_resolution_mixed_container_external_reference_storage() {
  container<static_consumer_source> mixed_container;
  auto config = make_external_config();
  register_runtime_external_reference_binding(mixed_container, config);
  return mixed_container.resolve<external_config &>().value;
}

extern "C" [[gnu::noinline]] int
probe_runtime_resolution_external_wrapper_storage() {
  container<> runtime;
  register_runtime_external_wrapper_binding(
      runtime, std::make_shared<external_config>(make_external_config()));
  return runtime.resolve<std::shared_ptr<external_config> &>()->value;
}

extern "C" [[gnu::noinline]] int
probe_runtime_resolution_mixed_container_external_wrapper_storage() {
  container<static_consumer_source> mixed_container;
  register_runtime_external_wrapper_binding(
      mixed_container,
      std::make_shared<external_config>(make_external_config()));
  return mixed_container.resolve<std::shared_ptr<external_config> &>()->value;
}

extern "C" [[gnu::noinline]] int probe_static_resolution_collection_sum() {
  static_container<static_collection_source> container;
  auto values = container.resolve<std::vector<std::shared_ptr<iface>>>();
  return values[0]->read() + values[1]->read();
}

extern "C" [[gnu::noinline]] int probe_runtime_resolution_collection_sum() {
  container<> runtime;
  register_runtime_collection_bindings(runtime);
  auto values = runtime.resolve<std::vector<std::shared_ptr<iface>>>();
  return values[0]->read() + values[1]->read();
}

extern "C" [[gnu::noinline]] int
probe_static_resolution_mixed_container_collection_sum() {
  container<static_collection_source> mixed_container;
  auto values = mixed_container.resolve<std::vector<std::shared_ptr<iface>>>();
  return values[0]->read() + values[1]->read();
}
