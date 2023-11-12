#pragma once

#include <dingo/config.h>

#include <dingo/index.h>

#include <unordered_map>

namespace dingo {
namespace index_type {
struct unordered_map {};
} // namespace index_type

template <typename Key, typename Value, typename Allocator>
struct index_collection<Key, Value, Allocator, index_type::unordered_map> {
    index_collection(Allocator& allocator) : map_(allocator) {}

    bool emplace(Key&& key, Value& value) {
        auto pb =
            map_.emplace(std::forward<Key>(key), std::forward<Value>(value));
        return pb.second;
    }

    Value find(Key& key) const {
        auto it = map_.find(key);
        return it != map_.end() ? it->second : nullptr;
    }

  private:
    using allocator_type = typename std::allocator_traits<
        Allocator>::template rebind_alloc<std::pair<const Key, Value>>;

    std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>,
                       allocator_type>
        map_;
};
} // namespace dingo
