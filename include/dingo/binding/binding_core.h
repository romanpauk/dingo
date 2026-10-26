//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/binding/binding_resolution.h>
#include <dingo/binding/resolution_route.h>

#include <utility>

namespace dingo {
namespace detail {

// The typed execution layer after selection: materialize the source, apply the
// selected resolution, retain conversions, and produce an address or value.
// Selection, storage ownership, and dispatch stay in the policy base.
template <typename Policy> class binding_core : public Policy {
public:
  using storage_type = typename Policy::storage_type;

  explicit binding_core(Policy policy) : Policy(std::move(policy)) {}

  template <typename Request, typename Resolution, typename Context>
  void *resolve_address(Context &context) {
    using operation = typename Resolution::operation;
    auto activation = this->activation();
    auto &storage = this->slot().storage();
    auto materialize = [&](auto &&source) -> void * {
      return resolution_route<storage_type, Resolution>::apply(
                 this->scope, activation, context,
                 std::forward<decltype(source)>(source),
                 describe_type<Request>(), this->registered_type())
          .address;
    };
    return this->template with_source<operation, Request>(
        this->scope, context, storage, activation, std::move(materialize));
  }

  template <typename Request, typename Resolution, typename Context,
            typename Fn>
  decltype(auto) consume_request(Context &context, Fn &&fn) {
    using operation = typename Resolution::operation;
    auto activation = this->activation();
    auto &storage = this->slot().storage();
    auto consume = [&](auto &&source) -> decltype(auto) {
      return apply_consumed_resolution<Request, Resolution, storage_type>(
          activation, context, std::forward<decltype(source)>(source),
          std::forward<Fn>(fn));
    };
    return this->template with_source<operation, Request>(
        this->scope, context, storage, activation, std::move(consume));
  }

  template <typename T, typename Context>
  decltype(auto) resolve_value(Context &context) {
    auto activation = this->activation();
    auto &storage = this->slot().storage();
    auto resolve = [&](auto &&source) -> decltype(auto) {
      return resolve_binding_value<T>(activation, context,
                                      std::forward<decltype(source)>(source));
    };
    return this->with_value_source(this->scope, context, storage, activation,
                                   std::move(resolve));
  }

  template <typename... Resolutions, typename Context>
  resolved_address
  resolve_candidates(type_list<Resolutions...>, std::size_t index,
                     type_descriptor requested_type, Context &context) {
    using candidates_type = resolution_candidates<storage_type, Resolutions...>;
    auto activation = this->activation();
    auto materialize = [&](auto &&source) -> resolved_address {
      return resolve_candidate(
          candidates_type{}, index, this->scope, activation, context,
          std::forward<decltype(source)>(source), requested_type,
          binding_registered_type<storage_type>());
    };
    return this->with_source(this->scope, context, std::move(materialize));
  }
};

} // namespace detail
} // namespace dingo
