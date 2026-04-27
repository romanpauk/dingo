//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/core/factory_traits.h>
#include <dingo/registration/constructor.h>
#include <dingo/resolution/conversion_cache.h>
#include <dingo/resolution/runtime_binding.h>
#include <dingo/core/binding_model.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/rebind_type.h>
#include <dingo/rtti/rtti.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/registration/type_registration.h>

#include <gtest/gtest.h>

using namespace dingo;

namespace {
int build_from_double_and_cstr(double, const char*) { return 0; }
}

TEST(type_registration_test, get_type) {
    struct A {};
    using list = type_list<storage<A>, factory<A>, interfaces<A, A>>;
    using storage_type = detail::get_type_t<storage<void>, list>;
    static_assert(std::is_same_v<storage_type, storage<A>>);

    using factory_type = detail::get_type_t<factory<void>, list>;
    static_assert(std::is_same_v<factory_type, factory<A>>);

    using interface_type = detail::get_type_t<interfaces<void>, list>;
    static_assert(std::is_same_v<interface_type, interfaces<A, A>>);
}

TEST(type_registration_test, registration_basic) {
    using registration =
        type_registration<storage<int>, interfaces<double, float>, factory<int>,
                          scope<int>>;

    static_assert(
        std::is_same_v<typename registration::storage_type, storage<int>>);
    static_assert(std::is_same_v<typename registration::interface_type,
                                 interfaces<double, float>>);
    static_assert(
        std::is_same_v<typename registration::factory_type, factory<int>>);
    static_assert(
        std::is_same_v<typename registration::scope_type, scope<int>>);
}

TEST(type_registration_test, registration_prefers_first_explicit_match) {
    using registration = type_registration<
        scope<int>, scope<float>, storage<double>, storage<char>,
        factory<long>, factory<short>, interfaces<unsigned>, interfaces<bool>,
        conversions<void*>, conversions<int*>, dependencies<long>,
        dependencies<short>>;

    static_assert(std::is_same_v<typename registration::scope_type, scope<int>>);
    static_assert(
        std::is_same_v<typename registration::storage_type, storage<double>>);
    static_assert(
        std::is_same_v<typename registration::factory_type, factory<long>>);
    static_assert(std::is_same_v<typename registration::interface_type,
                                 interfaces<unsigned>>);
    static_assert(std::is_same_v<typename registration::conversions_type,
                                 conversions<void*>>);
    static_assert(std::is_same_v<typename registration::dependencies_type,
                                 dependencies<long>>);
}

TEST(type_registration_test, registration_deduction) {
    static_assert(
        std::is_same_v<
            typename type_registration<scope<int>, factory<int>>::storage_type,
            storage<int>>);
    static_assert(
        std::is_same_v<typename type_registration<scope<int>,
                                                  factory<int>>::interface_type,
                       interfaces<int>>);

    static_assert(
        std::is_same_v<typename type_registration<scope<int>,
                                                  storage<int>>::interface_type,
                       interfaces<int>>);
    static_assert(
        std::is_same_v<
            typename type_registration<scope<int>, storage<int>>::factory_type,
            factory<constructor<int>>>);
}

TEST(type_registration_test, registration_default_interface_and_conversions) {
    using registration_from_factory = type_registration<scope<shared>, factory<int>>;
    static_assert(std::is_same_v<typename registration_from_factory::storage_type,
                                 storage<int>>);
    static_assert(
        std::is_same_v<typename registration_from_factory::interface_type,
                       interfaces<int>>);
    static_assert(
        std::is_same_v<typename registration_from_factory::conversions_type,
                       conversions<detail::conversions<shared, int, runtime_type>>>);

    using registration_from_storage = type_registration<scope<shared>, storage<int>>;
    static_assert(
        std::is_same_v<typename registration_from_storage::interface_type,
                       interfaces<int>>);
    static_assert(
        std::is_same_v<typename registration_from_storage::conversions_type,
                       conversions<detail::conversions<shared, int, runtime_type>>>);
}

