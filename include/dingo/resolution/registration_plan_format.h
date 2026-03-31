//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/exceptions.h>
#include <dingo/resolution/ir.h>
#include <dingo/type/type_list.h>

#include <string>

namespace dingo {
namespace detail {

constexpr const char* registration_materialization_kind_name(
    ir::registration_materialization_kind kind) {
    switch (kind) {
    case ir::registration_materialization_kind::single_interface:
        return "single_interface";
    case ir::registration_materialization_kind::shared_factory_data:
        return "shared_factory_data";
    }

    return "single_interface";
}

constexpr const char* registration_payload_kind_name(
    ir::registration_payload_kind kind) {
    switch (kind) {
    case ir::registration_payload_kind::default_constructed:
        return "default_constructed";
    case ir::registration_payload_kind::external_instance:
        return "external_instance";
    case ir::registration_payload_kind::factory_payload:
        return "factory_payload";
    }

    return "default_constructed";
}

template <typename Interfaces>
void append_interface_types(std::string& message) {
    bool first = true;
    for_each(Interfaces{}, [&](auto interface_type) {
        if (!first) {
            message += ", ";
        }
        append_type_name(message,
                         describe_type<typename decltype(interface_type)::type>());
        first = false;
    });
}

template <typename RegistrationPlan>
void append_registration_plan(std::string& message) {
    message += "interfaces [";
    append_interface_types<typename RegistrationPlan::interface_types>(message);
    message += "], scope ";
    append_type_name(message, describe_type<typename RegistrationPlan::scope_tag>());
    message += ", registered storage ";
    append_type_name(
        message,
        describe_type<typename RegistrationPlan::registered_storage_type>());
    message += ", stored ";
    append_type_name(message,
                     describe_type<typename RegistrationPlan::stored_type>());
    message += ", factory ";
    append_type_name(message,
                     describe_type<typename RegistrationPlan::factory_type>());
    if constexpr (RegistrationPlan::indexed) {
        message += ", index key ";
        append_type_name(
            message,
            describe_type<typename RegistrationPlan::index_key_type>());
    }
    message += ", payload ";
    message += registration_payload_kind_name(RegistrationPlan::payload_kind);
    message += ", materialization ";
    message += registration_materialization_kind_name(
        RegistrationPlan::materialization_kind);
}

} // namespace detail
} // namespace dingo
