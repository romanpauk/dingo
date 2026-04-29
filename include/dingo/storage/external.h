//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/factory/constructor.h>
#include <dingo/storage/resettable.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_conversion_traits.h>

namespace dingo {
struct external {};

template <typename Type> struct storage_materialization_traits<external, Type> {
    template <typename Leaf, typename Context, typename Storage>
    static auto make_guard(Context&, const Storage&) {
        return detail::no_materialization_scope();
    }

    template <typename Storage>
    static bool preserves_closure(const Storage&) {
        return false;
    }

    template <typename Context, typename Storage, typename Container>
    static auto materialize_source(Context& context, Storage& storage,
                                   Container& container) {
        return detail::make_resolved_source(storage.resolve(context, container));
    }
};

template <typename Type, typename U>
struct storage_traits<
    external, Type, U,
    std::enable_if_t<!type_traits<Type>::enabled && !std::is_reference_v<Type> &&
                     !std::is_array_v<Type>>> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = true;

    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename Type, typename U>
struct storage_traits<external, Type*, U> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = true;

    using value_types = type_list<U>;
    using lvalue_reference_types = type_list<U&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*>;
    using conversion_types = type_list<>;
};

template <typename T, typename U>
struct storage_traits<external, T[], U> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = true;

    using value_types = type_list<>;
    using lvalue_reference_types = type_list<>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<typename detail::wrapper_rebind_leaf<T, U>::type*>;
    using conversion_types = type_list<>;
};

template <typename T, size_t N, typename U>
struct storage_traits<external, T[N], U> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = true;

    using rebound_row_type = typename detail::wrapper_rebind_leaf<T, U>::type;
    using rebound_exact_type =
        typename detail::wrapper_rebind_leaf<T[N], U>::type;

    using value_types = type_list<>;
    using lvalue_reference_types = type_list<exact_lookup<rebound_exact_type>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types =
        type_list<rebound_row_type*, exact_lookup<rebound_exact_type>*>;
    using conversion_types = type_list<>;
};

template <typename Array, typename Deleter, typename U>
struct storage_traits<
    external, std::unique_ptr<Array, Deleter>, U,
    std::enable_if_t<std::is_array_v<Array> && (std::extent_v<Array, 0> == 0)>> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = true;

    using handle_type =
        detail::wrapper_rebind_leaf_t<std::unique_ptr<Array, Deleter>, U>;
    using pointer_types =
        typename detail::smart_array_pointer_types<
            handle_type, std::remove_extent_t<Array>, U>::type;

    using value_types = type_list<>;
    using lvalue_reference_types = type_list<handle_type&>;
    using rvalue_reference_types = type_list<>;
    using conversion_types = type_list<>;
};

template <typename T, typename Deleter, typename U>
struct storage_traits<external, std::unique_ptr<T, Deleter>, U,
                      std::enable_if_t<!std::is_array_v<T>>> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = true;

    using handle_type =
        detail::wrapper_rebind_leaf_t<std::unique_ptr<T, Deleter>, U>;
    using types = detail::wrapper_storage_types<handle_type>;

    using value_types = type_list<>;
    using lvalue_reference_types = typename types::lvalue_reference_types;
    using rvalue_reference_types = type_list<>;
    using pointer_types = typename types::pointer_types;
    using conversion_types = type_list<>;
};

template <typename Array, typename U>
struct storage_traits<
    external, std::shared_ptr<Array>, U,
    std::enable_if_t<std::is_array_v<Array> && (std::extent_v<Array, 0> == 0)>> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = true;

    using handle_type = detail::wrapper_rebind_leaf_t<std::shared_ptr<Array>, U>;
    using pointer_types =
        typename detail::smart_array_pointer_types<
            handle_type, std::remove_extent_t<Array>, U>::type;

    using value_types = type_list<handle_type>;
    using lvalue_reference_types = type_list<handle_type&>;
    using rvalue_reference_types = type_list<>;
    using conversion_types = type_list<handle_type>;
};

