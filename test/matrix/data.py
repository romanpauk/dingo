#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Validated catalog of axes used by the matrix generator."""

from __future__ import annotations

from collections import Counter
from collections.abc import Hashable, Iterable
from dataclasses import dataclass

from axes import (
    CONTAINERS,
    EXPOSED_TYPES,
    FEATURES,
    FEATURE_CASES,
    REGISTRATION_MODES,
    RESOLVED_TYPES,
    SCOPES,
    STORED_TYPES,
)
from schema import (
    ContainerSpec,
    ExposedType,
    Feature,
    FeatureCaseSpec,
    RegistrationMode,
    ResolvedType,
    ScopeSpec,
    StoredType,
)


@dataclass(frozen=True, slots=True)
class AxisCatalog:
    registration_modes: tuple[RegistrationMode, ...]
    scopes: tuple[ScopeSpec, ...]
    containers: tuple[ContainerSpec, ...]
    stored_types: tuple[StoredType, ...]
    exposed_types: tuple[ExposedType, ...]
    resolved_types: tuple[ResolvedType, ...]
    features: tuple[Feature, ...]
    feature_cases: tuple[FeatureCaseSpec, ...]

    @property
    def policy_sources(self) -> frozenset[tuple[str, ...]]:
        sources = {
            ("feature", feature.name)
            for feature in self.features
            if feature.policy is not None
        }
        sources.update(
            ("feature_case", feature_case.feature, feature_case.name)
            for feature_case in self.feature_cases
            if feature_case.policy is not None
        )
        sources.update(
            ("resolved_type", resolved_type.name, feature)
            for resolved_type in self.resolved_types
            for feature, _ in resolved_type.policies
        )
        return frozenset(sources)


def _assert_unique(axis: str, values: Iterable[Hashable]) -> None:
    duplicates = sorted(value for value, count in Counter(values).items() if count > 1)
    if duplicates:
        raise ValueError(f"matrix axis {axis} has duplicate identities: {duplicates}")


def _assert_known(
    owner: str, relation: str, references: Iterable[str], known: set[str]
) -> None:
    missing = sorted(set(references) - known)
    if missing:
        raise ValueError(f"matrix {owner} references unknown {relation}: {missing}")


def validate_catalog(catalog: AxisCatalog) -> None:
    _assert_unique(
        "registration_mode", (mode.name for mode in catalog.registration_modes)
    )
    _assert_unique("scope", (scope.name for scope in catalog.scopes))
    _assert_unique("container", (container.name for container in catalog.containers))
    _assert_unique(
        "stored_type", (stored_type.id for stored_type in catalog.stored_types)
    )
    _assert_unique(
        "exposed_type", (exposed_type.name for exposed_type in catalog.exposed_types)
    )
    _assert_unique(
        "resolved_type",
        (resolved_type.name for resolved_type in catalog.resolved_types),
    )
    _assert_unique("feature", (feature.name for feature in catalog.features))
    _assert_unique(
        "feature_case",
        (
            (feature_case.feature, feature_case.name)
            for feature_case in catalog.feature_cases
        ),
    )

    modes = {mode.name for mode in catalog.registration_modes}
    scopes = {scope.name for scope in catalog.scopes}
    stored_kinds = {stored_type.kind for stored_type in catalog.stored_types}
    exposures = {exposed_type.name for exposed_type in catalog.exposed_types}
    resolutions = {resolved_type.name for resolved_type in catalog.resolved_types}
    features = {feature.name for feature in catalog.features}

    for container in catalog.containers:
        _assert_known(f"container {container.name}", "modes", container.modes, modes)
    for stored_type in catalog.stored_types:
        _assert_known(
            f"stored type {stored_type.id}",
            "modes",
            stored_type.supported_modes,
            modes,
        )
        _assert_known(
            f"stored type {stored_type.id}",
            "scopes",
            stored_type.supported_scopes,
            scopes,
        )
    for exposed_type in catalog.exposed_types:
        _assert_known(
            f"exposed type {exposed_type.name}",
            "stored kinds",
            exposed_type.supported_stored_kinds,
            stored_kinds,
        )
    for resolved_type in catalog.resolved_types:
        _assert_known(
            f"resolved type {resolved_type.name}",
            "exposed types",
            resolved_type.supported_exposed_types,
            exposures,
        )
        _assert_known(
            f"resolved type {resolved_type.name}",
            "modes",
            resolved_type.supported_modes,
            modes,
        )
        _assert_known(
            f"resolved type {resolved_type.name}",
            "policy features",
            (feature for feature, _ in resolved_type.policies),
            features,
        )
    for feature in catalog.features:
        _assert_known(f"feature {feature.name}", "modes", feature.modes, modes)
    for feature_case in catalog.feature_cases:
        _assert_known(
            f"feature case {feature_case.name}",
            "features",
            (feature_case.feature,),
            features,
        )
        _assert_known(
            f"feature case {feature_case.feature}/{feature_case.name}",
            "exposed types",
            feature_case.supported_exposed_types,
            exposures,
        )
        _assert_known(
            f"feature case {feature_case.feature}/{feature_case.name}",
            "resolved types",
            feature_case.supported_resolved_types,
            resolutions,
        )


CATALOG = AxisCatalog(
    registration_modes=REGISTRATION_MODES,
    scopes=SCOPES,
    containers=CONTAINERS,
    stored_types=STORED_TYPES,
    exposed_types=EXPOSED_TYPES,
    resolved_types=RESOLVED_TYPES,
    features=FEATURES,
    feature_cases=FEATURE_CASES,
)
validate_catalog(CATALOG)


__all__ = (
    "CATALOG",
    "CONTAINERS",
    "EXPOSED_TYPES",
    "FEATURES",
    "FEATURE_CASES",
    "REGISTRATION_MODES",
    "RESOLVED_TYPES",
    "SCOPES",
    "STORED_TYPES",
    "AxisCatalog",
    "validate_catalog",
)
