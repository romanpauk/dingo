//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/type/type_descriptor.h>
#include <dingo/type/type_list.h>

#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace dingo {

enum class registration_origin { static_binding, runtime_binding };

enum class registration_materialization {
  unknown,
  not_materialized,
  materialized
};

struct registration_id {
  const void *value = nullptr;
};

constexpr bool operator==(registration_id lhs, registration_id rhs) {
  return lhs.value == rhs.value;
}

constexpr bool operator!=(registration_id lhs, registration_id rhs) {
  return !(lhs == rhs);
}

class type_descriptor_list {
  using callback_type = void (*)(void *, type_descriptor);
  using visit_type = void (*)(void *, callback_type);

public:
  constexpr type_descriptor_list() = default;

  constexpr type_descriptor_list(std::size_t size, visit_type visit)
      : size_(size), visit_(visit) {}

  constexpr std::size_t size() const noexcept { return size_; }
  constexpr bool empty() const noexcept { return size_ == 0; }

  template <typename Visitor> void for_each(Visitor &&visitor) const {
    auto invoke = [](void *state, type_descriptor type) {
      (*static_cast<std::remove_reference_t<Visitor> *>(state))(type);
    };
    visit_(std::addressof(visitor), invoke);
  }

private:
  std::size_t size_ = 0;
  visit_type visit_ = [](void *, callback_type) {};
};

class introspection_key {
public:
  constexpr introspection_key() = default;

  template <typename T> static introspection_key from(const T &value) {
    return introspection_key(describe_type<T>(), std::addressof(value));
  }

  constexpr bool has_value() const noexcept { return value_ != nullptr; }
  constexpr type_descriptor type() const noexcept { return type_; }

  template <typename T> const T *get_if() const noexcept {
    return type_ == describe_type<T>() ? static_cast<const T *>(value_)
                                       : nullptr;
  }

private:
  constexpr introspection_key(type_descriptor type, const void *value)
      : type_(type), value_(value) {}

  type_descriptor type_{};
  const void *value_ = nullptr;
};

struct registration_view {
  registration_id id;
  std::size_t container_id = 0;
  registration_origin origin = registration_origin::runtime_binding;
  type_descriptor registered_type;
  type_descriptor storage_type;
  type_descriptor scope_type;
  type_descriptor factory_type;
  type_descriptor interface_type;
  type_descriptor key_type;
  type_descriptor_list interfaces;
  type_descriptor_list dependencies;
  bool dependencies_known = false;
  registration_materialization materialization =
      registration_materialization::unknown;
  introspection_key key;
};

struct dependency_view {
  registration_id registration;
  std::size_t container_id = 0;
  type_descriptor constructed_type;
  type_descriptor dependency_type;
};

namespace detail {

class dependency_observer_ref {
public:
  dependency_observer_ref() = default;
  dependency_observer_ref(const dependency_observer_ref &) = default;
  dependency_observer_ref &operator=(const dependency_observer_ref &) = default;

  template <typename Observer,
            std::enable_if_t<!std::is_same_v<std::decay_t<Observer>,
                                             dependency_observer_ref>,
                             int> = 0>
  explicit dependency_observer_ref(Observer &observer) noexcept
      : state_(std::addressof(observer)),
        notify_([](void *state, const dependency_view &dependency) noexcept {
          (*static_cast<Observer *>(state))(dependency);
        }) {
    static_assert(
        std::is_nothrow_invocable_v<Observer &, const dependency_view &>,
        "dingo dependency observers must be noexcept");
  }

  void notify(const dependency_view &dependency) const noexcept {
    if (notify_ != nullptr) {
      notify_(state_, dependency);
    }
  }

private:
  void *state_ = nullptr;
  void (*notify_)(void *, const dependency_view &) noexcept = nullptr;
};

struct dependency_observer_node {
  const void *target = nullptr;
  dependency_observer_ref observer;
  dependency_observer_node *previous = nullptr;
};

inline thread_local dependency_observer_node *active_dependency_observer =
    nullptr;

inline dependency_observer_ref
find_dependency_observer(const void *target) noexcept {
  for (auto *node = active_dependency_observer; node != nullptr;
       node = node->previous) {
    if (node->target == target) {
      return node->observer;
    }
  }
  return {};
}

} // namespace detail

class dependency_observer_subscription {
public:
  dependency_observer_subscription() = default;

  dependency_observer_subscription(
      const void *target, detail::dependency_observer_ref observer) noexcept
      : node_{target, observer, detail::active_dependency_observer} {
    detail::active_dependency_observer = std::addressof(node_);
  }

