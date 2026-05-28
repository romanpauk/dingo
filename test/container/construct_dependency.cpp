//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/core/container_common.h>

#include <gtest/gtest.h>

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace dingo {

namespace issue_43 {
struct A {};
struct B {};

template <typename... Deps> class dependencies {
  public:
    explicit dependencies(Deps... deps) : deps_(std::forward<Deps>(deps)...) {}

    template <typename D> auto dependency() const { return std::get<D>(deps_); }

  private:
    std::tuple<Deps...> deps_;
};

template <typename... Deps> class auto_dependencies {
  public:
    explicit auto_dependencies(Deps... deps)
        : deps_(std::forward<Deps>(deps)...) {}

    template <typename D> auto dependency() const { return std::get<D>(deps_); }

  private:
    std::tuple<Deps...> deps_;
};

struct ambiguous_auto_dependencies {
    ambiguous_auto_dependencies(std::shared_ptr<A>) {}
    ambiguous_auto_dependencies(std::shared_ptr<B>) {}
};

template <typename T> struct holder {
    T value;
};

template <typename... Deps> struct aggregate_dependencies : holder<Deps>... {
    template <typename D> auto dependency() const {
        static_assert((std::is_same_v<D, Deps> || ...));
        return static_cast<const holder<D>&>(*this).value;
    }
};
} // namespace issue_43

template <typename... Deps>
struct is_auto_constructible<issue_43::auto_dependencies<Deps...>>
    : std::true_type {};
template <>
struct is_auto_constructible<issue_43::ambiguous_auto_dependencies>
    : std::true_type {};

TEST(construct_dependency_test, auto_constructible_trait_defaults_and_opt_in) {
    static_assert(
        std::is_aggregate_v<issue_43::aggregate_dependencies<
            std::shared_ptr<issue_43::A>, std::shared_ptr<issue_43::B>>>);
    static_assert(is_auto_constructible<issue_43::aggregate_dependencies<
                      std::shared_ptr<issue_43::A>,
                      std::shared_ptr<issue_43::B>>>::value);
    static_assert(!is_auto_constructible<
                  issue_43::dependencies<std::shared_ptr<issue_43::A>,
                                         std::shared_ptr<issue_43::B>>>::value);
    static_assert(
        is_auto_constructible<
            issue_43::auto_dependencies<std::shared_ptr<issue_43::A>,
                                        std::shared_ptr<issue_43::B>>>::value);
}

} // namespace dingo
