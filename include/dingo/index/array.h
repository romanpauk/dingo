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
    index_collection(Allocator&) {}

    bool emplace(Key&& key, Value& value) {
        check_key_in_range(key);
        if (!array_[key]) {
            array_[key] = value;
            return true;
        }
        return false;
    }

    Value find(Key& key) const {
        check_key_in_range(key);
        auto& value = array_[key];
        return value;
    }

  private:
    void check_key_in_range(const Key& key) const {
        if (key >= array_.size())
            throw type_index_out_of_range_exception();
    }

    std::array<Value, N> array_{};
};

} // namespace dingo
