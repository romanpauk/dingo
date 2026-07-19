#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from dataclasses import dataclass
from itertools import product
from pathlib import Path

from jinja2 import Template

from axes.constructor_detection import (
    CONSTRUCTOR_ARGUMENT_CATEGORIES,
    CONSTRUCTOR_ARGUMENT_STORAGES,
    CONSTRUCTOR_SHAPES,
    CONSTRUCTOR_DETECTION_BACKENDS,
    CONSTRUCTOR_DETECTION_MODES,
)
from family import (
    SourceShard,
    assert_axis_members_used,
    assert_unique_axis_members,
    render_family_executables,
)
from schema import (
    ConstructorArgumentCategory,
    ConstructorArgumentStorage,
    ConstructorDetectionLimitation,
    ConstructorShape,
    ConstructorDetectionBackend,
    ConstructorDetectionMode,
    GeneratedExecutable,
)


@dataclass(frozen=True, slots=True)
class ConstructorDetectionRow:
    backend: ConstructorDetectionBackend
    mode: ConstructorDetectionMode
    constructor_shape: ConstructorShape

    @property
    def name(self) -> str:
        return "_".join(
            (self.backend.name, self.mode.name, self.constructor_shape.name)
        )

    @property
    def expected_arguments(self) -> str:
        if self.mode.name == "signature":
            return self.constructor_shape.signature_arguments
        if (
            self.constructor_shape.expected_kind
            == "dingo::detail::constructor_kind::concrete"
            and self.constructor_shape.expected_arity == 0
        ):
            return "dingo::type_list<>"
        return "void"

    @property
    def supported(self) -> bool:
        return self.limitation is None

    @property
    def limitation(self) -> ConstructorDetectionLimitation | None:
        return next(
            (
                limitation
                for limitation in self.constructor_shape.constructor_detection_limitations
                if limitation.mode == self.mode.name
                and (
                    limitation.backend is None
                    or limitation.backend == self.backend.name
                )
            ),
            None,
        )

    @property
    def unsupported_reason(self) -> str | None:
        limitation = self.limitation
        return None if limitation is None else limitation.reason


@dataclass(frozen=True, slots=True)
class ConstructorArgumentConversionRow:
    storage: ConstructorArgumentStorage
    category: ConstructorArgumentCategory

    @property
    def name(self) -> str:
        return f"{self.storage.name}_arg_{self.category.name}"


def generate_constructor_detection_rows(
    backends: tuple[ConstructorDetectionBackend, ...] = (
        CONSTRUCTOR_DETECTION_BACKENDS
    ),
    modes: tuple[ConstructorDetectionMode, ...] = CONSTRUCTOR_DETECTION_MODES,
    constructor_shapes: tuple[ConstructorShape, ...] = (
        CONSTRUCTOR_SHAPES
    ),
) -> tuple[ConstructorDetectionRow, ...]:
    assert_unique_axis_members(
        "constructor detection",
        "backend",
        tuple(member.name for member in backends),
    )
    assert_unique_axis_members(
        "constructor detection",
        "mode",
        tuple(member.name for member in modes),
    )
    assert_unique_axis_members(
        "constructor detection",
        "constructor_shape",
        tuple(member.name for member in constructor_shapes),
    )
    if not backends or not modes or not constructor_shapes:
        raise ValueError("constructor detection axes must not be empty")

    known_backends = {
        backend.name for backend in CONSTRUCTOR_DETECTION_BACKENDS
    }
    known_modes = {mode.name for mode in CONSTRUCTOR_DETECTION_MODES}
    for constructor_shape in constructor_shapes:
        limitations = constructor_shape.constructor_detection_limitations
        unknown_backends = sorted(
            {
                limitation.backend
                for limitation in limitations
                if limitation.backend is not None
                and limitation.backend not in known_backends
            }
        )
        unknown_modes = sorted(
            {
                limitation.mode
                for limitation in limitations
                if limitation.mode not in known_modes
            }
        )
        if unknown_backends:
            raise ValueError(
                f"constructor shape {constructor_shape.name} references "
                f"unknown limitation backends: {unknown_backends}"
            )
        if unknown_modes:
            raise ValueError(
                f"constructor shape {constructor_shape.name} references "
                f"unknown limitation modes: {unknown_modes}"
            )
        if any(not limitation.reason for limitation in limitations):
            raise ValueError(
                f"constructor shape {constructor_shape.name} has a limitation "
                "without a reason"
            )
        for backend in known_backends:
            for mode in known_modes:
                matching = tuple(
                    limitation
                    for limitation in limitations
                    if limitation.mode == mode
                    and limitation.backend in {None, backend}
                )
                if len(matching) > 1:
                    raise ValueError(
                        f"constructor shape {constructor_shape.name} has "
                        f"overlapping limitations for {backend}/{mode}"
                    )

    rows = tuple(
        ConstructorDetectionRow(backend, mode, constructor_shape)
        for backend, mode, constructor_shape in product(
            backends,
            modes,
            constructor_shapes,
        )
    )
    expected = len(backends) * len(modes) * len(constructor_shapes)
    if len(rows) != expected:
        raise RuntimeError("constructor detection matrix is not a full product")
    return rows


