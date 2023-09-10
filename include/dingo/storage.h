#pragma once

#include <dingo/class_factory.h>
#include <dingo/config.h>
#include <dingo/decay.h>

namespace dingo {
template <typename Storage, typename Type, typename U> struct conversions;
template <typename StorageTag, typename Type, typename Factory = void, typename Container = void,
          typename Conversions = conversions<StorageTag, Type, runtime_type>>
class storage;

template <typename StorageTag, typename Type> class storage_instance;
template <typename Type> class class_instance_wrapper;

template <typename StorageTag, typename Type, typename TypeInterface>
struct storage_interface_requirements : std::bool_constant<true> {};
template <typename Storage, typename Type, typename TypeInterface>
static constexpr bool storage_interface_requirements_v =
    storage_interface_requirements<Storage, Type, TypeInterface>::value;

template <typename Factory, typename Type, typename Context>
using storage_factory_t =
    std::conditional_t<std::is_same_v<void, Factory>,
                       class_factory<decay_t<Type>, constructor_argument<decay_t<Type>, Context>>, Factory>;
} // namespace dingo