  dependency_observer_subscription(const dependency_observer_subscription &) =
      delete;
  dependency_observer_subscription &
  operator=(const dependency_observer_subscription &) = delete;

  dependency_observer_subscription(dependency_observer_subscription &&) =
      delete;
  dependency_observer_subscription &
  operator=(dependency_observer_subscription &&) = delete;

  ~dependency_observer_subscription() {
    if (node_.target != nullptr) {
      assert(detail::active_dependency_observer == std::addressof(node_));
      detail::active_dependency_observer = node_.previous;
    }
  }

private:
  detail::dependency_observer_node node_;
};

namespace detail {

template <typename Traits, typename = void>
struct dependency_observation : std::false_type {};

template <typename Traits>
struct dependency_observation<
    Traits, std::void_t<decltype(Traits::dependency_observation_enabled)>>
    : std::bool_constant<Traits::dependency_observation_enabled> {};

template <typename Traits>
inline constexpr bool dependency_observation_v =
    dependency_observation<Traits>::value;

template <typename List> struct type_descriptor_list_factory;

template <typename... Types>
struct type_descriptor_list_factory<type_list<Types...>> {
  static type_descriptor_list make() {
    return {sizeof...(Types), [](void *state, auto callback) {
              (callback(state, describe_type<Types>()), ...);
            }};
  }
};

template <> struct type_descriptor_list_factory<void> {
  static type_descriptor_list make() { return {}; }
};

template <typename List> type_descriptor_list describe_types() {
  return type_descriptor_list_factory<List>::make();
}

template <typename Storage, typename = void>
struct introspection_materialization {
  static registration_materialization get(const Storage &) noexcept {
    return registration_materialization::unknown;
  }
};

template <typename Storage>
struct introspection_materialization<
    Storage,
    std::void_t<decltype(std::declval<const Storage &>().is_resolved())>> {
  static registration_materialization get(const Storage &storage) noexcept {
    return storage.is_resolved()
               ? registration_materialization::materialized
               : registration_materialization::not_materialized;
  }
};

template <typename BindingModel, typename Storage>
registration_view introspect_static_registration(const Storage &storage,
                                                 std::size_t container_id) {
  using dependencies = typename BindingModel::dependencies_type::type;
  return {{std::addressof(storage)},
          container_id,
          registration_origin::static_binding,
          describe_type<typename BindingModel::registered_storage_type>(),
          describe_type<typename BindingModel::stored_type>(),
          describe_type<typename BindingModel::storage_tag>(),
          describe_type<typename BindingModel::factory_type>(),
          {},
          describe_type<typename BindingModel::registration_key_type>(),
          describe_types<typename BindingModel::interface_types>(),
          describe_types<dependencies>(),
          !std::is_same_v<dependencies, void>,
          introspection_materialization<Storage>::get(storage),
          {}};
}

struct dependency_construction_node {
  dependency_observer_ref observer;
  registration_id registration;
  std::size_t container_id = 0;
  type_descriptor constructed_type;
  dependency_construction_node *previous = nullptr;
};

inline thread_local dependency_construction_node
    *active_dependency_construction = nullptr;

class dependency_construction_scope {
public:
  dependency_construction_scope(dependency_observer_ref observer,
                                registration_id registration,
                                std::size_t container_id,
                                type_descriptor constructed_type) noexcept
      : node_{observer, registration, container_id, constructed_type,
              active_dependency_construction} {
    active_dependency_construction = std::addressof(node_);
  }

  dependency_construction_scope(const dependency_construction_scope &) = delete;
  dependency_construction_scope &
  operator=(const dependency_construction_scope &) = delete;

  ~dependency_construction_scope() {
    assert(active_dependency_construction == std::addressof(node_));
    active_dependency_construction = node_.previous;
  }

private:
  dependency_construction_node node_;
};

inline void observe_dependency(type_descriptor dependency_type) noexcept {
  if (active_dependency_construction != nullptr) {
    const auto &construction = *active_dependency_construction;
    construction.observer.notify(
        {construction.registration, construction.container_id,
         construction.constructed_type, dependency_type});
  }
}

template <typename Constructed, typename Fn>
decltype(auto) observe_dependencies(dependency_observer_ref observer,
                                    registration_id registration,
                                    std::size_t container_id, Fn &&fn) {
  dependency_construction_scope scope(observer, registration, container_id,
                                      describe_type<Constructed>());
  return std::forward<Fn>(fn)();
}

} // namespace detail
} // namespace dingo
