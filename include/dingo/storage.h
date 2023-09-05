#pragma once

#include <dingo/config.h>

namespace dingo {
template <typename Storage, typename Type, typename U> struct conversions;
template <typename StorageTag, typename Type, typename Container = void,
          typename Conversions = conversions<StorageTag, Type, runtime_type>>
class storage;
template <typename Type, typename StorageTag> class storage_instance;
template <typename Type> class class_instance_wrapper;

template <typename StorageTag, typename Type, typename TypeInterface>
struct storage_interface_requirements : std::bool_constant<true> {};
template <typename Storage, typename Type, typename TypeInterface>
static constexpr bool storage_interface_requirements_v =
    storage_interface_requirements<Storage, Type, TypeInterface>::value;

} // namespace dingo