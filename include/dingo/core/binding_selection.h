//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/type/type_list.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

enum class binding_selection_status {
    found,
    not_found,
    ambiguous,
};

template <binding_selection_status Status, typename Binding = void>
struct binding_choice {
    static constexpr binding_selection_status status = Status;
    static constexpr bool found = Status == binding_selection_status::found;
    using binding_type = Binding;
};

template <typename Binding>
using found_binding_choice_t =
    binding_choice<binding_selection_status::found, Binding>;

using missing_binding_choice_t =
    binding_choice<binding_selection_status::not_found>;

using ambiguous_binding_choice_t =
    binding_choice<binding_selection_status::ambiguous>;

template <typename Bindings> struct static_binding;

template <> struct static_binding<type_list<>> {
    using type = missing_binding_choice_t;
};

template <typename Binding> struct static_binding<type_list<Binding>> {
    using type = found_binding_choice_t<Binding>;
};

template <typename Binding0, typename Binding1, typename... Bindings>
struct static_binding<type_list<Binding0, Binding1, Bindings...>> {
    using type = ambiguous_binding_choice_t;
};

template <typename Bindings>
using static_binding_t = typename static_binding<Bindings>::type;

template <typename Binding, typename State = std::nullptr_t>
struct runtime_binding_route {
    binding_selection_status status = binding_selection_status::not_found;
    Binding* binding = nullptr;
    State state = nullptr;

    constexpr bool found() const {
        return status == binding_selection_status::found;
    }

    constexpr bool ambiguous() const {
        return status == binding_selection_status::ambiguous;
    }

    static constexpr runtime_binding_route found(Binding& binding,
                                                 State state = nullptr) {
        return {binding_selection_status::found, &binding, state};
    }

    static constexpr runtime_binding_route miss() { return {}; }

    static constexpr runtime_binding_route ambiguity() {
        return {binding_selection_status::ambiguous, nullptr, nullptr};
    }
};

template <typename Binding, typename State = std::nullptr_t>
constexpr runtime_binding_route<Binding, State>
make_runtime_route(Binding* binding, State state = nullptr) {
    return binding
               ? runtime_binding_route<Binding, State>::found(*binding, state)
               : runtime_binding_route<Binding, State>::miss();
}

template <typename Binding, typename State = std::nullptr_t, typename Visitor>
constexpr runtime_binding_route<Binding, State>
make_runtime_route(Visitor&& visit_candidates) {
    Binding* routed_binding = nullptr;
    State routed_state = nullptr;
    std::size_t matches = 0;

    std::forward<Visitor>(visit_candidates)(
        [&](Binding& binding, State state = nullptr) {
            ++matches;
            if (matches == 1) {
                routed_binding = &binding;
                routed_state = state;
            }
        });

    if (matches == 0) {
        return runtime_binding_route<Binding, State>::miss();
    }

    if (matches == 1) {
        return runtime_binding_route<Binding, State>::found(*routed_binding,
                                                            routed_state);
    }

    return runtime_binding_route<Binding, State>::ambiguity();
}

} // namespace detail
} // namespace dingo
