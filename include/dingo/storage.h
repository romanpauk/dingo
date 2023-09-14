#pragma once

#include <dingo/config.h>
#include <dingo/decay.h>
#include <dingo/factory/constructor.h>

namespace dingo {
template <typename Storage, typename Type, typename U> struct conversions;
template <typename StorageTag, typename Type, typename Factory = constructor<decay_t<Type>>, typename Container = void,
          typename Conversions = conversions<StorageTag, Type, runtime_type>>
class storage;

template <typename StorageTag, typename Type, typename Factory> class storage_instance;

template <typename Type> class class_instance_wrapper;

template <typename StorageTag, typename Type, typename TypeInterface>
struct storage_interface_requirements : std::bool_constant<true> {};

template <typename Storage, typename Type, typename TypeInterface>
static constexpr bool storage_interface_requirements_v =
    storage_interface_requirements<Storage, Type, TypeInterface>::value;

template <typename T> struct storage_traits;
template <typename StorageTag, typename Type, typename Factory, typename Container, typename Conversions>
struct storage_traits<storage<StorageTag, Type, Factory, Container, Conversions>> {
    template <typename FactoryT> using rebind_factory_t = storage<StorageTag, Type, FactoryT, Container, Conversions>;
};

} // namespace dingo