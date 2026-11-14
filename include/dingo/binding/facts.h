//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/flags.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/base/type_list.h>
#include <dingo/binding/activation.h>
#include <dingo/request/derivation.h>
#include <dingo/storage/scope_traits.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/annotated.h>

#include <type_traits>

namespace dingo {
namespace detail {

// The resolution a binding of a storage publishes, as data.
template <typename Storage, typename Resolution>
constexpr published_resolution published_resolution_of() {
  using target = typename Resolution::target_type;
  published_resolution resolution{described_type_v<target>, {}, false, false};
  if constexpr (std::is_pointer_v<target>) {
    using pointee = std::remove_pointer_t<target>;
    if constexpr (adopts_made_pointer_v<Storage, pointee>) {
      resolution.adopted = described_type_v<pointee &>;
      resolution.adopts = true;
      using value = std::remove_cv_t<pointee>;
      resolution.takes = takes_made_pointee_v<Storage> &&
                         std::is_constructible_v<value, value &&>;
    }
  }
  return resolution;
}

template <typename Storage, typename Resolutions>
struct published_resolution_table;

template <typename Storage, typename... Resolutions>
struct published_resolution_table<Storage, type_list<Resolutions...>> {
  static constexpr published_resolution value[] = {
      published_resolution_of<Storage, Resolutions>()...};
};

template <typename Storage>
struct published_resolution_table<Storage, type_list<>> {
  static constexpr const published_resolution *value = nullptr;
};

// What a binding of an interface and a storage tells of itself: the
// resolutions it publishes and what it is. Both containers decide a request of
// it from this data, by the same functions.
template <typename Interface, typename Storage,
          typename Resolutions =
              typename binding_resolutions<Interface, Storage>::type>
struct binding_facts_of {
  using interface_type = std::remove_cv_t<Interface>;
  static constexpr binding_facts value{
      published_resolution_table<Storage, Resolutions>::value,
      type_list_size_v<Resolutions>,
      flags<binding_trait>{}
          .set(binding_trait::keeps, binding_cell<Storage>::keeps_conversions)
          .set(binding_trait::places, places_made_values_v<Storage>)
          .set(binding_trait::publishes_early,
               scope_allows_cycles_v<typename Storage::tag_type>),
      slices_v<interface_type, leaf_type_t<typename Storage::type>>,
      described_type_v<interface_type>};
};

} // namespace detail
} // namespace dingo