TEST(type_registration_test, registration_dependencies_default_from_factory_traits) {
    struct service {
        explicit service(int, float) {}
    };
    struct selected_service {
        DINGO_CONSTRUCTOR(selected_service(int, float)) {}
    };

    using ctor_registration =
        type_registration<scope<unique>, storage<service>,
                          factory<constructor<service(int, float)>>>;
    static_assert(std::is_same_v<typename ctor_registration::dependencies_type,
                                 dependencies<int, float>>);

    using detected_registration =
        type_registration<scope<unique>, storage<selected_service>>;
    static_assert(
        std::is_same_v<typename detected_registration::dependencies_type,
                       dependencies<int, float>>);

    using function_registration =
        type_registration<scope<unique>, storage<int>,
                          factory<function<build_from_double_and_cstr>>>;
    static_assert(
        std::is_same_v<typename function_registration::dependencies_type,
                       dependencies<double, const char*>>);
}

TEST(type_registration_test, registration_dependencies_prefer_explicit_metadata) {
    struct service {
        explicit service(int) {}
    };

    using registration =
        type_registration<scope<unique>, storage<service>,
                          factory<constructor<service(int)>>,
                          dependencies<float, bool>>;
    static_assert(std::is_same_v<typename registration::dependencies_type,
                                 dependencies<float, bool>>);
}

TEST(type_registration_test,
     registration_explicit_dependencies_shape_default_constructor_factory) {
    struct service {
        service(int, float) {}
        service(double, bool) {}
    };

    using registration =
        type_registration<scope<unique>, storage<service>,
                          dependencies<double, bool>>;

    static_assert(std::is_same_v<typename registration::factory_type,
                                 factory<constructor<service(double, bool)>>>);
    static_assert(std::is_same_v<typename registration::dependencies_type,
                                 dependencies<double, bool>>);
}

TEST(type_registration_test, factory_traits_report_dependencies) {
    struct service {
        explicit service(int, float) {}
    };
    struct selected_service {
        DINGO_CONSTRUCTOR(selected_service(int, float)) {}
    };
    struct defaulted_service {};

    using constructor_factory = constructor<service(int, float)>;
    static_assert(std::is_same_v<typename factory_traits<
                                     constructor_factory>::dependencies,
                                 type_list<int, float>>);
    static_assert(factory_traits<constructor_factory>::has_explicit_dependencies);
    static_assert(factory_traits<constructor_factory>::is_compile_time_bindable);

    using detected_factory = constructor<selected_service>;
    static_assert(std::is_same_v<typename factory_traits<
                                     detected_factory>::dependencies,
                                 type_list<int, float>>);
    static_assert(!factory_traits<detected_factory>::has_explicit_dependencies);
    static_assert(factory_traits<detected_factory>::is_compile_time_bindable);

    using defaulted_factory = constructor<defaulted_service>;
    static_assert(std::is_same_v<typename factory_traits<
                                     defaulted_factory>::dependencies,
                                 type_list<>>);
    static_assert(!factory_traits<defaulted_factory>::has_explicit_dependencies);
    static_assert(factory_traits<defaulted_factory>::is_compile_time_bindable);

    using function_factory = function<build_from_double_and_cstr>;
    static_assert(std::is_same_v<typename factory_traits<
                                     function_factory>::dependencies,
                                 type_list<double, const char*>>);
    static_assert(factory_traits<function_factory>::has_explicit_dependencies);
    static_assert(factory_traits<function_factory>::is_compile_time_bindable);

    auto callable_factory = callable<int(short, long)>([](short, long) {
        return 0;
    });
    using callable_factory_type = decltype(callable_factory);
    static_assert(std::is_same_v<typename factory_traits<
                                     callable_factory_type>::dependencies,
                                 type_list<short, long>>);
    static_assert(factory_traits<callable_factory_type>::has_explicit_dependencies);
    static_assert(!factory_traits<callable_factory_type>::is_compile_time_bindable);
}

