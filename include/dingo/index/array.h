#pragma once

#include <dingo/config.h>

#include <dingo/index.h>

#include <array>

namespace dingo {
namespace index_type {
template <size_t N> struct array {};
} // namespace index_type

template <typename Key, typename Value, typename Allocator, size_t N>
struct index_collection<Key, Value, Allocator, index_type::array<N>> {
    static_assert(std::is_integral_v<Key> && std::is_unsigned_v<Key>);

    index_collection(Allocator&) {}

    bool emplace(Key key, Value value) {
        if (key >= array_.size())
            throw type_index_out_of_range_exception();
        if (!array_[key]) {
            array_[key] = value;
            return true;
        }
        return false;
    }

    Value* find(Key key) {
        if (key < array_.size() && array_[key])
            return &array_[key];
        return nullptr;
    }

  private:
    std::array<Value, N> array_{};
};

} // namespace dingo
