#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT

"""Shared validation and rendering mechanics for matrix test families."""

from __future__ import annotations

from collections import Counter, defaultdict
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

from jinja2 import Template

from schema import GeneratedExecutable


@dataclass(frozen=True, slots=True)
class SourceShard:
    executable: str
    name: str
    source_context: Mapping[str, object]
    runner_context: Mapping[str, object]


def assert_unique_axis_members(
    family: str,
    axis: str,
    identities: tuple[str, ...],
) -> None:
    duplicates = sorted(
        identity
        for identity, count in Counter(identities).items()
        if count > 1
    )
    if duplicates:
        raise ValueError(
            f"{family} axis {axis} has duplicate identities: {duplicates}"
        )


def assert_axis_members_used(
    family: str,
    axis: str,
    declared: set[str],
    used: set[str],
) -> None:
    missing = sorted(declared - used)
    if missing:
        raise RuntimeError(
            f"{family} axis {axis} did not generate rows for: {missing}"
        )


def render_family_executables(
    out_dir: Path,
    source_template: Template,
    runner_template: Template,
    shards: tuple[SourceShard, ...],
    claimed_sources: set[Path] | None = None,
) -> tuple[GeneratedExecutable, ...]:
    if not shards:
        raise RuntimeError("matrix family did not generate any source shards")

    assert_unique_axis_members(
        "matrix family", "source shard", tuple(shard.name for shard in shards)
    )
    shard_paths = tuple(
        (
            shard,
            out_dir / f"{shard.name}.cpp",
            out_dir / f"matrix_runner_{shard.name}.cpp",
        )
        for shard in shards
    )
    if claimed_sources is not None:
        collisions = sorted(
            path
            for _, source, runner in shard_paths
            for path in (source, runner)
            if path in claimed_sources
        )
        if collisions:
            raise RuntimeError(
                "matrix families generated duplicate sources: "
                + ", ".join(str(path) for path in collisions)
            )
        claimed_sources.update(
            path
            for _, source, runner in shard_paths
            for path in (source, runner)
        )

    sources_by_executable: dict[str, list[Path]] = defaultdict(list)
    for shard, source, runner in shard_paths:
        write_text_if_changed(
            source, source_template.render(**shard.source_context)
        )
        write_text_if_changed(
            runner, runner_template.render(**shard.runner_context)
        )
        sources_by_executable[shard.executable].extend((source, runner))

    return tuple(
        GeneratedExecutable(name=name, sources=tuple(sources))
        for name, sources in sorted(sources_by_executable.items())
    )


def write_text_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.write_text(content, encoding="utf-8")
