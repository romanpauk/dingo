#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

namespace dingo {

// Taken from
// https://howardhinnant.github.io/allocator_boilerplate.html
template <typename T> class test_allocator_stats {
  protected:
    static size_t allocated;

  public:
    static size_t get_allocated() { return allocated; }
};

template <typename T> size_t test_allocator_stats<T>::allocated;

template <typename T> class test_allocator : public test_allocator_stats<void> {
  public:
    using value_type = T;

    test_allocator() noexcept {}
    template <typename U> test_allocator(const test_allocator<U>&) noexcept {}

    value_type* allocate(std::size_t n) {
        this->allocated += n * sizeof(value_type);
        return static_cast<value_type*>(::operator new(n * sizeof(value_type)));
    }

    void deallocate(value_type* p, std::size_t n) noexcept {
        this->allocated -= n * sizeof(value_type);
        ::operator delete(p);
    }
};

template <typename T, typename U>
bool operator==(const test_allocator<T>&, const test_allocator<U>&) noexcept {
    return true;
}
template <typename T, typename U>
bool operator!=(const test_allocator<T>& x,
                const test_allocator<U>& y) noexcept {
    return !(x == y);
}

using container_types = ::testing::Types<
    dingo::container<dingo::static_container_traits<>, test_allocator<char>>,
    dingo::container<dingo::dynamic_container_traits, test_allocator<char>>>;

template <typename T> struct allocator_test : public testing::Test {};
TYPED_TEST_SUITE(allocator_test, container_types);

// TODO: not exactly allocator-related
TYPED_TEST(allocator_test, construct) {
    using container_type = TypeParam;

    // TODO:
    // For types with many constructors, it is a must to override as
    // detection will not do what intended (as for vectors, it will select list
    // initialization of arity 32).
    //
    // With override, the container could provide allocator argument if there is
    // none found in container and there is an allocator as one of the
    // parameters (or it is one of allocation-protocol cases) AND users selected
    // to propagate allocator to resolved types (generally there could be short
    // lived container and long-lived objects, so two allocators are needed, one
    // for container itself, other for injection)
    //
    // Lets finish container allocator first.

    container_type container;

    // This more or less just tests if things compile now.
    if (0) {
        container.template construct<
            std::vector<int>,
            detail::constructor_detection<std::vector<int>,
                                          detail::direct_initialization>>();
        container.template construct<
            std::vector<int>,
            detail::constructor_detection<std::vector<int>,
                                          detail::list_initialization>>();
        container.template construct<
            std::vector<int>,
            constructor<std::vector<int>(std::allocator<int>)>>();
    }
}

TYPED_TEST(allocator_test, user_allocator_unique) {
    using container_type = TypeParam;

    test_allocator<char> alloc;

    // For dynamic containers, std::map allocates in an empty state
    ASSERT_EQ(alloc.get_allocated(), 0);
    {
        container_type container(alloc);
        container.template register_type<scope<unique>, storage<int>>();
        ASSERT_GT(alloc.get_allocated(), 0);
        container.template resolve<int>();
    }
    ASSERT_GE(alloc.get_allocated(), 0);
}

TYPED_TEST(allocator_test, user_allocator_shared) {
    using container_type = TypeParam;

    test_allocator<char> alloc;

    // For dynamic containers, std::map allocates in an empty state
    ASSERT_EQ(alloc.get_allocated(), 0);
    {
        container_type container(alloc);
        container.template register_type<scope<shared>,
                                         storage<std::unique_ptr<int>>>();
        ASSERT_GT(alloc.get_allocated(), 0);
        container.template resolve<int&>();
    }
    ASSERT_GE(alloc.get_allocated(), 0);
}

} // namespace dingo