def generate_constructor_argument_conversion_rows(
    storages: tuple[ConstructorArgumentStorage, ...] = (
        CONSTRUCTOR_ARGUMENT_STORAGES
    ),
    categories: tuple[ConstructorArgumentCategory, ...] = (
        CONSTRUCTOR_ARGUMENT_CATEGORIES
    ),
) -> tuple[ConstructorArgumentConversionRow, ...]:
    assert_unique_axis_members(
        "constructor argument conversion",
        "storage",
        tuple(member.name for member in storages),
    )
    assert_unique_axis_members(
        "constructor argument conversion",
        "category",
        tuple(member.name for member in categories),
    )

    known_storages = {storage.name for storage in storages}
    for category in categories:
        missing_storages = sorted(category.supported_storages - known_storages)
        if missing_storages:
            raise ValueError(
                f"constructor argument category {category.name} references "
                f"unknown storages: {missing_storages}"
            )

    rows = tuple(
        ConstructorArgumentConversionRow(storage, category)
        for storage, category in product(storages, categories)
        if storage.name in category.supported_storages
    )
    assert_axis_members_used(
        "constructor argument conversion",
        "storage",
        known_storages,
        {row.storage.name for row in rows},
    )
    assert_axis_members_used(
        "constructor argument conversion",
        "category",
        {category.name for category in categories},
        {row.category.name for row in rows},
    )
    return rows


def generate_constructor_detection_executables(
    out_dir: Path,
    source_template: Template,
    runner_template: Template,
    claimed_sources: set[Path] | None = None,
) -> tuple[GeneratedExecutable, ...]:
    rows = generate_constructor_detection_rows()
    shards: list[SourceShard] = []
    for backend in CONSTRUCTOR_DETECTION_BACKENDS:
        for mode in CONSTRUCTOR_DETECTION_MODES:
            shard_rows = tuple(
                row
                for row in rows
                if row.backend == backend and row.mode == mode
            )
            if len(shard_rows) != len(CONSTRUCTOR_SHAPES):
                raise RuntimeError(
                    f"constructor detection shard {backend.name}/{mode.name} "
                    "does not cover every constructor shape"
                )
            shard = f"constructor_detection_{backend.name}_{mode.name}"
            context = {
                "detection_rows": shard_rows,
                "conversion_rows": (),
                "guard": backend.guard,
            }
            shards.append(
                SourceShard(
                    executable="dingo_matrix_test_constructor_detection",
                    name=shard,
                    source_context=context,
                    runner_context=context,
                )
            )
    conversion_rows = generate_constructor_argument_conversion_rows()
    conversion_context = {
        "detection_rows": (),
        "conversion_rows": conversion_rows,
        "guard": None,
    }
    shards.append(
        SourceShard(
            executable="dingo_matrix_test_constructor_detection",
            name="constructor_argument_conversion",
            source_context=conversion_context,
            runner_context=conversion_context,
        )
    )
    return render_family_executables(
        out_dir,
        source_template,
        runner_template,
        tuple(shards),
        claimed_sources,
    )
