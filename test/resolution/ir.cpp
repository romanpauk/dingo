//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/function.h>
#include <dingo/container.h>
#include <dingo/resolution/instance_factory.h>
#include <dingo/resolution/ir.h>
#include <dingo/resolution/runtime_execution_plan.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <variant>

namespace dingo {
namespace {
struct request_tag {};

struct detected_service {
    detected_service(int&, double&) {}
};

struct typedef_detected_service {
    using dingo_constructor_type =
        constructor<typedef_detected_service(int&, double&)>;

    typedef_detected_service(int&, double&) {}
};

struct explicit_service {};
struct array_service {};
struct alternative_service {};

using runtime_probe_container = container<dynamic_container_traits>;

struct runtime_request_probe_factory
    : instance_factory_interface<runtime_probe_container> {
    explicit_service instance;
    int value_calls = 0;
    int lvalue_calls = 0;
    int rvalue_calls = 0;
    int pointer_calls = 0;

    void* get_value(resolving_context&,
                    const runtime_probe_container::rtti_type::type_index&,
                    type_descriptor) override {
        ++value_calls;
        return &instance;
    }

    void* get_lvalue_reference(
        resolving_context&,
        const runtime_probe_container::rtti_type::type_index&,
        type_descriptor) override {
        ++lvalue_calls;
        return &instance;
    }

    void* get_rvalue_reference(
        resolving_context&,
        const runtime_probe_container::rtti_type::type_index&,
        type_descriptor) override {
        ++rvalue_calls;
        return &instance;
    }

    void* get_pointer(resolving_context&,
                      const runtime_probe_container::rtti_type::type_index&,
                      type_descriptor) override {
        ++pointer_calls;
        return &instance;
    }

    void destroy() override {}
};

struct runtime_optional_probe_factory
    : instance_factory_interface<runtime_probe_container> {
    std::optional<explicit_service> instance{std::in_place};
    int source_fetch_calls = 0;
    int value_calls = 0;
    int lvalue_calls = 0;
    type_descriptor last_requested_type = {};

    std::optional<explicit_service>& resolve(resolving_context&) {
        ++source_fetch_calls;
        return instance;
    }

    void* get_value(resolving_context&,
                    const runtime_probe_container::rtti_type::type_index&,
                    type_descriptor requested_type) override {
        ++value_calls;
        last_requested_type = requested_type;
        return &instance;
    }

    void* get_lvalue_reference(
        resolving_context&,
        const runtime_probe_container::rtti_type::type_index&,
        type_descriptor requested_type) override {
        ++lvalue_calls;
        last_requested_type = requested_type;
        return &instance;
    }

    void* get_rvalue_reference(
        resolving_context&,
        const runtime_probe_container::rtti_type::type_index&,
        type_descriptor requested_type) override {
        last_requested_type = requested_type;
        return &instance;
    }

    void* get_pointer(resolving_context&,
                      const runtime_probe_container::rtti_type::type_index&,
                      type_descriptor requested_type) override {
        last_requested_type = requested_type;
        return &instance;
    }

    void destroy() override {}
};

struct runtime_shared_ptr_probe_factory
    : instance_factory_interface<runtime_probe_container> {
    std::shared_ptr<explicit_service> instance =
        std::make_shared<explicit_service>();
    int source_fetch_calls = 0;
    int value_calls = 0;
    type_descriptor last_requested_type = {};

    std::shared_ptr<explicit_service>& resolve(resolving_context&) {
        ++source_fetch_calls;
        return instance;
    }

    void* get_value(resolving_context&,
                    const runtime_probe_container::rtti_type::type_index&,
                    type_descriptor requested_type) override {
        ++value_calls;
        last_requested_type = requested_type;
        return &instance;
    }

    void* get_lvalue_reference(
        resolving_context&,
        const runtime_probe_container::rtti_type::type_index&,
        type_descriptor requested_type) override {
        last_requested_type = requested_type;
        return &instance;
    }

    void* get_rvalue_reference(
        resolving_context&,
        const runtime_probe_container::rtti_type::type_index&,
        type_descriptor requested_type) override {
        last_requested_type = requested_type;
        return &instance;
    }

    void* get_pointer(resolving_context&,
                      const runtime_probe_container::rtti_type::type_index&,
                      type_descriptor requested_type) override {
        last_requested_type = requested_type;
        return &instance;
    }

    void destroy() override {}
};

struct runtime_unique_ptr_probe_factory
    : instance_factory_interface<runtime_probe_container> {
    int source_fetch_calls = 0;
    int value_calls = 0;
    type_descriptor last_requested_type = {};

    std::unique_ptr<explicit_service> resolve(resolving_context&) {
        ++source_fetch_calls;
        return std::make_unique<explicit_service>();
    }

    void* get_value(resolving_context&,
                    const runtime_probe_container::rtti_type::type_index&,
                    type_descriptor requested_type) override {
        ++value_calls;
        last_requested_type = requested_type;
        return nullptr;
    }

    void* get_lvalue_reference(
        resolving_context&,
        const runtime_probe_container::rtti_type::type_index&,
        type_descriptor requested_type) override {
        last_requested_type = requested_type;
        return nullptr;
    }

    void* get_rvalue_reference(
        resolving_context&,
        const runtime_probe_container::rtti_type::type_index&,
        type_descriptor requested_type) override {
        last_requested_type = requested_type;
        return nullptr;
    }

    void* get_pointer(resolving_context&,
                      const runtime_probe_container::rtti_type::type_index&,
                      type_descriptor requested_type) override {
        last_requested_type = requested_type;
        return nullptr;
    }

    void destroy() override {}
};

explicit_service build_explicit(int&, double&) { return {}; }
} // namespace

TEST(resolution_ir_test, request_ir_canonicalizes_reference_requests) {
    using request = detail::request_ir_t<int&>;

    EXPECT_TRUE((std::is_same_v<typename request::service_type, int&>));
    EXPECT_TRUE((std::is_same_v<typename request::lookup_type, int>));
    EXPECT_TRUE(
        (std::is_same_v<typename request::runtime_lookup_type, runtime_type&>));
    EXPECT_TRUE(
        (std::is_same_v<typename request::request_kind,
                        ir::lvalue_reference_request>));
}

TEST(resolution_ir_test, request_ir_preserves_lookup_annotations) {
    using request = detail::request_ir_t<annotated<int&, request_tag>>;

    EXPECT_TRUE((std::is_same_v<typename request::service_type, int&>));
    EXPECT_TRUE(
        (std::is_same_v<typename request::lookup_type,
                        annotated<int, request_tag>>));
    EXPECT_TRUE(
        (std::is_same_v<typename request::runtime_lookup_type, runtime_type&>));
    EXPECT_TRUE(
        (std::is_same_v<typename request::request_kind,
                        ir::lvalue_reference_request>));
}

TEST(resolution_ir_test, collection_plan_ir_preserves_collection_request_shape) {
    using plan = detail::collection_plan_ir_t<
        std::vector<annotated<int&, request_tag>>,
        detail::request_ir_t<annotated<int&, request_tag>>,
        ir::collection_aggregation_kind::custom>;

    EXPECT_TRUE((std::is_same_v<typename plan::collection_type,
                                std::vector<annotated<int&, request_tag>>>));
    EXPECT_TRUE((std::is_same_v<typename plan::element_request_type,
                                detail::request_ir_t<
                                    annotated<int&, request_tag>>>));
    EXPECT_TRUE((std::is_same_v<typename plan::element_lookup_type,
                                annotated<int, request_tag>>));
    EXPECT_EQ(plan::aggregation_kind, ir::collection_aggregation_kind::custom);
}

TEST(resolution_ir_test, indexed_lookup_plan_ir_preserves_lookup_request_shape) {
    using plan =
        detail::indexed_lookup_plan_ir_t<annotated<int&, request_tag>, long>;

    EXPECT_TRUE((std::is_same_v<typename plan::request_type::request_type,
                                detail::request_ir_t<
                                    annotated<int&, request_tag>>>));
    EXPECT_TRUE((std::is_same_v<typename plan::lookup_type,
                                annotated<int, request_tag>>));
    EXPECT_TRUE((std::is_same_v<typename plan::key_type, long>));
    EXPECT_EQ(plan::mode, ir::lookup_mode::indexed);
}

TEST(resolution_ir_test, binding_ir_collects_registration_ownership) {
    using storage_type =
        detail::storage<shared, explicit_service, explicit_service,
                        constructor<explicit_service()>,
                        detail::conversions<shared, explicit_service,
                                            runtime_type>>;
    using binding = detail::binding_ir_t<explicit_service, storage_type>;
    using acquisition = detail::acquisition_ir_t<storage_type>;

    EXPECT_TRUE(
        (std::is_same_v<typename binding::interface_type, explicit_service>));
    EXPECT_TRUE(
        (std::is_same_v<typename binding::registered_type, explicit_service>));
    EXPECT_TRUE((std::is_same_v<typename binding::stored_type, explicit_service>));
    EXPECT_TRUE((std::is_same_v<typename binding::scope_tag, shared>));
    EXPECT_TRUE((std::is_same_v<typename binding::factory_type,
                                constructor<explicit_service()>>));
    EXPECT_TRUE((std::is_same_v<typename acquisition::storage_tag, shared>));
    EXPECT_TRUE(acquisition::cacheable);
}

TEST(resolution_ir_test, constructor_factories_expose_dependency_ir) {
    using invocation =
        detail::factory_invocation_ir_t<constructor<explicit_service(int&, double&)>>;

    EXPECT_TRUE((std::is_same_v<typename invocation::constructed_type,
                                explicit_service>));
    EXPECT_TRUE(
        (std::is_same_v<typename invocation::dependencies,
                        type_list<int&, double&>>));
    EXPECT_EQ(invocation::arity, 2u);
}

TEST(resolution_ir_test, function_factories_expose_dependency_ir) {
    using invocation = detail::factory_invocation_ir_t<function<build_explicit>>;

    EXPECT_TRUE((std::is_same_v<typename invocation::return_type,
                                explicit_service>));
    EXPECT_TRUE(
        (std::is_same_v<typename invocation::dependencies,
                        type_list<int&, double&>>));
    EXPECT_EQ(invocation::arity, 2u);
}

TEST(resolution_ir_test, constructor_detection_exposes_detected_construction_ir) {
    using invocation =
        detail::factory_invocation_ir_t<constructor_detection<detected_service>>;

    EXPECT_TRUE((std::is_same_v<typename invocation::constructed_type,
                                detected_service>));
    EXPECT_TRUE(
        (std::is_same_v<typename invocation::detection_tag, detail::automatic>));
    EXPECT_TRUE(
        (std::is_same_v<typename invocation::dependencies,
                        type_list<
                            ir::detected_constructor_dependency<
                                detected_service, detail::automatic, 0>,
                            ir::detected_constructor_dependency<
                                detected_service, detail::automatic, 1>>>));
    EXPECT_EQ(invocation::arity, 2u);
}

TEST(resolution_ir_test,
     constructor_detection_forwards_constructor_typedef_dependency_ir) {
    using invocation =
        detail::factory_invocation_ir_t<constructor_detection<typedef_detected_service>>;

    EXPECT_TRUE((std::is_same_v<typename invocation::constructed_type,
                                typedef_detected_service>));
    EXPECT_TRUE(
        (std::is_same_v<typename invocation::dependencies,
                        type_list<int&, double&>>));
    EXPECT_EQ(invocation::arity, 2u);
}

TEST(resolution_ir_test, concrete_factory_exposes_full_resolution_plan) {
    using container_type = container<static_container_traits<>>;
    using storage_type =
        detail::storage<shared, explicit_service, explicit_service,
                        constructor<explicit_service(int&, double&)>,
                        detail::conversions<shared, explicit_service,
                                            runtime_type>>;
    using factory_data_type = instance_factory_data<container_type, storage_type>;
    using factory_type =
        instance_factory<container_type, explicit_service, storage_type,
                         factory_data_type>;
    using plan = decltype(detail::plan_for<explicit_service&>(
        std::declval<factory_type&>()));

    EXPECT_TRUE((std::is_same_v<typename plan::binding_type,
                                detail::binding_ir_t<explicit_service,
                                                     storage_type>>));
    EXPECT_TRUE((std::is_same_v<typename plan::acquisition_type,
                                detail::acquisition_ir_t<storage_type>>));
    EXPECT_TRUE((std::is_same_v<typename plan::invocation_type,
                                detail::factory_invocation_ir_t<
                                    constructor<explicit_service(int&, double&)>>>));
    EXPECT_TRUE((std::is_same_v<typename plan::conversion_type::target_type,
                                explicit_service>));
    EXPECT_TRUE((std::is_same_v<typename plan::conversion_type::source_type,
                                explicit_service*>));
    EXPECT_TRUE((std::is_same_v<typename plan::conversion_type::lookup_type,
                                runtime_type&>));
    EXPECT_EQ(plan::conversion_type::family, ir::conversion_family::native);
    EXPECT_EQ(plan::conversion_type::kind,
              ir::conversion_kind::dereference_pointer_to_lvalue_reference);
}

TEST(resolution_ir_test, erased_factory_path_uses_erased_resolution_plan) {
    using container_type = container<static_container_traits<>>;
    using factory_interface = instance_factory_interface<container_type>;
    using plan = decltype(detail::plan_for<explicit_service&>(
        std::declval<factory_interface&>()));

    EXPECT_TRUE(
        (std::is_same_v<typename plan::binding_type, ir::erased_binding>));
    EXPECT_TRUE((std::is_same_v<typename plan::acquisition_type,
                                ir::erased_acquisition>));
    EXPECT_TRUE(
        (std::is_same_v<typename plan::conversion_type, ir::erased_conversion>));
}

TEST(resolution_ir_test, lowered_runtime_plan_keeps_value_level_request_shape) {
    using container_type = container<dynamic_container_traits>;
    using factory_interface = instance_factory_interface<container_type>;
    using storage_type =
        detail::storage<shared, explicit_service, explicit_service,
                        constructor<explicit_service()>,
                        detail::conversions<shared, explicit_service,
                                            runtime_type>>;
    using plan = decltype(detail::plan_for<explicit_service*>(
        std::declval<factory_interface&>()));
    auto binding = detail::make_runtime_binding_record<
        typename container_type::rtti_type, explicit_service, storage_type,
        explicit_service*>(7, static_cast<factory_interface*>(nullptr));

    auto runtime_plan = detail::lower_runtime_plan<typename container_type::rtti_type>(
        plan{}, binding);

    EXPECT_EQ(runtime_plan.request_kind, detail::runtime_request_kind::pointer);
    EXPECT_EQ(runtime_plan.access_kind, detail::runtime_access_kind::pointer);
    EXPECT_EQ(runtime_plan.conversion_route,
              detail::runtime_conversion_route::request_dispatch);
    EXPECT_EQ(runtime_plan.conversion_family,
              detail::runtime_conversion_family::native);
    EXPECT_EQ(runtime_plan.conversion_kind,
              detail::runtime_conversion_kind::pointer);
    EXPECT_EQ(runtime_plan.lookup_type,
              container_type::rtti_type::template get_type_index<runtime_type*>());
    EXPECT_EQ(runtime_plan.requested_type, describe_type<explicit_service*>());
    EXPECT_EQ(runtime_plan.conversion_source_type,
              describe_type<explicit_service*>());
    EXPECT_EQ(runtime_plan.conversion_target_type,
              describe_type<explicit_service*>());
    EXPECT_EQ(runtime_plan.binding.id, 7u);
    EXPECT_EQ(runtime_plan.binding.storage_kind,
              detail::runtime_storage_kind::shared);
    EXPECT_EQ(runtime_plan.binding.interface_type, describe_type<explicit_service>());
    EXPECT_EQ(runtime_plan.binding.registered_type,
              describe_type<explicit_service>());
    EXPECT_EQ(runtime_plan.binding.stored_type, describe_type<explicit_service>());
    EXPECT_EQ(runtime_plan.binding.source_type, describe_type<explicit_service*>());
    EXPECT_NE(runtime_plan.binding.select_conversion, nullptr);
    EXPECT_TRUE(runtime_plan.binding.cacheable);
}

TEST(resolution_ir_test,
     lowered_runtime_plan_reifies_pointer_to_reference_conversion) {
    using container_type = container<dynamic_container_traits>;
    using factory_interface = instance_factory_interface<container_type>;
    using storage_type =
        detail::storage<shared, explicit_service, explicit_service,
                        constructor<explicit_service()>,
                        detail::conversions<shared, explicit_service,
                                            runtime_type>>;
    using plan = decltype(detail::plan_for<explicit_service&>(
        std::declval<factory_interface&>()));
    auto binding = detail::make_runtime_binding_record<
        typename container_type::rtti_type, explicit_service, storage_type,
        explicit_service*>(11, static_cast<factory_interface*>(nullptr));

    auto runtime_plan = detail::lower_runtime_plan<
        typename container_type::rtti_type>(plan{}, binding);

    EXPECT_EQ(runtime_plan.request_kind,
              detail::runtime_request_kind::lvalue_reference);
    EXPECT_EQ(runtime_plan.access_kind, detail::runtime_access_kind::pointer);
    EXPECT_EQ(runtime_plan.conversion_route,
              detail::runtime_conversion_route::source_access);
    EXPECT_EQ(runtime_plan.conversion_family,
              detail::runtime_conversion_family::native);
    EXPECT_EQ(runtime_plan.conversion_kind,
              detail::runtime_conversion_kind::
                  dereference_pointer_to_lvalue_reference);
    EXPECT_EQ(runtime_plan.lookup_type,
              container_type::rtti_type::template get_type_index<runtime_type*>());
    EXPECT_EQ(runtime_plan.conversion_source_type,
              describe_type<explicit_service*>());
    EXPECT_EQ(runtime_plan.conversion_target_type,
              describe_type<explicit_service&>());
}

TEST(resolution_ir_test,
     lowered_runtime_plan_reifies_reference_to_pointer_conversion) {
    using container_type = container<dynamic_container_traits>;
    using factory_interface = instance_factory_interface<container_type>;
    using storage_type =
        detail::storage<external, explicit_service, explicit_service, void,
                        detail::conversions<external, explicit_service,
                                            runtime_type>>;
    using plan = decltype(detail::plan_for<explicit_service*>(
        std::declval<factory_interface&>()));
    auto binding = detail::make_runtime_binding_record<
        typename container_type::rtti_type, explicit_service, storage_type,
        explicit_service&>(13, static_cast<factory_interface*>(nullptr));

    auto runtime_plan = detail::lower_runtime_plan<
        typename container_type::rtti_type>(plan{}, binding);

    EXPECT_EQ(runtime_plan.request_kind, detail::runtime_request_kind::pointer);
    EXPECT_EQ(runtime_plan.access_kind,
              detail::runtime_access_kind::lvalue_reference);
    EXPECT_EQ(runtime_plan.conversion_route,
              detail::runtime_conversion_route::source_access);
    EXPECT_EQ(runtime_plan.conversion_family,
              detail::runtime_conversion_family::native);
    EXPECT_EQ(runtime_plan.conversion_kind,
              detail::runtime_conversion_kind::address_of_lvalue_reference);
    EXPECT_EQ(runtime_plan.lookup_type,
              container_type::rtti_type::template get_type_index<runtime_type&>());
    EXPECT_EQ(runtime_plan.conversion_source_type,
              describe_type<explicit_service&>());
    EXPECT_EQ(runtime_plan.conversion_target_type,
              describe_type<explicit_service*>());
}

TEST(resolution_ir_test,
     lowered_runtime_plan_reifies_reference_to_value_conversion) {
    using container_type = container<dynamic_container_traits>;
    using factory_interface = instance_factory_interface<container_type>;
    using storage_type =
        detail::storage<external, explicit_service, explicit_service, void,
                        detail::conversions<external, explicit_service,
                                            runtime_type>>;
    using plan = decltype(detail::plan_for<explicit_service>(
        std::declval<factory_interface&>()));
    auto binding = detail::make_runtime_binding_record<
        typename container_type::rtti_type, explicit_service, storage_type,
        explicit_service&>(17, static_cast<factory_interface*>(nullptr));

    auto runtime_plan = detail::lower_runtime_plan<
        typename container_type::rtti_type>(plan{}, binding);

    EXPECT_EQ(runtime_plan.request_kind, detail::runtime_request_kind::value);
    EXPECT_EQ(runtime_plan.access_kind,
              detail::runtime_access_kind::lvalue_reference);
    EXPECT_EQ(runtime_plan.conversion_route,
              detail::runtime_conversion_route::source_access);
    EXPECT_EQ(runtime_plan.conversion_family,
              detail::runtime_conversion_family::native);
    EXPECT_EQ(runtime_plan.conversion_kind,
              detail::runtime_conversion_kind::
                  copy_or_move_from_lvalue_reference);
    EXPECT_EQ(runtime_plan.lookup_type,
              container_type::rtti_type::template get_type_index<runtime_type&>());
}

TEST(resolution_ir_test,
     lowered_runtime_plan_keeps_array_bindings_on_request_path) {
    using container_type = container<dynamic_container_traits>;
    using factory_interface = instance_factory_interface<container_type>;
    using storage_type =
        detail::storage<external, array_service[2], array_service[2], void,
                        detail::conversions<external, array_service[2],
                                            runtime_type>>;
    using plan = decltype(detail::plan_for<array_service&>(
        std::declval<factory_interface&>()));
    auto binding = detail::make_runtime_binding_record<
        typename container_type::rtti_type, array_service, storage_type,
        array_service*>(19, static_cast<factory_interface*>(nullptr));

    auto runtime_plan = detail::lower_runtime_plan<
        typename container_type::rtti_type>(plan{}, binding);

    EXPECT_FALSE(runtime_plan.binding.supports_source_access);
    EXPECT_EQ(runtime_plan.conversion_route,
              detail::runtime_conversion_route::request_dispatch);
    EXPECT_EQ(runtime_plan.conversion_family,
              detail::runtime_conversion_family::array);
    EXPECT_EQ(runtime_plan.access_kind,
              detail::runtime_access_kind::lvalue_reference);
    EXPECT_EQ(runtime_plan.conversion_kind,
              detail::runtime_conversion_kind::lvalue_reference);
    EXPECT_EQ(runtime_plan.lookup_type,
              container_type::rtti_type::template get_type_index<runtime_type&>());
}

TEST(resolution_ir_test,
     lowered_runtime_plan_lowers_wrapper_bindings_to_source_fetch) {
    using container_type = container<dynamic_container_traits>;
    using factory_interface = instance_factory_interface<container_type>;
    using storage_type =
        detail::storage<shared, std::shared_ptr<explicit_service>,
                        std::shared_ptr<explicit_service>,
                        constructor<std::shared_ptr<explicit_service>()>,
                        detail::conversions<shared,
                                            std::shared_ptr<explicit_service>,
                                            runtime_type>>;
    using plan = decltype(detail::plan_for<std::shared_ptr<explicit_service>>(
        std::declval<factory_interface&>()));
    auto binding = detail::make_runtime_binding_record<
        typename container_type::rtti_type, explicit_service, storage_type,
        std::shared_ptr<explicit_service>&, runtime_shared_ptr_probe_factory>(
        23, static_cast<factory_interface*>(nullptr));

    auto runtime_plan = detail::lower_runtime_plan<
        typename container_type::rtti_type>(plan{}, binding);

    EXPECT_FALSE(runtime_plan.binding.supports_source_access);
    EXPECT_EQ(runtime_plan.conversion_route,
              detail::runtime_conversion_route::source_fetch);
    EXPECT_EQ(runtime_plan.conversion_family,
              detail::runtime_conversion_family::handle);
    EXPECT_EQ(runtime_plan.access_kind, detail::runtime_access_kind::value);
    EXPECT_EQ(runtime_plan.conversion_kind,
              detail::runtime_conversion_kind::value_copy_or_move);
    EXPECT_EQ(runtime_plan.lookup_type,
              container_type::rtti_type::template get_type_index<
                  std::shared_ptr<runtime_type>>());
}

TEST(resolution_ir_test,
     lowered_runtime_plan_classifies_wrapper_borrow_conversion) {
    using container_type = container<dynamic_container_traits>;
    using factory_interface = instance_factory_interface<container_type>;
    using storage_type =
        detail::storage<shared, std::optional<explicit_service>,
                        std::optional<explicit_service>,
                        constructor<std::optional<explicit_service>()>,
                        detail::conversions<shared,
                                            std::optional<explicit_service>,
                                            runtime_type>>;
    using plan = decltype(detail::plan_for<explicit_service&>(
        std::declval<factory_interface&>()));
    auto binding = detail::make_runtime_binding_record<
        typename container_type::rtti_type, explicit_service, storage_type,
        std::optional<explicit_service>&>(
        29, static_cast<factory_interface*>(nullptr));

    auto runtime_plan = detail::lower_runtime_plan<
        typename container_type::rtti_type>(plan{}, binding);

    EXPECT_FALSE(runtime_plan.binding.supports_source_access);
    EXPECT_EQ(runtime_plan.conversion_route,
              detail::runtime_conversion_route::request_dispatch);
    EXPECT_EQ(runtime_plan.conversion_family,
              detail::runtime_conversion_family::borrow);
    EXPECT_EQ(runtime_plan.access_kind,
              detail::runtime_access_kind::lvalue_reference);
    EXPECT_EQ(runtime_plan.conversion_kind,
              detail::runtime_conversion_kind::lvalue_reference);
}

TEST(resolution_ir_test,
     lowered_runtime_plan_classifies_alternative_conversion) {
    using container_type = container<dynamic_container_traits>;
    using factory_interface = instance_factory_interface<container_type>;
    using storage_type =
        detail::storage<shared,
                        std::variant<explicit_service, alternative_service>,
                        std::variant<explicit_service, alternative_service>,
                        constructor<
                            std::variant<explicit_service, alternative_service>()>,
                        detail::conversions<
                            shared,
                            std::variant<explicit_service, alternative_service>,
                            runtime_type>>;
    using plan = decltype(detail::plan_for<explicit_service&>(
        std::declval<factory_interface&>()));
    auto binding = detail::make_runtime_binding_record<
        typename container_type::rtti_type, explicit_service, storage_type,
        std::variant<explicit_service, alternative_service>&>(
        31, static_cast<factory_interface*>(nullptr));

    auto runtime_plan = detail::lower_runtime_plan<
        typename container_type::rtti_type>(plan{}, binding);

    EXPECT_FALSE(runtime_plan.binding.supports_source_access);
    EXPECT_EQ(runtime_plan.conversion_route,
              detail::runtime_conversion_route::request_dispatch);
    EXPECT_EQ(runtime_plan.conversion_family,
              detail::runtime_conversion_family::alternative);
    EXPECT_EQ(runtime_plan.access_kind,
              detail::runtime_access_kind::lvalue_reference);
    EXPECT_EQ(runtime_plan.conversion_kind,
              detail::runtime_conversion_kind::lvalue_reference);
}

TEST(resolution_ir_test,
     request_dispatch_runtime_plan_uses_binding_request_executor) {
    using container_type = runtime_probe_container;
    using factory_interface = instance_factory_interface<container_type>;
    using storage_type =
        detail::storage<shared, std::optional<explicit_service>,
                        std::optional<explicit_service>,
                        constructor<std::optional<explicit_service>()>,
                        detail::conversions<shared,
                                            std::optional<explicit_service>,
                                            runtime_type>>;
    using plan = decltype(detail::plan_for<explicit_service&>(
        std::declval<factory_interface&>()));
    runtime_request_probe_factory factory;
    auto binding = detail::make_runtime_binding_record<
        typename container_type::rtti_type, explicit_service, storage_type,
        std::optional<explicit_service>&, runtime_request_probe_factory>(
        37, static_cast<factory_interface*>(&factory));

    auto runtime_plan = detail::lower_runtime_plan<
        typename container_type::rtti_type>(plan{}, binding);
    resolving_context context;
    void* ptr = detail::execute_runtime_plan(context, runtime_plan);

    ASSERT_NE(runtime_plan.binding.execute_request, nullptr);
    EXPECT_EQ(ptr, &factory.instance);
    EXPECT_EQ(factory.value_calls, 0);
    EXPECT_EQ(factory.lvalue_calls, 1);
    EXPECT_EQ(factory.rvalue_calls, 0);
    EXPECT_EQ(factory.pointer_calls, 0);
}

TEST(resolution_ir_test,
     request_dispatch_runtime_plan_borrows_from_fetched_source) {
    using container_type = runtime_probe_container;
    using factory_interface = instance_factory_interface<container_type>;
    using storage_type =
        detail::storage<shared, std::optional<explicit_service>,
                        std::optional<explicit_service>,
                        constructor<std::optional<explicit_service>()>,
                        detail::conversions<shared,
                                            std::optional<explicit_service>,
                                            runtime_type>>;
    using plan = decltype(detail::plan_for<explicit_service&>(
        std::declval<factory_interface&>()));
    runtime_optional_probe_factory factory;
    auto binding = detail::make_runtime_binding_record<
        typename container_type::rtti_type, explicit_service, storage_type,
        std::optional<explicit_service>&, runtime_optional_probe_factory>(
        41, static_cast<factory_interface*>(&factory));

    auto runtime_plan = detail::lower_runtime_plan<
        typename container_type::rtti_type>(plan{}, binding);
    resolving_context context;
    void* ptr = detail::execute_runtime_plan(context, runtime_plan);

    EXPECT_EQ(ptr, std::addressof(factory.instance.value()));
    EXPECT_EQ(factory.source_fetch_calls, 1);
    EXPECT_EQ(factory.value_calls, 0);
    EXPECT_EQ(factory.lvalue_calls, 0);
    EXPECT_EQ(factory.last_requested_type, type_descriptor{});
}

TEST(resolution_ir_test,
     source_fetch_runtime_plan_rebinds_shared_ptr_handle) {
    using container_type = runtime_probe_container;
    using factory_interface = instance_factory_interface<container_type>;
    using storage_type =
        detail::storage<shared, std::shared_ptr<explicit_service>,
                        std::shared_ptr<explicit_service>,
                        constructor<std::shared_ptr<explicit_service>()>,
                        detail::conversions<shared,
                                            std::shared_ptr<explicit_service>,
                                            runtime_type>>;
    using plan = decltype(detail::plan_for<std::shared_ptr<explicit_service>>(
        std::declval<factory_interface&>()));
    runtime_shared_ptr_probe_factory factory;
    auto binding = detail::make_runtime_binding_record<
        typename container_type::rtti_type, explicit_service, storage_type,
        std::shared_ptr<explicit_service>&, runtime_shared_ptr_probe_factory>(
        43, static_cast<factory_interface*>(&factory));

    auto runtime_plan = detail::lower_runtime_plan<
        typename container_type::rtti_type>(plan{}, binding);
    resolving_context context;
    void* ptr = detail::execute_runtime_plan(context, runtime_plan);
    auto result = detail::convert_runtime_plan<
        typename container_type::rtti_type>(ptr, runtime_plan, plan{});

    EXPECT_EQ(runtime_plan.conversion_route,
              detail::runtime_conversion_route::source_fetch);
    EXPECT_EQ(runtime_plan.conversion_family,
              detail::runtime_conversion_family::handle);
    EXPECT_EQ(result.get(), factory.instance.get());
    EXPECT_EQ(factory.source_fetch_calls, 1);
    EXPECT_EQ(factory.value_calls, 0);
    EXPECT_EQ(factory.last_requested_type, type_descriptor{});
}

TEST(resolution_ir_test, format_runtime_plan_prints_binding_and_conversion) {
    using container_type = runtime_probe_container;
    using factory_interface = instance_factory_interface<container_type>;
    using storage_type =
        detail::storage<shared, std::shared_ptr<explicit_service>,
                        std::shared_ptr<explicit_service>,
                        constructor<std::shared_ptr<explicit_service>()>,
                        detail::conversions<shared,
                                            std::shared_ptr<explicit_service>,
                                            runtime_type>>;
    using plan = decltype(detail::plan_for<std::shared_ptr<explicit_service>>(
        std::declval<factory_interface&>()));
    runtime_shared_ptr_probe_factory factory;
    auto binding = detail::make_runtime_binding_record<
        typename container_type::rtti_type, explicit_service, storage_type,
        std::shared_ptr<explicit_service>&, runtime_shared_ptr_probe_factory>(
        44, static_cast<factory_interface*>(&factory));

    auto runtime_plan = detail::lower_runtime_plan<
        typename container_type::rtti_type>(plan{}, binding);
    std::string formatted;
    detail::append_execution_plan(formatted, runtime_plan);

    EXPECT_NE(formatted.find("request value"), std::string::npos);
    EXPECT_NE(formatted.find("route source_fetch"), std::string::npos);
    EXPECT_NE(formatted.find("family handle"), std::string::npos);
    EXPECT_NE(formatted.find("kind value_copy_or_move"), std::string::npos);
    EXPECT_NE(formatted.find("binding { id 44"), std::string::npos);
    EXPECT_NE(formatted.find("storage shared"), std::string::npos);
    EXPECT_NE(formatted.find("shape wrapper"), std::string::npos);
}

TEST(resolution_ir_test,
     source_fetch_runtime_plan_moves_unique_handle_to_shared_ptr) {
    using container_type = runtime_probe_container;
    using factory_interface = instance_factory_interface<container_type>;
    using storage_type =
        detail::storage<unique, std::unique_ptr<explicit_service>,
                        std::unique_ptr<explicit_service>,
                        constructor<std::unique_ptr<explicit_service>()>,
                        detail::conversions<unique,
                                            std::unique_ptr<explicit_service>,
                                            runtime_type>>;
    using plan = decltype(detail::plan_for<std::shared_ptr<explicit_service>>(
        std::declval<factory_interface&>()));
    runtime_unique_ptr_probe_factory factory;
    auto binding = detail::make_runtime_binding_record<
        typename container_type::rtti_type, explicit_service, storage_type,
        std::unique_ptr<explicit_service>, runtime_unique_ptr_probe_factory>(
        45, static_cast<factory_interface*>(&factory));

    auto runtime_plan = detail::lower_runtime_plan<
        typename container_type::rtti_type>(plan{}, binding);
    resolving_context context;
    void* ptr = detail::execute_runtime_plan(context, runtime_plan);
    auto result = detail::convert_runtime_plan<
        typename container_type::rtti_type>(ptr, runtime_plan, plan{});

    EXPECT_EQ(runtime_plan.conversion_route,
              detail::runtime_conversion_route::source_fetch);
    EXPECT_EQ(runtime_plan.conversion_family,
              detail::runtime_conversion_family::handle);
    EXPECT_NE(result.get(), nullptr);
    EXPECT_EQ(factory.source_fetch_calls, 1);
    EXPECT_EQ(factory.value_calls, 0);
    EXPECT_EQ(factory.last_requested_type, type_descriptor{});
}

TEST(resolution_ir_test,
     source_fetch_runtime_plan_preserves_exact_wrapper_reference) {
    using container_type = runtime_probe_container;
    using factory_interface = instance_factory_interface<container_type>;
    using storage_type =
        detail::storage<shared, std::optional<explicit_service>,
                        std::optional<explicit_service>,
                        constructor<std::optional<explicit_service>()>,
                        detail::conversions<shared,
                                            std::optional<explicit_service>,
                                            runtime_type>>;
    using plan = decltype(detail::plan_for<std::optional<explicit_service>&>(
        std::declval<factory_interface&>()));
    runtime_optional_probe_factory factory;
    auto binding = detail::make_runtime_binding_record<
        typename container_type::rtti_type, explicit_service, storage_type,
        std::optional<explicit_service>&, runtime_optional_probe_factory>(
        47, static_cast<factory_interface*>(&factory));

    auto runtime_plan = detail::lower_runtime_plan<
        typename container_type::rtti_type>(plan{}, binding);
    resolving_context context;
    void* ptr = detail::execute_runtime_plan(context, runtime_plan);
    auto& result = detail::convert_runtime_plan<
        typename container_type::rtti_type>(ptr, runtime_plan, plan{});

    EXPECT_EQ(runtime_plan.conversion_route,
              detail::runtime_conversion_route::source_fetch);
    EXPECT_EQ(runtime_plan.conversion_family,
              detail::runtime_conversion_family::borrow);
    EXPECT_EQ(std::addressof(result), std::addressof(factory.instance));
    EXPECT_EQ(factory.source_fetch_calls, 1);
    EXPECT_EQ(factory.lvalue_calls, 0);
    EXPECT_EQ(factory.last_requested_type, type_descriptor{});
}

} // namespace dingo
