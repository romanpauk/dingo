#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from dataclasses import replace

from schema import (
    Feature,
)


FEATURES = (
    Feature(
        name="construct_invoke",
        requires=frozenset(
            {
                "concrete_binding",
                "constructable_dependency",
                "invokable_dependency",
                "stable_concrete_storage",
                "resolved_concrete",
            }
        ),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_concrete",
        requires=frozenset({"concrete_binding", "resolved_concrete"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_interface",
        requires=frozenset({"interface_binding", "resolved_interface"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_collection",
        requires=frozenset(
            {"collection_binding", "shared_storage", "resolved_collection"}
        ),
        modes=frozenset({"static", "mixed"}),
        system_headers=("algorithm",),
    ),
    Feature(
        name="resolve_keyed",
        requires=frozenset({"keyed_binding", "shared_storage", "resolved_keyed"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_unique_wrapper",
        requires=frozenset(
            {"interface_binding", "unique_storage", "resolved_unique_wrapper"}
        ),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_wrapper",
        requires=frozenset({"resolved_wrapper"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="custom_wrappers",
        requires=frozenset({"custom_wrapper_binding", "resolved_custom_wrapper"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="nested_wrappers",
        requires=frozenset({"nested_wrapper_binding", "resolved_nested_wrapper"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="lifetime_counts",
        requires=frozenset({"lifetime_countable"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_indexed",
        requires=frozenset(
            {"indexed_binding", "shared_storage", "resolved_indexed"}
        ),
        modes=frozenset({"runtime"}),
        container_requires=frozenset({"indexed_container"}),
    ),
    Feature(
        name="resolve_keyed_collection",
        requires=frozenset({"keyed_collection_binding", "resolved_keyed_collection"}),
        modes=frozenset({"static", "mixed"}),
        system_headers=("algorithm",),
    ),
    Feature(
        name="construct",
        requires=frozenset({"constructable_dependency"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="construct_collection",
        requires=frozenset({"collection_binding", "constructable_collection"}),
        modes=frozenset({"static", "mixed"}),
        system_headers=("algorithm", "map"),
    ),
    Feature(
        name="local_bindings",
        requires=frozenset({"local_bindings"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="nested_container",
        requires=frozenset(
            {
                "concrete_binding",
                "direct_value_resolution",
                "nested_container",
                "resolved_concrete",
            }
        ),
        modes=frozenset({"runtime"}),
        system_headers=("type_traits",),
    ),
    Feature(
        name="annotated",
        requires=frozenset({"annotated_binding", "resolved_annotated"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="variant",
        requires=frozenset({"variant_binding", "resolved_variant"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="array",
        requires=frozenset({"array_binding", "resolved_array"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="factory_override",
        requires=frozenset({"factory_override"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        headers=("matrix/scenarios/factory.h",),
    ),
    Feature(
        name="explicit_dependencies",
        requires=frozenset(
            {"explicit_dependencies", "resolved_explicit_dependencies"}
        ),
        modes=frozenset({"runtime", "static", "mixed"}),
        container_requires=frozenset({"explicit_dependencies_container"}),
    ),
    Feature(
        name="mixed_external_dependency",
        requires=frozenset(
            {"mixed_external_dependency", "resolved_mixed_external_dependency"}
        ),
        modes=frozenset({"mixed"}),
    ),
    Feature(
        name="mixed_singular_ambiguity",
        requires=frozenset(
            {"mixed_singular_conflict", "resolved_mixed_singular_conflict"}
        ),
        modes=frozenset({"mixed"}),
    ),
    Feature(
        name="custom_allocator",
        requires=frozenset(
            {
                "custom_allocator_container",
                "concrete_binding",
                "direct_value_resolution",
                "stable_concrete_storage",
            }
        ),
        modes=frozenset({"runtime"}),
    ),
    Feature(
        name="custom_rtti",
        requires=frozenset(
            {
                "custom_rtti_container",
                "concrete_binding",
                "direct_value_resolution",
                "stable_concrete_storage",
            }
        ),
        modes=frozenset({"runtime"}),
        system_headers=("type_traits",),
    ),
)


_FEATURE_POLICIES = {
    "construct_invoke": (
        "resolution::construct_invoke<dependent_type, value_type, 3>"
    ),
    "construct": "resolution::construct_member_value<dependent_type, 3>",
    "construct_collection": (
        "resolution::construct_collection_values<"
        "std::vector<std::shared_ptr<element_interface>>>"
    ),
    "explicit_dependencies": (
        "resolution::member_value<explicit_dependencies_type, 17>"
    ),
    "mixed_singular_ambiguity": "resolution::ambiguous<value_type>",
    "custom_allocator": (
        "resolution::custom_allocator<value_type, test_allocator_stats>"
    ),
    "custom_rtti": (
        "resolution::custom_rtti<value_type, "
        "dingo::rtti<dingo::static_provider>>"
    ),
}

POLICY_HEADERS = {
    "annotated": "matrix/policies/policy_aggregate.h",
    "array": "matrix/policies/policy_aggregate.h",
    "construct": "matrix/policies/resolution.h",
    "construct_collection": "matrix/policies/policy_collection.h",
    "construct_invoke": "matrix/policies/resolution.h",
    "explicit_dependencies": "matrix/policies/policy_core.h",
    "custom_allocator": "matrix/policies/policy_special.h",
    "custom_rtti": "matrix/policies/policy_special.h",
    "custom_wrappers": "matrix/policies/resolution.h",
    "lifetime_counts": "matrix/policies/policy_lifetime.h",
    "local_bindings": "matrix/policies/policy_special.h",
    "mixed_singular_ambiguity": "matrix/policies/policy_special.h",
    "nested_wrappers": "matrix/policies/policy_aggregate.h",
    "resolve_collection": "matrix/policies/policy_collection.h",
    "resolve_concrete": "matrix/policies/resolution.h",
    "resolve_indexed": "matrix/policies/policy_collection.h",
    "resolve_interface": "matrix/policies/resolution.h",
    "resolve_keyed": "matrix/policies/policy_collection.h",
    "resolve_keyed_collection": "matrix/policies/policy_collection.h",
    "resolve_unique_wrapper": "matrix/policies/resolution.h",
    "resolve_wrapper": "matrix/policies/resolution.h",
    "variant": "matrix/policies/policy_aggregate.h",
}

FEATURES = tuple(
    replace(
        feature,
        policy=_FEATURE_POLICIES.get(feature.name),
        policy_header=POLICY_HEADERS.get(feature.name, feature.policy_header),
    )
    for feature in FEATURES
)
