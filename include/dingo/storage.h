#pragma once

namespace dingo
{
    template < typename Storage, typename Type, typename U > class conversions;
    template < typename StorageTag, typename Type, typename Conversions = conversions< StorageTag, Type, runtime_type > > class storage;
}