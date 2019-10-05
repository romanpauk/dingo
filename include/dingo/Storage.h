#pragma once

namespace dingo
{
    struct Shared {};
    struct SharedCyclical {};

    struct Unique {};

    template < typename Storage, typename Type, typename U > class Conversions;
    template < typename StorageTag, typename Type, typename Conversions = Conversions< StorageTag, Type, RuntimeType > > class Storage;
}