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


REGISTRATION_MODES = (
    RegistrationMode("runtime"),
    RegistrationMode("static"),
    RegistrationMode("mixed"),
)


SCOPES = (
    ScopeSpec(
        name="shared",
        type_name="dingo::scope<dingo::shared>",
        provides=frozenset({"shared_storage", "stable_concrete_storage"}),
    ),
    ScopeSpec(
        name="unique",
        type_name="dingo::scope<dingo::unique>",
        provides=frozenset({"unique_storage"}),
    ),
    ScopeSpec(
        name="external",
        type_name="dingo::scope<dingo::external>",
        provides=frozenset({"external_storage", "stable_concrete_storage"}),
    ),
    ScopeSpec(
        name="shared_cyclical",
        type_name="dingo::scope<dingo::shared_cyclical>",
        provides=frozenset({"shared_storage", "stable_concrete_storage"}),
    ),
)


FILTER_RULES = (
    "container_mode",
    "feature_mode",
    "stored_type_supports_mode",
    "scope_supports_stored_type",
    "exposure_supports_stored_type",
    "resolved_type_supports_exposure",
    "resolved_type_supports_mode",
    "feature_requires_tags",
    "resolved_type_requires_tags",
    "container_requires",
)
