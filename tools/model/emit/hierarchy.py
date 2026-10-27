#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""The shared test hierarchy.

Every generated test belongs to a phase, a container front-end, and the
storage path it exercises:

    <phase>/<container>/<scope>/<storage>/<category>/<traits>/<binding>/<request>

The phase is detection, registration, or resolution. The container is compile,
runtime, or static. A phase that does not use a level leaves it empty. The case
path is the identity used by manifests and reports; the file groups cases up to
the category; the suite name is the same prefix as an identifier.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class CasePath:
    phase: str
    container: str
    scope: str = ""
    storage: str = ""
    category: str = ""
    traits: str = ""
    binding: str = ""
    request: str = ""

    @property
    def prefix_parts(self) -> tuple[str, ...]:
        return tuple(
            part
            for part in (
                self.phase,
                self.container,
                self.scope,
                self.storage,
                self.category,
            )
            if part
        )

    @property
    def case_parts(self) -> tuple[str, ...]:
        return tuple(
            part
            for part in (
                *self.prefix_parts,
                self.traits,
                self.binding,
                self.request,
            )
            if part
        )

    @property
    def path(self) -> str:
        return "/".join(self.case_parts)

    @property
    def file(self) -> str:
        return "/".join(self.prefix_parts) + ".cpp"

    @property
    def suite(self) -> str:
        return "_".join(
            part.replace("/", "_").replace("-", "_")
            for part in self.prefix_parts
        )
