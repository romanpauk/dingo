#pragma once

#include <typeindex>

namespace dingo {
class dynamic_rtti {
  public:
    class type_index {
      public:
        type_index(std::type_index value, bool smart_ptr) : value_(value), is_smart_ptr_(smart_ptr) {}

        bool operator<(const type_index& other) const { return value_ < other.value_; }
        bool operator==(const type_index& other) const { return value_ == other.value_; }

        struct hasher {
            size_t operator()(const type_index& index) const { return std::hash<std::type_index>()(index.value_); }
        };

        bool is_smart_ptr() const { return is_smart_ptr_; }

      private:
        const std::type_index value_;
        bool is_smart_ptr_;
    };

    template <typename T> static type_index get_type_index() {
        return {std::type_index(typeid(T)), type_traits<std::remove_pointer_t<std::decay_t<T>>>::is_smart_ptr};
    }
};

template <typename Tag> struct static_type_index_base {
  public:
    static size_t generate_id() { return counter_ += 2; }

  private:
    static size_t counter_;
};

template <typename Tag> size_t static_type_index_base<Tag>::counter_ = 0;

template <typename T> struct static_type_index_cache {
    static const size_t value;
};
template <typename T> const size_t static_type_index_cache<T>::value = static_type_index_base<void>::generate_id();

class static_rtti {
  public:
    // A faster variant of std::type_index that can be obtained without calling
    // typeid().
    class type_index {
      public:
        type_index(size_t value) : value_(value) {}

        bool operator<(const type_index& other) const { return value_ < other.value_; }
        bool operator==(const type_index& other) const { return value_ == other.value_; }

        struct hasher {
            size_t operator()(const type_index& index) const { return std::hash<size_t>()(index.value_); }
        };

        bool is_smart_ptr() const { return value_ & 1; }

      private:
        const size_t value_;
    };

    template <typename T> static type_index get_type_index() {
        return static_type_index_cache<T>::value | type_traits<std::remove_pointer_t<std::decay_t<T>>>::is_smart_ptr;
    }
};
} // namespace dingo