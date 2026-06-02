#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import argparse
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

from jinja2 import Environment, FileSystemLoader, StrictUndefined, Template

from plugins import bindings_type
from rows import MatrixRow, generate_rows


@dataclass(frozen=True, slots=True)
class GeneratedCase:
    suite: str
    name: str
    container_type: str
    static_bindings: str | None
    before_lines: tuple[str, ...]
    setup_lines: tuple[str, ...]
    check_lines: tuple[str, ...]
    after_lines: tuple[str, ...]
    system_headers: tuple[str, ...]
    support_headers: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class GeneratedExecutable:
    name: str
    sources: tuple[Path, ...]


def make_case(row: MatrixRow) -> GeneratedCase:
    static_bindings = None
    setup_lines = ()
    if row.mode.name == "static":
        static_bindings = bindings_type(row.plan.static_bindings)
    elif row.mode.name == "runtime":
        setup_lines = row.plan.runtime_setup
    elif row.mode.name == "mixed":
        if row.plan.mixed_static_bindings is None:
            raise RuntimeError(f"row {row.name} has no mixed static bindings")
        static_bindings = bindings_type(row.plan.mixed_static_bindings)
        setup_lines = row.plan.mixed_runtime_setup
    else:
        raise RuntimeError(f"unknown registration mode: {row.mode.name}")

    return GeneratedCase(
        suite=row.feature.name,
        name=row.name,
        container_type=row.container.container_type,
        static_bindings=static_bindings,
        before_lines=row.lines.before,
        setup_lines=setup_lines,
        check_lines=row.lines.check,
        after_lines=row.lines.after,
        system_headers=tuple(
            sorted(
                {
                    *row.feature.system_headers,
                    *row.stored_type.system_headers,
                    *row.exposed_type.system_headers,
                    *row.resolved_type.system_headers,
                    *row.container.system_headers,
                }
            )
        ),
        support_headers=tuple(
            sorted(
                {
                    *row.feature.support_headers,
                    *row.stored_type.support_headers,
                    *row.exposed_type.support_headers,
                    *row.resolved_type.support_headers,
                    *row.container.support_headers,
                }
            )
        ),
    )


def render_source(
    source: Path,
    rows: list[MatrixRow],
    template: Template,
) -> Path:
    cases: list[GeneratedCase] = []

    for row in rows:
        cases.append(make_case(row))

    if not cases:
        raise RuntimeError(f"source {source} did not generate any tests")

    rendered = template.render(
        cases=cases,
        system_headers=tuple(
            sorted({header for case in cases for header in case.system_headers})
        ),
        support_headers=tuple(
            sorted({header for case in cases for header in case.support_headers})
        ),
    )
    write_text_if_changed(source, rendered)
    return source


def render_runner(
    source: Path,
    rows: tuple[MatrixRow, ...],
    template: Template,
) -> Path:
    cases = [make_case(row) for row in rows]
    rendered = template.render(cases=cases)
    write_text_if_changed(source, rendered)
    return source


def executable_name(feature_name: str) -> str:
    return f"dingo_matrix_test_{feature_name}"


def source_groups(rows: tuple[MatrixRow, ...]) -> dict[str, list[MatrixRow]]:
    grouped: dict[str, list[MatrixRow]] = defaultdict(list)
    for row in rows:
        grouped[row.source_name].append(row)
    return grouped


def executable_groups(rows: tuple[MatrixRow, ...]) -> dict[str, list[MatrixRow]]:
    grouped: dict[str, list[MatrixRow]] = defaultdict(list)
    for row in rows:
        grouped[row.feature.name].append(row)
    return grouped


def generate_sources(
    rows: tuple[MatrixRow, ...],
    out_dir: Path,
    source_template: Template,
    runner_template: Template,
) -> tuple[GeneratedExecutable, ...]:
    sources_by_group: dict[str, Path] = {}
    rows_by_source = source_groups(rows)
    for group_name, group_rows in rows_by_source.items():
        source = out_dir / f"{group_name}.cpp"
        sources_by_group[group_name] = render_source(source, group_rows, source_template)

    executables: list[GeneratedExecutable] = []
    for feature_name, feature_rows in executable_groups(rows).items():
        runner = render_runner(
            out_dir / f"matrix_runner_{feature_name}.cpp",
            tuple(feature_rows),
            runner_template,
        )
        feature_sources = {
            sources_by_group[row.source_name] for row in feature_rows
        }
        executables.append(
            GeneratedExecutable(
                name=executable_name(feature_name),
                sources=tuple(sorted((*feature_sources, runner))),
            )
        )

    return tuple(sorted(executables, key=lambda executable: executable.name))


def generate(out_dir: Path, cmake_file: Path) -> None:
    script_dir = Path(__file__).resolve().parent
    env = Environment(
        loader=FileSystemLoader(script_dir / "templates"),
        undefined=StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
    )

    out_dir.mkdir(parents=True, exist_ok=True)
    rows = generate_rows()
    executables = generate_sources(
        rows,
        out_dir,
        env.get_template("source.cpp.j2"),
        env.get_template("runner.cpp.j2"),
    )
    cmake_file.parent.mkdir(parents=True, exist_ok=True)
    write_text_if_changed(
        cmake_file,
        env.get_template("cmake.cmake.j2").render(
            executables=[
                {
                    "name": executable.name,
                    "sources": [source.as_posix() for source in executable.sources],
                }
                for executable in executables
            ]
        ),
    )


def write_text_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.write_text(content, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--cmake", required=True, type=Path)
    args = parser.parse_args()

    generate(args.out, args.cmake)


if __name__ == "__main__":
    main()
