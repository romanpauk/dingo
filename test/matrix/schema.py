#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class Feature:
    name: str
    requires: frozenset[str]
    modes: frozenset[str]
    checks: tuple[str, ...] = ()
    container_requires: frozenset[str] = frozenset()
    system_headers: tuple[str, ...] = ()
    support_headers: tuple[str, ...] = ()
    implemented: bool = True


@dataclass(frozen=True, slots=True)
class FeatureCaseSpec:
    name: str
    feature: str
    requires: frozenset[str] = frozenset()
    supported_exposed_types: frozenset[str] = frozenset()
    supported_resolved_types: frozenset[str] = frozenset()
    checks: tuple[str, ...] = ()
    system_headers: tuple[str, ...] = ()
    support_headers: tuple[str, ...] = ()
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
    support_headers: tuple[str, ...] = ()
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
    mixed: str = "static"
    static: bool = True


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
    support_headers: tuple[str, ...] = ()
    implemented: bool = True


@dataclass(frozen=True, slots=True)
class ResolvedType:
    name: str
    supported_exposed_types: frozenset[str]
    provides: frozenset[str]
    requires: frozenset[str] = frozenset()
    supported_modes: frozenset[str] = frozenset({"runtime", "static", "mixed"})
    checks: tuple[tuple[str, tuple[str, ...]], ...] = ()
    system_headers: tuple[str, ...] = ()
    support_headers: tuple[str, ...] = ()
    implemented: bool = True


@dataclass(frozen=True, slots=True)
class ContainerSpec:
    name: str
    modes: frozenset[str]
    container_type: str
    provides: frozenset[str] = frozenset()
    system_headers: tuple[str, ...] = ()
    support_headers: tuple[str, ...] = ()
    implemented: bool = True
