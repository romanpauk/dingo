//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/static/container.h>
#include <dingo/static/container_traits.h>

#include <type_traits>

namespace dingo {

namespace detail {
// The implementation of static_container<StaticSource, ParentContainer>. The
// second argument is either container traits or a parent container, whose
// traits the static container then shares.
template <typename StaticSource, typename ParentContainer,
          bool IsTraits = is_container_traits_v<ParentContainer>>
struct static_container_base {
  using type = static_container_impl<static_bindings_source_t<StaticSource>,
                                     ParentContainer,
                                     container_traits_of_t<ParentContainer>>;
};

template <typename StaticSource, typename ContainerTraits>
struct static_container_base<StaticSource, ContainerTraits, true> {
  using type = static_container_impl<static_bindings_source_t<StaticSource>,
                                     void, ContainerTraits>;
};

} // namespace detail

template <typename StaticSource, typename ParentContainer>
class static_container
    : public detail::static_container_base<StaticSource,
                                           ParentContainer>::type {
  using base_type =
      typename detail::static_container_base<StaticSource,
                                             ParentContainer>::type;

public:
  using base_type::base_type;
};

} // namespace dingo