TEST(type_registration_test, registration_specialization) {
    struct I {
        virtual ~I() = default;
    };
    struct A : I {};
    struct test_container {
        using rtti_type = rtti<typeid_provider>;
        using allocator_type = std::allocator<char>;

        explicit test_container(test_container*,
                                allocator_type allocator = allocator_type())
            : allocator_(allocator) {}

        allocator_type& get_allocator() { return allocator_; }

      private:
        allocator_type allocator_;
    };

    using shared_registration = type_registration<scope<shared>, storage<A>>;
    using shared_storage = detail::storage<
        typename shared_registration::scope_type::type,
        typename shared_registration::storage_type::type,
        rebind_type_t<typename shared_registration::storage_type::type,
                      normalized_type_t<
                          typename shared_registration::storage_type::type>>,
        typename shared_registration::factory_type::type,
        typename shared_registration::conversions_type::type>;
    using shared_binding_data =
        runtime_binding_data<test_container, shared_storage>;
    using shared_binding = runtime_binding<test_container, A, shared_storage,
                                            shared_binding_data>;

    using shared_ptr_registration =
        type_registration<scope<shared>, storage<std::shared_ptr<A>>>;
    using shared_ptr_storage = detail::storage<
        typename shared_ptr_registration::scope_type::type,
        typename shared_ptr_registration::storage_type::type,
        rebind_type_t<typename shared_ptr_registration::storage_type::type,
                      normalized_type_t<typename shared_ptr_registration::
                                            storage_type::type>>,
        typename shared_ptr_registration::factory_type::type,
        typename shared_ptr_registration::conversions_type::type>;
    using shared_ptr_binding_data =
        runtime_binding_data<test_container, shared_ptr_storage>;
    using shared_ptr_binding =
        runtime_binding<test_container, A, shared_ptr_storage,
                         shared_ptr_binding_data>;

    using external_registration =
        type_registration<scope<external>, storage<A&>>;
    using external_storage = detail::storage<
        typename external_registration::scope_type::type,
        typename external_registration::storage_type::type,
        rebind_type_t<typename external_registration::storage_type::type,
                      normalized_type_t<typename external_registration::
                                            storage_type::type>>,
        typename external_registration::factory_type::type,
        typename external_registration::conversions_type::type>;
    using external_binding_data =
        runtime_binding_data<test_container, external_storage>;
    using external_binding =
        runtime_binding<test_container, A, external_storage,
                         external_binding_data>;

    using external_shared_registration =
        type_registration<scope<external>, storage<std::shared_ptr<A>>>;
    using external_shared_storage = detail::storage<
        typename external_shared_registration::scope_type::type,
        typename external_shared_registration::storage_type::type,
        rebind_type_t<typename external_shared_registration::storage_type::type,
                      normalized_type_t<typename external_shared_registration::
                                            storage_type::type>>,
        typename external_shared_registration::factory_type::type,
        typename external_shared_registration::conversions_type::type>;
    using external_shared_binding_data =
        runtime_binding_data<test_container, external_shared_storage>;
    using external_shared_binding =
        runtime_binding<test_container, A, external_shared_storage,
                         external_shared_binding_data>;

    using nested_registration =
        type_registration<scope<shared>,
                          storage<std::shared_ptr<std::unique_ptr<A>>>,
                          interfaces<I>>;
    using nested_storage_type = typename nested_registration::storage_type::type;
    using nested_interface_type =
        type_list_head_t<typename nested_registration::interface_type::type>;
    using nested_stored_type = rebind_leaf_t<
        nested_storage_type,
        std::conditional_t<
            std::has_virtual_destructor_v<nested_interface_type> &&
                is_interface_storage_rebindable_v<nested_storage_type,
                                                  nested_interface_type>,
            nested_interface_type, leaf_type_t<nested_storage_type>>>;

    // Nested wrapper requests can still borrow to the leaf interface, but
    // built-in smart handles only rebind when C++ itself has the direct handle
    // conversion.
    static_assert(is_interface_storage_rebindable_v<std::shared_ptr<A>, I>);
    static_assert(is_interface_storage_rebindable_v<std::unique_ptr<A>, I>);
    static_assert(
        !is_interface_storage_rebindable_v<std::shared_ptr<std::unique_ptr<A>>,
                                           I>);
    static_assert(
        !is_interface_storage_rebindable_v<std::unique_ptr<std::shared_ptr<A>>,
                                           I>);

    static_assert(
        std::is_same_v<nested_stored_type, std::shared_ptr<std::unique_ptr<A>>>);

    static_assert(std::is_empty_v<conversion_cache<type_list<>>>);
    static_assert(
        std::is_trivially_destructible_v<conversion_cache<type_list<>>>);
    static_assert(sizeof(shared_binding) <= sizeof(shared_ptr_binding));
    static_assert(sizeof(external_binding) <= sizeof(external_shared_binding));
}

