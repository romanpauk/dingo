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

RUNNER_CASE_LIMIT = 512


@dataclass(frozen=True, slots=True)
class SourceShard:
    executable: str
    name: str
    source_context: Mapping[str, object]
    runner_context: Mapping[str, object]


@dataclass(frozen=True, slots=True)
class RunnerShard:
    executable: str
    name: str
    context: Mapping[str, object]


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
    runner_shards: tuple[RunnerShard, ...] | None = None,
) -> tuple[GeneratedExecutable, ...]:
    if not shards:
        raise RuntimeError("matrix family did not generate any source shards")

    assert_unique_axis_members(
        "matrix family", "source shard", tuple(shard.name for shard in shards)
    )
    if runner_shards is None:
        runner_shards = tuple(
            RunnerShard(
                executable=shard.executable,
                name=shard.name,
                context=shard.runner_context,
            )
            for shard in shards
        )
    if not runner_shards:
        raise RuntimeError("matrix family did not generate any runner shards")
    assert_unique_axis_members(
        "matrix family",
        "runner shard",
        tuple(shard.name for shard in runner_shards),
    )
    source_executables = {shard.executable for shard in shards}
    runner_executables = {shard.executable for shard in runner_shards}
    if source_executables != runner_executables:
        raise RuntimeError(
            "matrix family source and runner executables differ: "
            f"sources={sorted(source_executables)}, "
            f"runners={sorted(runner_executables)}"
        )

    source_paths = tuple(
        (shard, out_dir / f"{shard.name}.cpp") for shard in shards
    )
    runner_paths = tuple(
        (shard, out_dir / f"matrix_runner_{shard.name}.cpp")
        for shard in runner_shards
    )
    generated_paths = tuple(
        path for _, path in (*source_paths, *runner_paths)
    )
    if claimed_sources is not None:
        collisions = sorted(
            path for path in generated_paths if path in claimed_sources
        )
        if collisions:
            raise RuntimeError(
                "matrix families generated duplicate sources: "
                + ", ".join(str(path) for path in collisions)
            )
        claimed_sources.update(generated_paths)

    sources_by_executable: dict[str, list[Path]] = defaultdict(list)
    for shard, source in source_paths:
        write_text_if_changed(
            source, source_template.render(**shard.source_context)
        )
        sources_by_executable[shard.executable].append(source)
    for shard, runner in runner_paths:
        write_text_if_changed(
            runner, runner_template.render(**shard.context)
        )
        sources_by_executable[shard.executable].append(runner)

    return tuple(
        GeneratedExecutable(name=name, sources=tuple(sources))
        for name, sources in sorted(sources_by_executable.items())
    )


def render_case_family_executables(
    out_dir: Path,
    source_template: Template,
    runner_template: Template,
    shards: tuple[SourceShard, ...],
    claimed_sources: set[Path] | None = None,
    runner_case_limit: int = RUNNER_CASE_LIMIT,
) -> tuple[GeneratedExecutable, ...]:
    if runner_case_limit <= 0:
        raise ValueError("runner case limit must be positive")

    shards_by_executable: dict[str, list[SourceShard]] = defaultdict(list)
    for shard in shards:
        shards_by_executable[shard.executable].append(shard)

    runner_shards: list[RunnerShard] = []
    for executable, executable_shards in sorted(shards_by_executable.items()):
        ordered_shards = sorted(executable_shards, key=lambda shard: shard.name)
        cases: list[object] = []
        for shard in ordered_shards:
            if set(shard.runner_context) != {"cases"}:
                raise ValueError(
                    f"case runner shard {shard.name} must only define cases"
                )
            shard_cases = shard.runner_context["cases"]
            if not isinstance(shard_cases, tuple):
                raise TypeError(
                    f"case runner shard {shard.name} cases must be a tuple"
                )
            cases.extend(shard_cases)
        if not cases:
            raise RuntimeError(
                f"matrix executable {executable} did not generate runner cases"
            )

        case_batches = tuple(
            tuple(cases[offset : offset + runner_case_limit])
            for offset in range(0, len(cases), runner_case_limit)
        )
        for index, case_batch in enumerate(case_batches):
            if len(ordered_shards) == 1 and len(case_batches) == 1:
                name = ordered_shards[0].name
            else:
                name = f"{ordered_shards[0].name}_batch_{index + 1}"
            runner_shards.append(
                RunnerShard(
                    executable=executable,
                    name=name,
                    context={"cases": case_batch},
                )
            )

    return render_family_executables(
        out_dir,
        source_template,
        runner_template,
        shards,
        claimed_sources,
        tuple(runner_shards),
    )


def write_text_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.write_text(content, encoding="utf-8")
