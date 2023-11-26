#pragma once

#include <dingo/config.h>

#include <dingo/index.h>
#include <dingo/static_allocator.h>

#include <map>

namespace dingo {
namespace index_type {
struct map {};
} // namespace index_type

template <typename Key, typename Value, typename Allocator>
struct index_collection<Key, Value, Allocator, index_type::map> {
    static_assert(!is_static_allocator_v<Allocator>);

    index_collection(Allocator& allocator) : map_(allocator) {}

    bool emplace(Key&& key, Value value) {
        auto pb = map_.emplace(std::move(key), value);
        return pb.second;
    }

    Value* find(const Key& key) {
        auto it = map_.find(key);
        return it != map_.end() ? &it->second : nullptr;
    }

  private:
    using allocator_type = typename std::allocator_traits<
        Allocator>::template rebind_alloc<std::pair<const Key, Value>>;

    std::map<Key, Value, std::less<Key>, allocator_type> map_;
};
} // namespace dingo
