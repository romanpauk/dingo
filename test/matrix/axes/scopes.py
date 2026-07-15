#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from schema import ScopeSpec


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
