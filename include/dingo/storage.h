#pragma once

#include <dingo/config.h>
#include <dingo/decay.h>
#include <dingo/factory/constructor.h>

namespace dingo {
namespace detail {
template <typename StorageTag, typename Type, typename U> struct conversions;
template <typename StorageTag, typename Type, typename Factory, typename Container, typename Conversions> class storage;

template <typename StorageTag, typename Type, typename Factory> class storage_instance;

template <typename Type> class class_instance_wrapper;

template <typename StorageTag, typename Type, typename TypeInterface>
struct storage_interface_requirements : std::bool_constant<true> {};

template <typename Storage, typename Type, typename TypeInterface>
static constexpr bool storage_interface_requirements_v =
    storage_interface_requirements<Storage, Type, TypeInterface>::value;
} // namespace detail
} // namespace dingo