#pragma once

#include <typeindex>

namespace dingo {
    class dynamic_rtti {
    public:
        class type_index {
        public:
            type_index(std::type_index value): value_(value) {}
        
            bool operator < (const type_index& other) const { return value_ < other.value_; }
            bool operator == (const type_index& other) const { return value_ == other.value_; }

            struct hasher {
                size_t operator()(const type_index& index) const { return std::hash< decltype(value_) >()(index.value_); }
            };
        private:
            std::type_index value_;
        };

        template< typename T > static type_index get_type_index() {
            return std::type_index(typeid(T));
        }
    };

    template < typename Tag > struct static_type_index_base {
    public:
        static size_t generate_id() { return ++counter_; }
    private:
        static size_t counter_;
    };

    template < typename Tag > size_t static_type_index_base<Tag>::counter_;

    template < typename T > struct static_type_index_cache {
        static size_t value;
    };
    template < typename T > size_t static_type_index_cache< T >::value = static_type_index_base<void>::generate_id();

    class static_rtti {
    public:
        // A faster variant of std::type_index that can be obtained without calling typeid().
        class type_index {
        public:
            type_index(size_t value): value_(value) {}
        
            bool operator < (const type_index& other) const { return value_ < other.value_; }
            bool operator == (const type_index& other) const { return value_ == other.value_; }

            struct hasher {
                size_t operator()(const type_index& index) const { return std::hash< decltype(value_) >()(index.value_); }
            };
        private:
            size_t value_;
        };

        template< typename T > static type_index get_type_index() {
            return static_type_index_cache<T>::value;
        }
    };
}