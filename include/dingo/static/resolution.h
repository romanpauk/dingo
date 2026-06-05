//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/factory_traits.h>
#include <dingo/static/graph.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/request_traits.h>
#include <dingo/type/type_list.h>

#include <type_traits>

namespace dingo {
namespace detail {

template <typename StaticRegistry, bool DependenciesResolved>
struct static_container_graph_type;

template <typename StaticRegistry>
struct static_container_graph_type<StaticRegistry, true> {
    using type = static_graph<StaticRegistry>;
};

template <typename StaticRegistry>
struct static_container_graph_type<StaticRegistry, false> {
    struct type {
        static constexpr bool resolvable = true;
        static constexpr bool acyclic = true;
    };
};

struct factory_probe_context {
    template <typename T, typename Container> T resolve(Container&) = delete;
};

struct factory_probe_container {};

template <typename Factory>
inline constexpr bool factory_has_no_dependencies_v =
    std::is_same_v<typename factory_traits<Factory>::dependencies, type_list<>>;

template <typename Factory, typename Type>
Type construct_factory_value_without_dependencies() {
    factory_probe_context context;
    factory_probe_container container;
    return Factory::template construct<Type>(context, container);
}

template <typename Selection, typename Request,
          bool Enabled = Selection::status == binding_selection_status::found>
struct binding_factory {
    static constexpr bool enabled = false;
};

template <typename Selection, typename Request>
struct binding_factory<Selection, Request, true> {
    using binding_type = typename Selection::binding_type;
    using factory_type =
        typename binding_type::binding_model_type::factory_type;

    static constexpr bool enabled = factory_has_no_dependencies_v<factory_type>;

    static normalized_type_t<Request> construct() {
        return construct_factory_value_without_dependencies<
            factory_type, normalized_type_t<Request>>();
    }
};

template <typename Request, typename Selection, typename ResolveNormalized>
request_result_t<Request>
construct_static_binding_value(ResolveNormalized&& resolve_normalized) {
    if constexpr (binding_factory<Selection, Request>::enabled) {
        return type_traits<std::decay_t<Request>>::make(
            binding_factory<Selection, Request>::construct());
    } else {
        auto&& value = std::forward<ResolveNormalized>(resolve_normalized)();
        return type_traits<std::decay_t<Request>>::make(
            std::forward<decltype(value)>(value));
    }
}

} // namespace detail
} // namespace dingo
