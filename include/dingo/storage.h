#pragma once

#include <dingo/config.h>

namespace dingo {
template <typename Storage, typename Type, typename U> struct conversions;
template <typename StorageTag, typename Type, typename Container = void,
          typename Conversions = conversions<StorageTag, Type, runtime_type>>
class storage;
template <typename Type, typename StorageTag> class storage_instance;
template <typename Type> class class_instance_wrapper;
} // namespace dingo