template <typename T, typename U>
struct storage_traits<external, std::shared_ptr<T>, U,
                      std::enable_if_t<!std::is_array_v<T>>> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = true;

    using handle_type = detail::wrapper_rebind_leaf_t<std::shared_ptr<T>, U>;
    using types = detail::wrapper_storage_types<handle_type>;

    using value_types = typename types::copyable_value_types;
    using lvalue_reference_types = typename types::lvalue_reference_types;
    using rvalue_reference_types = type_list<>;
    using pointer_types = typename types::pointer_types;
    using conversion_types = typename types::copyable_value_types;
};

template <typename T, typename U>
struct storage_traits<external, std::optional<T>, U> {
    static constexpr bool enabled = true;
    static constexpr bool is_stable = true;

    using value_types = type_list<>;
    using lvalue_reference_types =
        type_list<U&, exact_lookup<std::optional<T>>&>;
    using rvalue_reference_types = type_list<>;
    using pointer_types = type_list<U*, exact_lookup<std::optional<T>>*>;
    using conversion_types = type_list<>;
};

namespace detail {
template <typename Type, typename U> struct conversions<external, Type, U>
    : type_storage_traits<external, Type, U> {};

template <typename Type, typename U> struct conversions<external, Type&, U>
    : public type_storage_traits<external, Type&, U> {};

template <typename Type, typename U> struct conversions<external, Type*, U>
    : public type_storage_traits<external, Type*, U> {};

template <typename Type, typename StoredType, typename = void>
class external_storage_instance_impl {
  public:
    template <typename T>
    external_storage_instance_impl(T&& instance)
        : instance_(std::forward<T>(instance)) {}

    Type& get() { return instance_; }

  private:
    Type instance_;
};

template <typename Type, typename StoredType>
class external_storage_instance_impl<
    Type, StoredType, std::enable_if_t<type_traits<Type>::enabled>> {
  public:
    template <typename T>
    external_storage_instance_impl(T&& instance)
        : instance_(type_conversion_traits<StoredType, Type>::convert(
              std::forward<T>(instance))) {}

    StoredType& get() { return instance_; }

  private:
    StoredType instance_;
};

template <typename Type, typename StoredType>
class storage_instance<external, Type, StoredType, void>
    : public external_storage_instance_impl<Type, StoredType> {
  public:
    template <typename T>
    storage_instance(T&& instance)
        : external_storage_instance_impl<Type, StoredType>(
              std::forward<T>(instance)) {}
};

template <typename Type, size_t N, typename StoredType>
class storage_instance<external, Type[N], StoredType, void> {
  public:
    storage_instance(Type (&instance)[N]) : instance_(instance) {}

    Type* get() { return instance_; }

  private:
    Type (&instance_)[N];
};

template <typename Type, typename StoredType>
class storage_instance<external, Type&, StoredType, void> {
  public:
    storage_instance(Type& instance) : instance_(instance) {}

    Type& get() { return instance_; }

  private:
    Type& instance_;
};

template <typename Type, typename StoredType>
class storage_instance<external, Type*, StoredType, void> {
  public:
    storage_instance(Type* instance) : instance_(instance) {}

    Type* get() { return instance_; }

  private:
    Type* instance_;
};

template <typename Type, typename StoredType, typename Factory,
          typename Conversions>
class storage<external, Type, StoredType, Factory, Conversions>
    : public resettable {
    storage_instance<external, Type, StoredType, void> instance_;

  public:
    using conversions = Conversions;
    using type = Type;
    using stored_type = StoredType;
    using tag_type = external;

    template <typename T>
    storage(T&& instance) : instance_(std::forward<T>(instance)) {}

    template <typename Context, typename Container>
    decltype(auto) resolve(Context&, Container&) {
        return instance_.get();
    }
    constexpr bool is_resolved() const { return true; }

    void reset() override {}
};
} // namespace detail
} // namespace dingo
