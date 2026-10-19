//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/conversion/candidates.h>
#include <dingo/conversion/conversions.h>
#include <dingo/type/type_traits.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {
enum class basic_conversion_kind { none, identity, reference, converted };

// Taking the address of a compatible lvalue is what the fallback walk selects
// once the direct conversion has failed. A source that is an alternative type
// is probed per alternative first, so only the identity address, which no
// alternative can provide, is shortened here.
//
// The predicates of basic_type_conversion_path are function templates rather
// than lambdas in its member initializers: MSVC's pre-C++20 lambda processor
// fails on those lambdas for some instantiations.
template <typename Target, typename Source, bool UsesDefaultConversion>
constexpr bool basic_conversion_takes_address() {
  using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;
  bool result = false;
  if constexpr (UsesDefaultConversion && std::is_pointer_v<Target> &&
                std::is_lvalue_reference_v<Source> &&
                !std::is_pointer_v<source_type> &&
                std::is_convertible_v<std::remove_reference_t<Source> *,
                                      Target>) {
    using const_source = std::add_lvalue_reference_t<
        std::add_const_t<std::remove_reference_t<Source>>>;
    if constexpr (!is_alternative_type_v<source_type> ||
                  std::is_volatile_v<std::remove_reference_t<Source>> ||
                  std::is_same_v<
                      std::remove_cv_t<std::remove_pointer_t<Target>>,
                      source_type>) {
      // A direct conversion has priority over taking the address.
      result = !std::is_constructible_v<Target, Source> &&
               !std::is_constructible_v<Target, const_source>;
    }
  }
  return result;
}

template <typename Target, typename Source, typename Access,
          bool UsesDefaultConversion>
constexpr basic_conversion_kind basic_conversion_selected() {
  using kind = basic_conversion_kind;
  using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;
  kind result = kind::none;
  if constexpr (!UsesDefaultConversion || std::is_pointer_v<Target>) {
    // Custom conversions are resolved by the direct path.
  } else if constexpr (std::is_lvalue_reference_v<Target>) {
    if constexpr (std::is_lvalue_reference_v<Source> &&
                  access_satisfies<Access, borrow>::value &&
                  std::is_convertible_v<
                      std::add_pointer_t<std::remove_reference_t<Source>>,
                      std::add_pointer_t<std::remove_reference_t<Target>>>) {
      result = kind::converted;
    }
  } else if constexpr (!std::is_rvalue_reference_v<Target> &&
                       !std::is_volatile_v<std::remove_reference_t<Source>>) {
    if constexpr (std::is_lvalue_reference_v<Source>) {
      if constexpr (access_satisfies<Access, borrow>::value &&
                    std::is_same_v<std::remove_cv_t<Target>, source_type>) {
        if constexpr (is_copy_constructible_v<Target>) {
          result = kind::identity;
        }
      } else if constexpr (access_satisfies<Access, borrow>::value &&
                           std::is_convertible_v<
                               std::add_pointer_t<
                                   std::remove_reference_t<Source>>,
                               std::add_pointer_t<Target>>) {
        if constexpr (is_copy_constructible_v<Target>) {
          result = kind::reference;
        }
      }
    } else if constexpr (std::is_rvalue_reference_v<Source> &&
                         std::is_same_v<std::remove_cv_t<Target>,
                                        source_type> &&
                         access_satisfies<Access, consume>::value) {
      if constexpr (std::is_constructible_v<Target, Source>) {
        result = kind::identity;
      }
    }
  }
  return result;
}

// Keep common default conversions out of the general recursive search.
// Predicates are evaluated behind if constexpr so a short-circuit also skips
// the type trait probes of the later operands.
template <typename Target, typename Source, typename Access>
struct basic_type_conversion_path {
private:
  using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;
  using conversion_traits = type_conversion_traits<Target, source_type>;
  static constexpr bool uses_default_conversion =
      is_default_type_conversion<conversion_traits>::value;

  using kind = basic_conversion_kind;

  static constexpr bool takes_address =
      basic_conversion_takes_address<Target, Source, uses_default_conversion>();

