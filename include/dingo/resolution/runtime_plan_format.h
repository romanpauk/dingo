//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/exceptions.h>
#include <dingo/resolution/runtime_execution_plan.h>

#include <string>

namespace dingo {
namespace detail {

constexpr const char* collection_aggregation_kind_name(
    ir::collection_aggregation_kind kind) {
    switch (kind) {
    case ir::collection_aggregation_kind::standard:
        return "standard";
    case ir::collection_aggregation_kind::custom:
        return "custom";
    }

    return "custom";
}

constexpr const char* lookup_mode_name(ir::lookup_mode mode) {
    switch (mode) {
    case ir::lookup_mode::single:
        return "single";
    case ir::lookup_mode::indexed:
        return "indexed";
    case ir::lookup_mode::collection:
        return "collection";
    }

    return "single";
}

inline void append_runtime_bool(std::string& message, bool value) {
    message += value ? "true" : "false";
}

struct collection_plan_summary {
    type_descriptor collection_type;
    ir::collection_aggregation_kind aggregation_kind;
    type_descriptor element_request_type;
    type_descriptor element_lookup_type;
};

template <typename CollectionPlan>
constexpr auto make_collection_plan_summary() -> collection_plan_summary {
    return {describe_type<typename CollectionPlan::collection_type>(),
            CollectionPlan::aggregation_kind,
            describe_type<
                typename CollectionPlan::element_request_type::request_type>(),
            describe_type<typename CollectionPlan::element_lookup_type>()};
}

struct lookup_plan_summary {
    ir::lookup_mode mode;
    type_descriptor request_type;
    type_descriptor lookup_type;
    type_descriptor key_type;
};

template <typename LookupPlan>
constexpr auto make_lookup_plan_summary() -> lookup_plan_summary {
    return {LookupPlan::mode,
            describe_type<typename LookupPlan::request_type::request_type>(),
            describe_type<typename LookupPlan::lookup_type>(),
            describe_type<typename LookupPlan::key_type>()};
}

template <typename FactoryPtr, typename TypeIndex>
void append_runtime_binding_summary(
    std::string& message,
    const runtime_binding_record<FactoryPtr, TypeIndex>& binding);

template <typename RTTI, typename Binding>
void append_execution_plan(std::string& message,
                           const execution_plan<RTTI, Binding>& plan) {
    message += "request ";
    message += runtime_request_kind_name(plan.request_kind);
    message += ", access ";
    message += runtime_access_kind_name(plan.access_kind);
    message += ", route ";
    message += runtime_conversion_route_name(plan.conversion_route);
    message += ", family ";
    message += runtime_conversion_family_name(plan.conversion_family);
    message += ", kind ";
    message += runtime_conversion_kind_name(plan.conversion_kind);
    message += ", requested ";
    append_type_name(message, plan.requested_type);
    message += ", source ";
    append_type_name(message, plan.conversion_source_type);
    message += ", target ";
    append_type_name(message, plan.conversion_target_type);
    message += ", binding ";
    append_runtime_binding_summary(message, plan.binding);
}

template <typename FactoryPtr, typename TypeIndex>
void append_runtime_binding_summary(
    std::string& message,
    const runtime_binding_record<FactoryPtr, TypeIndex>& binding) {
    message += "{ id ";
    message += std::to_string(binding.id);
    message += ", storage ";
    message += runtime_storage_kind_name(binding.storage_kind);
    message += ", shape ";
    message += runtime_binding_shape_kind_name(binding.shape_kind);
    message += ", interface ";
    append_type_name(message, binding.interface_type);
    message += ", registered ";
    append_type_name(message, binding.registered_type);
    message += ", stored ";
    append_type_name(message, binding.stored_type);
    message += ", source ";
    append_type_name(message, binding.source_type);
    message += ", cacheable ";
    append_runtime_bool(message, binding.cacheable);
    message += ", wrapper_enabled ";
    append_runtime_bool(message, binding.wrapper_enabled);
    message += ", value_borrowable ";
    append_runtime_bool(message, binding.value_borrowable);
    message += ", pointer_like ";
    append_runtime_bool(message, binding.pointer_like);
    message += ", supports_source_access ";
    append_runtime_bool(message, binding.supports_source_access);
    message += " }";
}

template <typename Factories>
void append_runtime_candidate_bindings(std::string& message, Factories& factories) {
    message += "\ncandidates:";
    bool first = true;
    for (auto&& entry : factories) {
        message += first ? " " : ", ";
        append_runtime_binding_summary(message, entry.second);
        first = false;
    }
}

inline void append_collection_plan(std::string& message,
                                   const collection_plan_summary& plan) {
    message += "collection ";
    append_type_name(message, plan.collection_type);
    message += ", aggregation ";
    message += collection_aggregation_kind_name(plan.aggregation_kind);
    message += ", element request ";
    append_type_name(message, plan.element_request_type);
    message += ", element lookup ";
    append_type_name(message, plan.element_lookup_type);
}

inline void append_lookup_plan(std::string& message,
                               const lookup_plan_summary& plan) {
    message += "lookup ";
    message += lookup_mode_name(plan.mode);
    message += ", request ";
    append_type_name(message, plan.request_type);
    message += ", lookup ";
    append_type_name(message, plan.lookup_type);
    if (!(plan.key_type == describe_type<void>())) {
        message += ", key ";
        append_type_name(message, plan.key_type);
    }
}

} // namespace detail
} // namespace dingo
