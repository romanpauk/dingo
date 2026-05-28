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
    ExposedType,
    Feature,
    RegistrationMode,
    RegistrationSpec,
    ResolvedType,
    ScopeSpec,
    StoredType,
)


CONTAINERS = (
    ContainerSpec(
        name="runtime_container",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<>",
        provides=frozenset({"nested_container"}),
    ),
    ContainerSpec(
        name="container_runtime",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<>",
        provides=frozenset({"nested_container"}),
    ),
    ContainerSpec(
        name="static_container",
        modes=frozenset({"static"}),
        container_type="dingo::static_container<static_bindings>",
    ),
    ContainerSpec(
        name="container_static",
        modes=frozenset({"static"}),
        container_type="dingo::container<static_bindings>",
    ),
    ContainerSpec(
        name="container_mixed",
        modes=frozenset({"mixed"}),
        container_type="dingo::container<static_bindings>",
    ),
    ContainerSpec(
        name="runtime_container_indexed",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<indexed_container_traits>",
        provides=frozenset({"indexed_container", "indexed_regression_container"}),
    ),
    ContainerSpec(
        name="container_indexed",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<indexed_container_traits>",
        provides=frozenset({"indexed_container", "indexed_regression_container"}),
    ),
    ContainerSpec(
        name="runtime_container_indexed_int",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<indexed_int_container_traits>",
        provides=frozenset({"indexed_regression_container"}),
    ),
    ContainerSpec(
        name="container_indexed_int",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<indexed_int_container_traits>",
        provides=frozenset({"indexed_regression_container"}),
    ),
    ContainerSpec(
        name="runtime_container_indexed_string",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<indexed_string_container_traits>",
        provides=frozenset({"indexed_regression_container"}),
    ),
    ContainerSpec(
        name="container_indexed_string",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<indexed_string_container_traits>",
        provides=frozenset({"indexed_regression_container"}),
    ),
    ContainerSpec(
        name="runtime_container_indexed_unordered",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<indexed_unordered_container_traits>",
        provides=frozenset({"indexed_container", "indexed_regression_container"}),
    ),
    ContainerSpec(
        name="container_indexed_unordered",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<indexed_unordered_container_traits>",
        provides=frozenset({"indexed_container", "indexed_regression_container"}),
    ),
    ContainerSpec(
        name="runtime_container_indexed_int_unordered",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<indexed_int_unordered_container_traits>",
        provides=frozenset({"indexed_regression_container"}),
    ),
    ContainerSpec(
        name="container_indexed_int_unordered",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<indexed_int_unordered_container_traits>",
        provides=frozenset({"indexed_regression_container"}),
    ),
    ContainerSpec(
        name="runtime_container_indexed_array",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<indexed_array_container_traits>",
        provides=frozenset({"indexed_container", "indexed_regression_container"}),
    ),
    ContainerSpec(
        name="container_indexed_array",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<indexed_array_container_traits>",
        provides=frozenset({"indexed_container", "indexed_regression_container"}),
    ),
    ContainerSpec(
        name="container_allocator",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<dingo::dynamic_container_traits, test_allocator<char>>",
        provides=frozenset({"custom_allocator_container"}),
    ),
    ContainerSpec(
        name="container_custom_rtti",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<custom_rtti_container_traits>",
        provides=frozenset({"custom_rtti_container"}),
    ),
)
