#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Typed model for generated matrix test configuration."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from pathlib import Path


class MixedRegistrationPlacement(Enum):
    STATIC = "static"
    RUNTIME = "runtime"


@dataclass(frozen=True, slots=True)
class Feature:
    name: str
    requires: frozenset[str]
    modes: frozenset[str]
    policy: str | None = None
    policy_header: str = "matrix/policies/policy_core.h"
    container_requires: frozenset[str] = frozenset()
    system_headers: tuple[str, ...] = ()
    headers: tuple[str, ...] = ()
    implemented: bool = True


@dataclass(frozen=True, slots=True)
class FeatureCaseSpec:
    name: str
    feature: str
    supported_exposed_types: frozenset[str] = frozenset()
    registrations: tuple[RegistrationSpec, ...] = ()
    policy: str | None = None
    system_headers: tuple[str, ...] = ()
    headers: tuple[str, ...] = ()
    implemented: bool = True


@dataclass(frozen=True, slots=True)
class RegistrationMode:
    name: str


@dataclass(frozen=True, slots=True)
class ScopeSpec:
    name: str
    type_name: str
    provides: frozenset[str]
    implemented: bool = True


@dataclass(frozen=True, slots=True)
class StoredType:
    id: str
    name: str
    kind: str
    storage: str
    supported_scopes: frozenset[str]
    provides: frozenset[str]
    supported_modes: frozenset[str] = frozenset({"runtime", "static", "mixed"})
    runtime_setup: tuple[str, ...] = ()
    runtime_argument: str | None = None
    factory: str | None = None
    system_headers: tuple[str, ...] = ()
    headers: tuple[str, ...] = ()
    implemented: bool = True


@dataclass(frozen=True, slots=True)
class RegistrationSpec:
    scope: str | None = None
    storage: str | None = None
    interfaces: str | None = None
    local_bindings: str | None = None
    factory: str | None = None
    dependencies: str | None = None
    key: str | None = None
    indexed_key: str | None = None
    runtime_setup: tuple[str, ...] = ()
    runtime_argument: str | None = None
    mixed_placement: MixedRegistrationPlacement = MixedRegistrationPlacement.STATIC
    include_in_static: bool = True


@dataclass(frozen=True, slots=True)
class ExposedType:
    name: str
    kind: str
    supported_stored_kinds: frozenset[str]
    provides: frozenset[str]
    registrations: tuple[RegistrationSpec, ...]
    runtime_prefix: tuple[str, ...] = ()
    mixed_runtime_prefix: tuple[str, ...] = ()
    mixed: bool = True
    system_headers: tuple[str, ...] = ()
    headers: tuple[str, ...] = ()
    implemented: bool = True


@dataclass(frozen=True, slots=True)
class ResolvedType:
    name: str
    supported_exposed_types: frozenset[str]
    provides: frozenset[str]
    dependency_forms: frozenset[str] = frozenset()
    requires: frozenset[str] = frozenset()
    supported_modes: frozenset[str] = frozenset({"runtime", "static", "mixed"})
    policies: tuple[tuple[str, str], ...] = ()
    system_headers: tuple[str, ...] = ()
    headers: tuple[str, ...] = ()
    implemented: bool = True


@dataclass(frozen=True, slots=True)
class ContainerSpec:
    name: str
    modes: frozenset[str]
    container_type: str
    provides: frozenset[str] = frozenset()
    system_headers: tuple[str, ...] = ()
    headers: tuple[str, ...] = ()
    implemented: bool = True


@dataclass(frozen=True, slots=True)
class ConstructorDetectionBackend:
    name: str
    type_name: str
    guard: str | None = None


@dataclass(frozen=True, slots=True)
class ConstructorDetectionMode:
    name: str
    type_name: str


@dataclass(frozen=True, slots=True)
class ConstructorDetectionLimitation:
    backend: str | None
    mode: str
    reason: str


@dataclass(frozen=True, slots=True)
class DependencyShapeConstructorDetectionLimitation:
    carriers: frozenset[str]
    decorations: frozenset[str]
    limitation: ConstructorDetectionLimitation


@dataclass(frozen=True, slots=True)
class DependencyShape:
    name: str
    type_expression: str
    required_families: frozenset[str]
    constructor_detection_limitations: tuple[
        DependencyShapeConstructorDetectionLimitation, ...
    ] = ()


@dataclass(frozen=True, slots=True)
class DependencyCarrier:
    name: str
    type_expression: str


@dataclass(frozen=True, slots=True)
class DependencyDecoration:
    name: str
    request_expression: str
    supported_shape_carriers: frozenset[tuple[str, str]]
    required_families: frozenset[str]


@dataclass(frozen=True, slots=True)
class DependencyForm:
    name: str
    shape: str
    carrier: str
    decoration: str
    required_families: frozenset[str]


@dataclass(frozen=True, slots=True)
class DependencyProvisioning:
    name: str
    scope: str
    stored_type: str
    exposed_type: str
    resolution_cases: tuple[tuple[str, str, str], ...]
    supported_modes: frozenset[str]

    @property
    def resolved_type(self) -> str:
        return self.resolution_cases[0][1]

    @property
    def supported_forms(self) -> frozenset[str]:
        return frozenset(form for form, _, _ in self.resolution_cases)


@dataclass(frozen=True, slots=True)
class ConstructorShape:
    name: str
    target_type: str
    expected_kind: str
    expected_arity: int | str
    signature_arguments: str
    dependency_forms: frozenset[str] = frozenset()
    detector_only: bool = False
    constructor_detection_limitations: tuple[
        ConstructorDetectionLimitation, ...
    ] = ()


@dataclass(frozen=True, slots=True)
class ConstructorArgumentStorage:
    name: str
    type_name: str


@dataclass(frozen=True, slots=True)
class ConstructorArgumentCategory:
    name: str
    type_name: str
    dependency_form: str
    supported_storages: frozenset[str]
    guard: str | None = None


@dataclass(frozen=True, slots=True)
class ScenarioContainer:
    name: str
    type_name: str | None
    headers: tuple[str, ...] = ()


@dataclass(frozen=True, slots=True)
class ScenarioSpec:
    suite: str
    name: str
    test_name: str
    function: str
    supported_containers: frozenset[str]
    system_headers: tuple[str, ...] = ()
    headers: tuple[str, ...] = ()


@dataclass(frozen=True, slots=True)
class InvokeCallable:
    name: str
    policy: str
    dependency_forms: frozenset[str]
    supported_provisionings: frozenset[str]
    supported_containers: frozenset[str] | None = None
    system_headers: tuple[str, ...] = ()
    headers: tuple[str, ...] = ()


@dataclass(frozen=True, slots=True)
class GeneratedExecutable:
    name: str
    sources: tuple[Path, ...]
