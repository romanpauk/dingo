#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from schema import (
    ContainerSpec,
)


def container(**kwargs: object) -> ContainerSpec:
    return ContainerSpec(**kwargs)


CONTAINERS = (
    container(
        name="runtime_container",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<>",
        provides=frozenset(
            {
                "cross_parent_container_matrix",
                "explicit_dependencies_container",
                "nested_container",
                "runtime_regression_container",
                "runtime_lookup_routing_container",
            }
        ),
        headers=("matrix/containers/runtime.h",),
    ),
    container(
        name="container_runtime",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<>",
        provides=frozenset(
            {
                "nested_container",
                "runtime_lookup_routing_container",
                "runtime_regression_container",
            }
        ),
        headers=("matrix/containers/container.h",),
    ),
    container(
        name="static_container",
        modes=frozenset({"static"}),
        container_type="dingo::static_container<static_bindings>",
        provides=frozenset(
            {
                "explicit_dependencies_container",
                "static_indexed_regression_container",
                "static_parent_static_child",
            }
        ),
        headers=("matrix/containers/static.h",),
    ),
    container(
        name="container_static",
        modes=frozenset({"static"}),
        container_type="dingo::container<static_bindings>",
        provides=frozenset({"static_parent_container_child"}),
        headers=("matrix/containers/container.h",),
    ),
    container(
        name="container_mixed",
        modes=frozenset({"mixed"}),
        container_type="dingo::container<static_bindings>",
        provides=frozenset({"explicit_dependencies_container"}),
        headers=("matrix/containers/container.h",),
    ),
    container(
        name="runtime_container_indexed",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<indexed_container_traits>",
        provides=frozenset({"indexed_container", "indexed_regression_container"}),
        headers=("matrix/containers/indexed.h",),
    ),
    container(
        name="container_indexed",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<indexed_container_traits>",
        provides=frozenset({"indexed_container", "indexed_regression_container"}),
        headers=("matrix/containers/indexed.h",),
    ),
    container(
        name="runtime_container_indexed_int",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<indexed_int_container_traits>",
        provides=frozenset({"indexed_regression_container"}),
        headers=("matrix/containers/indexed.h",),
    ),
    container(
        name="container_indexed_int",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<indexed_int_container_traits>",
        provides=frozenset({"indexed_regression_container"}),
        headers=("matrix/containers/indexed.h",),
    ),
    container(
        name="runtime_container_indexed_string",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<indexed_string_container_traits>",
        provides=frozenset({"indexed_regression_container"}),
        headers=("matrix/containers/indexed.h",),
    ),
    container(
        name="container_indexed_string",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<indexed_string_container_traits>",
        provides=frozenset({"indexed_regression_container"}),
        headers=("matrix/containers/indexed.h",),
    ),
    container(
        name="runtime_container_indexed_dsl",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<indexed_dsl_container_traits>",
        provides=frozenset({"indexed_container", "indexed_regression_container"}),
        headers=("matrix/containers/indexed.h",),
    ),
    container(
        name="container_indexed_dsl",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<indexed_dsl_container_traits>",
        provides=frozenset({"indexed_container", "indexed_regression_container"}),
        headers=("matrix/containers/indexed.h",),
    ),
    container(
        name="container_allocator",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<dingo::dynamic_container_traits, test_allocator<char>>",
        provides=frozenset({"custom_allocator_container"}),
        headers=("matrix/containers/allocator.h",),
    ),
    container(
        name="container_custom_rtti",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<custom_rtti_container_traits>",
        provides=frozenset({"custom_rtti_container"}),
        headers=("matrix/containers/custom_rtti.h",),
    ),
)
