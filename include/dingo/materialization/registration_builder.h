//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/exceptions.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/type_registration.h>
#include <dingo/resolution/instance_factory.h>
#include <dingo/resolution/interface_storage_traits.h>
#include <dingo/resolution/registration_plan_format.h>
#include <dingo/resolution/runtime_execution_plan.h>

#include <cassert>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

struct registration_metadata {
    std::size_t binding_id;
    type_descriptor interface_type;
    type_descriptor registered_storage_type;
    type_descriptor index_key_type;
    bool indexed;
    void (*append_plan)(std::string&);
};

template <typename BindingRecord, typename OwnedFactoryType>
struct registry_entry {
    BindingRecord binding;
    registration_metadata registration;
    OwnedFactoryType owned_factory;
};

template <typename ReturnContainer, typename InterfaceList,
          typename RegisteredStorageType, typename... Entries>
struct registration_descriptor {
    using interface_types = InterfaceList;
    using registered_storage_type = RegisteredStorageType;

    ReturnContainer* container;
    std::tuple<Entries...> entries;
};

template <typename RegistrationPlan>
void append_registration_metadata(std::string& message) {
    append_registration_plan<RegistrationPlan>(message);
}

template <typename Container> struct registration_builder {
    using binding_record_type = typename Container::binding_record;
    using owned_factory_type = instance_factory_ptr<
        instance_factory_interface<typename Container::container_type>>;

    template <typename RegistrationPlan>
    using interface_wrapper_t =
        interfaces<typename RegistrationPlan::interface_types>;

    template <typename RegistrationPlan>
    using instance_container_t = typename Container::template rebind_t<
        typename Container::container_traits_type::template rebind_t<
            type_list<typename Container::container_traits_type::tag_type,
                      interface_wrapper_t<RegistrationPlan>>>,
        typename Container::allocator_type, typename Container::container_type>;

    template <typename RegistrationPlan>
    using factory_data_t = instance_factory_data<
        instance_container_t<RegistrationPlan>,
        typename RegistrationPlan::storage_type>;

    template <typename RegistrationPlan>
    using source_type_t = storage_resolve_result_t<
        typename RegistrationPlan::storage_type, resolving_context,
        instance_container_t<RegistrationPlan>>;

    template <typename RegistrationPlan, typename Arg>
    static auto build(Container& container, Arg&& arg) {
        if constexpr (RegistrationPlan::materialization_kind ==
                      ir::registration_materialization_kind::
                          single_interface) {
            if constexpr (RegistrationPlan::payload_kind !=
                          ir::registration_payload_kind::default_constructed) {
                return build_single_registration<RegistrationPlan>(
                    container, std::forward<Arg>(arg));
            } else {
                return build_single_registration<RegistrationPlan>(
                    container);
            }
        } else {
            if constexpr (RegistrationPlan::payload_kind !=
                          ir::registration_payload_kind::default_constructed) {
                return build_shared_registration<RegistrationPlan>(
                    container, std::forward<Arg>(arg));
            } else {
                return build_shared_registration<RegistrationPlan>(
                    container);
            }
        }
    }

  private:

    template <typename RegistrationPlan, typename TypeInterface,
              typename TypeStorage, typename SourceType, typename Factory>
    static auto make_entry(Factory&& factory) {
        check_interface_requirements<
            TypeStorage, typename annotated_traits<TypeInterface>::type,
            typename TypeStorage::type>();

        using concrete_factory_type =
            std::remove_pointer_t<decltype(factory.get())>;
        auto factory_ptr =
            static_cast<instance_factory_interface<typename Container::container_type>*>(
                factory.get());
        return registry_entry<binding_record_type, owned_factory_type>{
            make_binding_record<TypeInterface, TypeStorage, SourceType,
                                concrete_factory_type>(factory_ptr),
            make_registration_metadata<RegistrationPlan, TypeInterface>(),
            std::forward<Factory>(factory)};
    }

    template <typename RegistrationPlan, typename... Args>
    static auto build_single_registration(Container& container,
                                          Args&&... args) {
        using interface_type =
            type_list_head_t<typename RegistrationPlan::interface_types>;
        using storage_type = typename RegistrationPlan::storage_type;
        using instance_factory_type = instance_factory<
            typename Container::container_type,
            typename annotated_traits<interface_type>::type, storage_type,
            factory_data_t<RegistrationPlan>>;

        auto&& [factory, factory_container] =
            allocate_factory<instance_factory_type>(container, &container,
                                                    std::forward<Args>(args)...);
        using entry_type = registry_entry<binding_record_type, owned_factory_type>;
        return registration_descriptor<typename instance_factory_type::container_type,
                                       type_list<interface_type>,
                                       typename storage_type::type,
                                       entry_type>{
            factory_container,
            std::make_tuple(make_entry<RegistrationPlan, interface_type,
                                       storage_type,
                                       source_type_t<RegistrationPlan>>(
                std::move(factory)))};
    }

    template <typename RegistrationPlan, typename... Args>
    static auto build_shared_registration(Container& container,
                                          Args&&... args) {
        auto data = std::allocate_shared<factory_data_t<RegistrationPlan>>(
            allocator_traits::rebind<factory_data_t<RegistrationPlan>>(
                container.get_allocator()),
            &container, std::forward<Args>(args)...);
        return make_shared_descriptor<RegistrationPlan>(
            container, data, typename RegistrationPlan::interface_types{});
    }

    template <class Storage, class TypeInterface, class Type>
    static void check_interface_requirements() {
        using normalized_type = normalized_type_t<Type>;
        using normalized_interface_type = normalized_type_t<TypeInterface>;
        constexpr bool is_alternative_type_interface =
            is_alternative_type_interface_compatible_v<normalized_type,
                                                       normalized_interface_type>;

        static_assert(!std::is_reference_v<TypeInterface>);
        if constexpr (detail::is_array_like_type_v<Type>) {
            if constexpr (std::is_array_v<TypeInterface>) {
                using exact_interface_type =
                    detail::array_like_exact_interface_type_t<Type>;
                static_assert(
                    std::is_same_v<std::remove_cv_t<TypeInterface>,
                                   std::remove_cv_t<exact_interface_type>> ||
                        (std::is_array_v<Type> && (std::rank_v<Type> > 1) &&
                         std::is_same_v<std::remove_cv_t<TypeInterface>,
                                        std::remove_cv_t<std::remove_extent_t<Type>>>),
                    "array registrations require matching array-shape interfaces");
            } else {
                static_assert(
                    std::is_same_v<normalized_type, normalized_interface_type>,
                    "array registrations require matching element-type interfaces");
            }
        }
        static_assert(
            std::is_convertible_v<normalized_type*, normalized_interface_type*> ||
            is_alternative_type_interface,
            "registered type must be pointer-convertible to the interface");
        if constexpr (!std::is_same_v<normalized_type,
                                      normalized_interface_type>) {
            static_assert(storage_interface_requirements_v<
                              Storage, normalized_type,
                              normalized_interface_type>,
                          "storage requirements not met");
        }
    }

    template <typename FactoryType, typename... Args>
    static std::pair<
        instance_factory_ptr<instance_factory_interface<typename Container::container_type>>,
        typename FactoryType::container_type*>
    allocate_factory(Container& container, Args&&... args) {
        auto alloc = allocator_traits::rebind<FactoryType>(container.get_allocator());
        FactoryType* instance = allocator_traits::allocate(alloc, 1);
        if (!instance) {
            return {nullptr, nullptr};
        }

        allocator_traits::construct(alloc, instance, std::forward<Args>(args)...);
        instance->cacheable = FactoryType::storage_type::cacheable;

        return {instance, &instance->get_container()};
    }

    template <typename TypeInterface, typename TypeStorage, typename SourceType,
              typename ConcreteFactory>
    static typename Container::binding_record make_binding_record(
        instance_factory_interface<typename Container::container_type>* factory) {
        return make_runtime_binding_record<typename Container::rtti_type,
                                           TypeInterface, TypeStorage,
                                           SourceType, ConcreteFactory>(
            0, factory);
    }

    template <typename RegistrationPlan, typename InterfaceType>
    static registration_metadata make_registration_metadata() {
        using key_type = typename RegistrationPlan::template key<InterfaceType>;
        return {
            0,
            describe_type<typename key_type::interface_type>(),
            describe_type<typename key_type::registered_storage_type>(),
            key_type::indexed
                ? describe_type<typename key_type::index_key_type>()
                : type_descriptor{},
            key_type::indexed,
            &append_registration_metadata<RegistrationPlan>};
    }

    template <typename RegistrationPlan, typename SharedData,
              typename... Interfaces>
    static auto make_shared_descriptor(Container& container, SharedData& data,
                                       type_list<Interfaces...>) {
        using descriptor_type = registration_descriptor<
            typename factory_data_t<RegistrationPlan>::container_type,
            typename RegistrationPlan::interface_types,
            typename RegistrationPlan::storage_type::type,
            std::conditional_t<
                true, registry_entry<binding_record_type, owned_factory_type>,
                Interfaces>...>;

        return descriptor_type{
            &data->container,
            std::make_tuple(make_shared_entry<RegistrationPlan, Interfaces>(
                container, data)... )};
    }

    template <typename RegistrationPlan, typename InterfaceType,
              typename SharedData>
    static auto make_shared_entry(Container& container, SharedData& data) {
        using instance_factory_type = instance_factory<
            typename Container::container_type,
            typename annotated_traits<InterfaceType>::type,
            typename RegistrationPlan::storage_type,
            std::shared_ptr<factory_data_t<RegistrationPlan>>>;

        return make_entry<RegistrationPlan, InterfaceType,
                          typename RegistrationPlan::storage_type,
                          source_type_t<RegistrationPlan>>(
            allocate_factory<instance_factory_type>(container, data).first);
    }
};

} // namespace detail
} // namespace dingo
