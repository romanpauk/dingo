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
constexpr published_resolution published_resolution_v() {
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
      published_resolution_v<Storage, Resolutions>()...};
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
struct binding_facts_v {
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

// The resolutions a runtime binding publishes: those of the storage but an
// owning wrapper of the registered type, as a value or an rvalue, which a
// request composes of the rvalue the binding publishes. The runtime engine
// composes such a wrapper where it is requested, instead of every binding of
// a container instantiating it. The static container instantiates only what
// is requested, so it keeps them published, which the compiler folds.
template <typename Interface, typename Storage> struct runtime_resolutions {
private:
  using all = typename binding_resolutions<Interface, Storage>::type;
  using registered = remove_cvref_t<typename Storage::type>;

  template <typename Target, typename List> struct publishes_target;
  template <typename Target, typename... Resolutions>
  struct publishes_target<Target, type_list<Resolutions...>>
      : std::bool_constant<(
            std::is_same_v<Target, typename Resolutions::target_type> || ...)> {
  };

  template <typename Target> static constexpr bool composed() {
    using wrapper = remove_cvref_t<Target>;
    if constexpr (std::is_lvalue_reference_v<Target> ||
                  std::is_pointer_v<Target> || !is_object_wrapper_v<wrapper> ||
                  !std::is_same_v<std::remove_cv_t<Interface>, registered> ||
                  std::is_same_v<wrapper, registered>) {
      return false;
    } else if constexpr (!std::is_same_v<std::remove_cv_t<typename type_traits<
                                             wrapper>::value_type>,
                                         registered>) {
      return false;
    } else if constexpr (!composes_v<Target>) {
      return false;
    } else {
      return publishes_target<composed_inner_t<Target>, all>::value;
    }
  }

  template <typename List> struct without_composed;
  template <typename... Resolutions>
  struct without_composed<type_list<Resolutions...>> {
    using type = type_list_cat_t<
        std::conditional_t<composed<typename Resolutions::target_type>(),
                           type_list<>, type_list<Resolutions>>...>;
  };

public:
  using type = typename without_composed<all>::type;
};

template <typename Interface, typename Storage>
using runtime_resolutions_t =
    typename runtime_resolutions<Interface, Storage>::type;

} // namespace detail
} // namespace dingo
