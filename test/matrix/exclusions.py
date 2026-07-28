#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT

"""Select matrix exclusions for the active compiler."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class Compiler:
    id: str = ""
    version: tuple[int, ...] = ()
    architecture: str = ""

    @classmethod
    def parse(
        cls,
        compiler_id: str,
        version: str,
        architecture: str,
    ) -> Compiler:
        if not version:
            return cls(id=compiler_id, architecture=architecture)
        try:
            parsed_version = tuple(int(part) for part in version.split("."))
        except ValueError as error:
            raise ValueError(f"invalid compiler version: {version}") from error
        if not parsed_version or any(part < 0 for part in parsed_version):
            raise ValueError(f"invalid compiler version: {version}")
        return cls(
            id=compiler_id,
            version=parsed_version,
            architecture=architecture,
        )

    @property
    def description(self) -> str:
        if not self.id:
            return "not specified"
        description = self.id
        if self.version:
            description += " " + _format_version(self.version)
        if self.architecture:
            description += f" ({self.architecture})"
        return description


@dataclass(frozen=True, slots=True)
class MatrixExclusion:
    name: str
    reason: str
    cases: frozenset[tuple[str, str]]
    compiler_id: str
    minimum_version: tuple[int, ...] | None = None
    maximum_version: tuple[int, ...] | None = None
    architectures: frozenset[str] = frozenset()

    @property
    def condition(self) -> str:
        conditions = [self.compiler_id]
        if self.minimum_version is not None:
            conditions.append(f">= {_format_version(self.minimum_version)}")
        if self.maximum_version is not None:
            conditions.append(f"< {_format_version(self.maximum_version)}")
        condition = " ".join(conditions)
        if self.architectures:
            condition += " on " + ", ".join(sorted(self.architectures))
        return condition

    def applies_to(self, compiler: Compiler) -> bool:
        has_required_version = (
            self.minimum_version is None and self.maximum_version is None
        ) or bool(compiler.version)
        matches_architecture = (
            not self.architectures
            or compiler.architecture.casefold()
            in {
                architecture.casefold()
                for architecture in self.architectures
            }
        )
        return (
            compiler.id.casefold() == self.compiler_id.casefold()
            and has_required_version
            and (
                self.minimum_version is None
                or compiler.version >= self.minimum_version
            )
            and (
                self.maximum_version is None
                or compiler.version < self.maximum_version
            )
            and matches_architecture
        )


def _format_version(version: tuple[int, ...]) -> str:
    return ".".join(str(part) for part in version)


def _composition_cases(row: str) -> frozenset[tuple[str, str]]:
    return frozenset((operation, row) for operation in ("invoke", "resolve"))


MATRIX_EXCLUSIONS = (
    MatrixExclusion(
        name="msvc-before-19.50-array-unique-pointer-copy-only",
        reason=(
            "MSVC crashes in CloseTypeServerPDB while compiling the array of "
            "unique pointers to copy-only values"
        ),
        cases=_composition_cases(
            "runtime_container_unique_value_array_unique_pointer_copy_only"
        ),
        compiler_id="MSVC",
        maximum_version=(19, 50),
    ),
    MatrixExclusion(
        name="msvc-19.50-array-shared-pointer-copy-only",
        reason=(
            "MSVC crashes while compiling the array of shared pointers to "
            "copy-only values"
        ),
        cases=_composition_cases(
            "runtime_container_unique_value_array_shared_pointer_copy_only"
        ),
        compiler_id="MSVC",
        minimum_version=(19, 50),
    ),
    MatrixExclusion(
        name="msvc-19.50-arm64-array-unique-pointer-copy-only",
        reason=(
            "MSVC additionally crashes on ARM64 while compiling the array of "
            "unique pointers to copy-only values"
        ),
        cases=_composition_cases(
            "runtime_container_unique_value_array_unique_pointer_copy_only"
        ),
        compiler_id="MSVC",
        minimum_version=(19, 50),
        architectures=frozenset({"ARM64"}),
    ),
)


_EXCLUSIONS_BY_NAME = {
    exclusion.name: exclusion for exclusion in MATRIX_EXCLUSIONS
}


def compiler_exclusions(compiler: Compiler) -> frozenset[str]:
    return frozenset(
        exclusion.name
        for exclusion in MATRIX_EXCLUSIONS
        if exclusion.applies_to(compiler)
    )


def exclusion_cases(names: frozenset[str]) -> frozenset[tuple[str, str]]:
    unknown = names - _EXCLUSIONS_BY_NAME.keys()
    if unknown:
        raise ValueError(f"unknown matrix exclusions: {sorted(unknown)}")
    return frozenset(
        case
        for name in names
        for case in _EXCLUSIONS_BY_NAME[name].cases
    )


__all__ = (
    "Compiler",
    "MATRIX_EXCLUSIONS",
    "compiler_exclusions",
    "exclusion_cases",
)
