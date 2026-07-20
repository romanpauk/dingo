#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT

"""Independent axes for two-node shared-cyclical graphs."""

from __future__ import annotations

from itertools import product

from schema import (
    SharedCyclicalContainer,
    SharedCyclicalDependencyEdge,
    SharedCyclicalDependencyEdgeShape,
    SharedCyclicalStorageRepresentation,
    SharedCyclicalStorageShape,
)


SHARED_CYCLICAL_CONTAINERS = (
    SharedCyclicalContainer(
        name="runtime_container",
        container_types=(("runtime", "dingo::runtime_container<>"),),
        headers=("matrix/containers/runtime.h",),
    ),
    SharedCyclicalContainer(
        name="static_container",
        container_types=(
            ("static", "dingo::static_container<static_bindings>"),
        ),
        headers=("matrix/containers/static.h",),
    ),
    SharedCyclicalContainer(
        name="container",
        container_types=(
            ("runtime", "dingo::container<>"),
            ("static", "dingo::container<static_bindings>"),
            ("mixed", "dingo::container<static_bindings>"),
        ),
        headers=("matrix/containers/container.h",),
    ),
)


SHARED_CYCLICAL_DEPENDENCY_EDGES = (
    SharedCyclicalDependencyEdge(
        name="lvalue_reference",
        type_name="cycle_lvalue_reference",
    ),
    SharedCyclicalDependencyEdge(
        name="pointer",
        type_name="cycle_pointer",
    ),
    SharedCyclicalDependencyEdge(
        name="shared_pointer_value",
        type_name="cycle_shared_pointer_value",
    ),
    SharedCyclicalDependencyEdge(
        name="shared_pointer_reference",
        type_name="cycle_shared_pointer_reference",
    ),
    SharedCyclicalDependencyEdge(
        name="shared_pointer_pointer",
        type_name="cycle_shared_pointer_pointer",
    ),
)


_PLAIN_DEPENDENCY_EDGES = frozenset({"lvalue_reference", "pointer"})
_ALL_DEPENDENCY_EDGES = frozenset(
    edge.name for edge in SHARED_CYCLICAL_DEPENDENCY_EDGES
)


SHARED_CYCLICAL_STORAGE_REPRESENTATIONS = (
    SharedCyclicalStorageRepresentation(
        name="value",
        type_expression="{type}",
        supported_dependency_edges=_PLAIN_DEPENDENCY_EDGES,
    ),
    SharedCyclicalStorageRepresentation(
        name="shared_pointer",
        type_expression="std::shared_ptr<{type}>",
        supported_dependency_edges=_ALL_DEPENDENCY_EDGES,
        shared_ownership=True,
    ),
)


SHARED_CYCLICAL_STORAGE_SHAPES = tuple(
    SharedCyclicalStorageShape(
        name=f"{a.name}_{b.name}",
        a=a,
        b=b,
    )
    for a, b in product(
        SHARED_CYCLICAL_STORAGE_REPRESENTATIONS,
        repeat=2,
    )
)


SHARED_CYCLICAL_DEPENDENCY_EDGE_SHAPES = tuple(
    SharedCyclicalDependencyEdgeShape(
        name=f"{a_to_b.name}_{b_to_a.name}",
        a_to_b=a_to_b,
        b_to_a=b_to_a,
    )
    for a_to_b, b_to_a in product(
        SHARED_CYCLICAL_DEPENDENCY_EDGES,
        repeat=2,
    )
)