  static constexpr kind selected =
      basic_conversion_selected<Target, Source, Access,
                                uses_default_conversion>();

public:
  using type = std::conditional_t<
      selected == kind::identity, identity_type_conversion<Target, Source>,
      std::conditional_t<
          selected == kind::reference,
          reference_type_conversion<Target, Source>,
          std::conditional_t<
              selected == kind::converted,
              traits_type_conversion<Target, Source, borrow, Source>,
              std::conditional_t<takes_address,
                                 address_type_conversion<Target, Source>,
                                 unavailable_type_conversion>>>>;
  static constexpr bool available = type::available;
};

template <typename Target, typename Source, typename Access>
struct direct_type_conversion_path {
private:
  // Identity and reference-preserving conversions are handled by the basic
  // path before this fallback is instantiated.
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;
  using conversion_traits = type_conversion_traits<Target, source_type>;

public:
  static constexpr bool uses_default_conversion =
      is_default_type_conversion<conversion_traits>::value;

private:
  static constexpr bool borrows_existing_object = [] {
    if constexpr (std::is_lvalue_reference_v<Target> &&
                  std::is_lvalue_reference_v<Source>) {
      return std::is_convertible_v<
          std::add_pointer_t<std::remove_reference_t<Source>>,
          std::add_pointer_t<std::remove_reference_t<Target>>>;
    } else {
      return false;
    }
  }();
  static constexpr bool conversion_takes_ownership = [] {
    if constexpr (!borrows_existing_object) {
      return type_traits<target_type>::is_owning_handle &&
             !type_traits<source_type>::is_owning_handle;
    } else {
      return false;
    }
  }();
  using const_source = std::add_lvalue_reference_t<
      std::add_const_t<std::remove_reference_t<Source>>>;
  static constexpr bool has_const_conversion = [] {
    if constexpr (uses_default_conversion && !borrows_existing_object &&
                  !conversion_takes_ownership &&
                  std::is_lvalue_reference_v<Source>) {
      return has_compatible_conversion<Target, conversion_traits,
                                       const_source>::value;
    } else {
      return false;
    }
  }();
  using default_required_access = std::conditional_t<
      std::is_rvalue_reference_v<Source> || conversion_takes_ownership ||
          (std::is_lvalue_reference_v<Source> && !borrows_existing_object &&
           !has_const_conversion),
      consume, borrow>;
  using custom_required_access =
      typename custom_conversion_access<conversion_traits, Source>::type;
  using required_access =
      std::conditional_t<uses_default_conversion, default_required_access,
                         custom_required_access>;
  using conversion_source =
      std::conditional_t<uses_default_conversion &&
                             std::is_same_v<default_required_access, borrow> &&
                             std::is_lvalue_reference_v<Source> &&
                             !borrows_existing_object,
                         const_source, Source>;
  static constexpr bool has_traits_conversion =
      has_compatible_conversion<Target, conversion_traits,
                                conversion_source>::value;
  static constexpr bool has_valid_custom_access =
      std::is_same_v<custom_required_access, borrow> ||
      std::is_same_v<custom_required_access, consume>;
  static_assert(uses_default_conversion || !has_traits_conversion ||
                    has_valid_custom_access,
                "type_conversion_traits specialization must declare "
                "required_access<Source> as borrow or consume");
  static constexpr bool takes_address = [] {
    if constexpr (std::is_pointer_v<Target> &&
                  std::is_lvalue_reference_v<Source>) {
      return std::is_convertible_v<
          std::add_pointer_t<std::remove_reference_t<Source>>, Target>;
    } else {
      return false;
    }
  }();
  using direct =
      std::conditional_t<has_traits_conversion &&
                             (!uses_default_conversion ||
                              !is_structural_conversion_target_v<Target>),
                         traits_type_conversion<Target, Source, required_access,
                                                conversion_source>,
                         unavailable_type_conversion>;

public:
  static constexpr bool can_take_address = takes_address;
  using type = conversion_for_access_t<direct, Access>;
  static constexpr bool available = type::available;
};
} // namespace detail
} // namespace dingo
