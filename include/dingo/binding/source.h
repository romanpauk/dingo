//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/construction_scope.h>
#include <dingo/exposure/type_storage_traits.h>
#include <dingo/resolution/source.h>
#include <dingo/type/rebind.h>

#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

template <typename Context> struct retained_frame_scope {
  explicit retained_frame_scope(Context &context) : context_(context) {
    context_.push_frame(&frame_);
  }

  ~retained_frame_scope() { context_.pop_frame(); }

  retained_frame_scope(const retained_frame_scope &) = delete;
  retained_frame_scope &operator=(const retained_frame_scope &) = delete;

private:
  Context &context_;
  typename Context::frame_type frame_;
};

template <typename Storage, typename Context, typename Owner, typename Fn>
decltype(auto) with_binding_source(construction_scope scope, Context &context,
                                   Storage &storage, Owner &owner, Fn &&fn) {
  using materialization_traits =
      storage_materialization_traits<typename Storage::tag_type,
                                     typename Storage::type>;
  using leaf_type = leaf_type_t<typename Storage::type>;

  [[maybe_unused]] auto guard =
      materialization_traits::template make_guard<leaf_type>(context, storage);
  auto source = materialization_traits::materialize_source(scope, context,
                                                           storage, owner);
  return std::forward<Fn>(fn)(std::move(source));
}

template <typename MaterializationTraits, typename Context, typename Storage,
          typename Owner, typename = void>
struct has_context_materialize_source : std::false_type {};

template <typename MaterializationTraits, typename Context, typename Storage,
          typename Owner>
struct has_context_materialize_source<
    MaterializationTraits, Context, Storage, Owner,
    std::void_t<decltype(MaterializationTraits::materialize_source_in_context(
        std::declval<construction_scope>(), std::declval<Context &>(),
        std::declval<Storage &>(), std::declval<Owner &>()))>>
    : std::true_type {};

template <typename Storage, typename Context, typename Owner>
inline constexpr bool materializes_value_source_v = [] {
  using source_result = decltype(std::declval<Storage &>().resolve(
      std::declval<construction_scope>(), std::declval<Context &>(),
      std::declval<Owner &>()));
  using source_type = std::remove_cv_t<std::remove_reference_t<source_result>>;
  return !std::is_reference_v<source_result> && !std::is_pointer_v<source_type>;
}();

template <typename Storage, typename Context, typename Owner, typename Fn>
decltype(auto) with_tracked_binding_source(construction_scope scope,
                                           Context &context, Storage &storage,
                                           Owner &owner, Fn &&fn) {
  using materialization_traits =
      storage_materialization_traits<typename Storage::tag_type,
                                     typename Storage::type>;
  using leaf_type = leaf_type_t<typename Storage::type>;

  [[maybe_unused]] auto guard =
      materialization_traits::template make_guard<leaf_type>(context, storage);
  auto source = materialization_traits::materialize_source(scope, context,
                                                           storage, owner);
  return std::forward<Fn>(fn)(std::move(source));
}

template <typename Storage, typename Context, typename Owner, typename Fn>
decltype(auto) with_context_lvalue_source(construction_scope scope,
                                          Context &context, Storage &storage,
                                          Owner &owner, Fn &&fn) {
  using materialization_traits =
      storage_materialization_traits<typename Storage::tag_type,
                                     typename Storage::type>;
  using leaf_type = leaf_type_t<typename Storage::type>;
  using source_type =
      std::remove_cv_t<std::remove_reference_t<decltype(storage.resolve(
          scope, context, owner))>>;

  [[maybe_unused]] auto guard =
      materialization_traits::template make_guard<leaf_type>(context, storage);
  auto &source_value = context.template construct<source_type>(
      scope, storage.resolve(scope, context, owner));
  auto source = lvalue_source<source_type>(source_value);
  return std::forward<Fn>(fn)(source);
}

template <typename Storage, typename Context, typename Owner, typename Fn>
decltype(auto) with_retained_binding_source(construction_scope scope,
                                            Context &context, Storage &storage,
                                            Owner &owner, Fn &&fn) {
  using materialization_traits =
      storage_materialization_traits<typename Storage::tag_type,
                                     typename Storage::type>;
  using leaf_type = leaf_type_t<typename Storage::type>;

  [[maybe_unused]] auto guard =
      materialization_traits::template make_guard<leaf_type>(context, storage);
  if (materialization_traits::retains_source(storage)) {
    retained_frame_scope<Context> frame_scope(context);
    auto source = materialization_traits::materialize_source(
        persistent_scope, context, storage, owner);
    return std::forward<Fn>(fn)(std::move(source));
  }

  auto source = materialization_traits::materialize_source(scope, context,
                                                           storage, owner);
  return std::forward<Fn>(fn)(std::move(source));
}

template <typename Storage, typename Context, typename Owner, typename Fn>
decltype(auto) with_runtime_binding_source(construction_scope scope,
                                           Context &context, Storage &storage,
                                           Owner &owner, Fn &&fn) {
  using materialization_traits =
      storage_materialization_traits<typename Storage::tag_type,
                                     typename Storage::type>;
  using leaf_type = leaf_type_t<typename Storage::type>;

  [[maybe_unused]] auto guard =
      materialization_traits::template make_guard<leaf_type>(context, storage);
  if constexpr (has_context_materialize_source<materialization_traits, Context,
                                               Storage, Owner>::value) {
    const bool retains_source =
        materialization_traits::retains_source(storage) ||
        scope.is_persistent();
    auto &source = [&]() -> auto & {
      if (retains_source) {
        return materialization_traits::materialize_source_in_context(
            persistent_scope, context, storage, owner);
      } else {
        return materialization_traits::materialize_source_in_context(
            scope, context, storage, owner);
      }
    }();
    if (retains_source) {
      return std::forward<Fn>(fn)(source);
    }
    return std::forward<Fn>(fn)(std::move(source));
  } else {
    auto source = [&] {
      if (materialization_traits::retains_source(storage)) {
        return materialization_traits::materialize_source(
            persistent_scope, context, storage, owner);
      } else {
        return materialization_traits::materialize_source(scope, context,
                                                          storage, owner);
      }
    }();
    return std::forward<Fn>(fn)(std::move(source));
  }
}

} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