TEST(type_registration_test, binding_model_rewrites_single_interface_storage_leaf) {
    struct I {
        virtual ~I() = default;
    };
    struct A : I {};

    using registration =
        type_registration<scope<shared>, storage<std::shared_ptr<A>>,
                          interfaces<I>>;
    using model = detail::binding_model<registration>;

    static_assert(model::storage_tag_is_complete);
    static_assert(model::use_interface_as_stored_leaf);
    static_assert(std::is_same_v<typename model::stored_leaf_type, I>);
    static_assert(std::is_same_v<typename model::stored_type,
                                 std::shared_ptr<I>>);
    static_assert(std::is_same_v<typename model::storage_type::stored_type,
                                 std::shared_ptr<I>>);
    static_assert(model::valid);
}

TEST(type_registration_test, binding_model_preserves_storage_for_multi_interface_registration) {
    struct I {
        virtual ~I() = default;
    };
    struct J {
        virtual ~J() = default;
    };
    struct A : I, J {};

    using registration =
        type_registration<scope<shared>, storage<std::shared_ptr<A>>,
                          interfaces<I, J>>;
    using model = detail::binding_model<registration>;
    using expansion = detail::binding_expansion<model>;

    static_assert(model::storage_tag_is_complete);
    static_assert(!model::use_interface_as_stored_leaf);
    static_assert(std::is_same_v<typename model::stored_leaf_type, A>);
    static_assert(std::is_same_v<typename model::stored_type,
                                 std::shared_ptr<A>>);
    static_assert(std::is_same_v<typename expansion::interface_bindings,
                                 type_list<
                                     detail::interface_binding<I, model>,
                                     detail::interface_binding<J, model>>>);
    static_assert(model::valid);
}

TEST(type_registration_test, recursive_leaf_and_rebind_traits) {
    struct I {
        virtual ~I() = default;
    };
    struct A : I {};

    using nested_handle = const std::shared_ptr<std::unique_ptr<A>>&;
    using nested_list = type_list<nested_handle, std::unique_ptr<A*>>;

    static_assert(std::is_same_v<leaf_type_t<nested_handle>, A>);
    static_assert(std::is_same_v<rebind_leaf_t<nested_handle, I>,
                                 std::shared_ptr<std::unique_ptr<I>>&>);
    static_assert(std::is_same_v<rebind_type_t<nested_handle, I>,
                                 std::shared_ptr<I>&>);
    static_assert(std::is_same_v<leaf_type_t<nested_list>,
                                 type_list<A, A>>);
    static_assert(std::is_same_v<rebind_leaf_t<nested_list, I>,
                                 type_list<std::shared_ptr<std::unique_ptr<I>>&,
                                           std::unique_ptr<I*>>>);

}

TEST(type_registration_test, normalized_type_trait) {
    struct I {
        virtual ~I() = default;
    };
    struct A : I {};
    struct annotation_tag {};

    static_assert(std::is_same_v<normalized_type_t<const std::shared_ptr<
                                     std::unique_ptr<A>>&>,
                                 A>);
    static_assert(
        std::is_same_v<normalized_type_t<annotated<I&, annotation_tag>>,
                       annotated<I, annotation_tag>>);
}
