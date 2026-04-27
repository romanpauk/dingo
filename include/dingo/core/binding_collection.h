//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/exceptions.h>
#include <dingo/registration/collection_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_list.h>

#include <cstddef>
#include <utility>

namespace dingo::detail {

template <typename StaticRegistry, typename T, typename Key = void>
constexpr std::size_t static_collection_binding_count() {
    return type_list_size_v<typename StaticRegistry::template bindings<
        normalized_type_t<typename collection_traits<T>::resolve_type>, Key>>;
}

template <typename T, typename RuntimeCountFn>
std::size_t count_binding_collection(RuntimeCountFn&& runtime_count,
                                     std::size_t static_count) {
    return std::forward<RuntimeCountFn>(runtime_count)() + static_count;
}

template <typename T, typename RuntimeAppendFn, typename StaticAppendFn,
          typename Fn>
std::size_t append_binding_collection(T& results,
                                      RuntimeAppendFn&& runtime_append,
                                      StaticAppendFn&& static_append, Fn&& fn) {
    std::size_t count = 0;
    count += std::forward<RuntimeAppendFn>(runtime_append)(results, fn);
    count += std::forward<StaticAppendFn>(static_append)(results, fn);
    return count;
}

template <typename T, typename PrimaryCountFn, typename SecondaryCountFn,
          typename PrimaryAppendFn, typename SecondaryAppendFn, typename Fn>
T construct_binding_collection(PrimaryCountFn&& primary_count,
                               SecondaryCountFn&& secondary_count,
                               PrimaryAppendFn&& primary_append,
                               SecondaryAppendFn&& secondary_append, Fn&& fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");

    const std::size_t total = std::forward<PrimaryCountFn>(primary_count)() +
                              std::forward<SecondaryCountFn>(secondary_count)();
    if (total == 0) {
        throw detail::make_collection_type_not_found_exception<T,
                                                               resolve_type>();
    }

    T results;
    collection_type::reserve(results, total);

    auto&& append = fn;
    std::forward<PrimaryAppendFn>(primary_append)(results, append);
    std::forward<SecondaryAppendFn>(secondary_append)(results, append);
    return results;
}

} // namespace dingo::detail
