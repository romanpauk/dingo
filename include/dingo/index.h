#pragma once

#include <dingo/config.h>

#include <dingo/allocator.h>
#include <dingo/exceptions.h>

#include <tuple>
#include <type_traits>
#include <variant>

namespace dingo {
#if 1
template <typename Arg, typename... Args> struct index_tag;
template <typename Arg, typename Head, typename... Tail>
struct index_tag<Arg, std::tuple<Head, Tail...>> {
    using type =
        std::conditional_t<std::is_same_v<std::tuple_element_t<0, Head>, Arg>,
                           std::tuple_element_t<1, Head>,
                           typename index_tag<Arg, std::tuple<Tail...>>::type>;
};

template <typename Arg> struct index_tag<Arg, std::tuple<>> {
    using type = void;
};
#else
// TODO: measure compile time if using same tuple will be different
template <typename Arg, size_t N, typename Tuple,
          bool Enabled = std::is_same_v<
              Arg, std::tuple_element_t<0, std::tuple_element_t<N, Tuple>>>>
struct index_tag_impl {
    using type = std::tuple_element_t<1, std::tuple_element_t<N, Tuple>>;
};

template <typename Arg, size_t N, typename Tuple>
struct index_tag_impl<Arg, N, Tuple, false>
    : index_tag_impl<Arg, N + 1, Tuple> {};

// template <typename Arg, typename Tuple, bool Enabled> struct
// get_index_tag2_impl<Arg, std::tuple_size_v<Tuple>, Tuple, Enabled> {
//     static_assert("type not found");
// };

template <typename Arg, typename... Args> struct index_tag;

template <typename Arg, typename... Args>
struct index_tag<Arg, std::tuple<Args...>>
    : index_tag_impl<Arg, 0, std::tuple<Args...>> {};
#endif

template <typename Key, typename Value, typename Allocator, typename Tag>
struct index_collection;

// TODO: static_allocator does not work with this
template <typename Factory, typename Allocator, typename... Args> struct index {
    index(Allocator&) {}

    template <typename T> struct index_ptr : allocator_base<Allocator> {
        // TODO: this pattern is already in class_factory_instance_ptr,
        // try to merge those
        index_ptr(Allocator& allocator) : allocator_base<Allocator>(allocator) {
            auto alloc = allocator_traits::rebind<T>(this->get_allocator());
            try {
                index_ = allocator_traits::allocate(alloc, 1);
                allocator_traits::construct(alloc, index_,
                                            this->get_allocator());
            } catch (...) {
                allocator_traits::deallocate(alloc, index_, 1);
                throw;
            }
        }

        ~index_ptr() {
            if (index_) {
                auto alloc = allocator_traits::rebind<T>(this->get_allocator());
                allocator_traits::destroy(alloc, index_);
                allocator_traits::deallocate(alloc, index_, 1);
            }
        }

        index_ptr() = default;
        index_ptr(const index_ptr<T>&) = delete;
        index_ptr(index_ptr<T>&& other)
            : allocator_base<Allocator>(std::move(other)) {
            std::swap(index_, other.index_);
        }

        T& operator*() {
            assert(index_);
            return *index_;
        }

      private:
        T* index_ = nullptr;
    };

    template <typename Key> auto& get_index(Allocator& allocator) {
        using index_type = index_collection<
            Key, Factory, Allocator,
            typename index_tag<Key, std::tuple<Args...>>::type>;

        if (indexes_.index() == 0) {
            indexes_.template emplace<index_ptr<index_type>>(allocator);
        }

        return *std::get<index_ptr<index_type>>(indexes_);
    }

  private:
    std::variant<std::monostate,
                 index_ptr<index_collection<std::tuple_element_t<0, Args>,
                                            Factory, Allocator,
                                            std::tuple_element_t<1, Args>>>...>
        indexes_;
};

template <typename Factory, typename Allocator, typename... Args>
struct index<std::tuple<Args...>, Factory, Allocator>
    : index<Factory, Allocator, Args...> {
    index(Allocator& allocator)
        : index<Factory, Allocator, Args...>(allocator) {}
};

template <typename Factory, typename Allocator>
struct index<Factory, Allocator> {
    index(Allocator&){};
};

} // namespace dingo
