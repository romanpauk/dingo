#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from plugins import bindings_type
from probe_config import FASTER_THAN_RUNTIME, MODE_PREFIXES, PROBE_SHAPES, ProbeShape
from rows import MatrixRow, generate_rows


@dataclass(frozen=True, slots=True)
class GeneratedProbe:
    symbol: str
    shape: str
    prefix: str
    container_type: str
    static_bindings: str | None
    setup_lines: tuple[str, ...]
    expression: str


def row_matches(row: MatrixRow, shape: ProbeShape, mode: str, container: str) -> bool:
    return (
        row.feature.name == shape.feature
        and row.mode.name == mode
        and row.scope.name == shape.scope
        and row.stored_type.id == shape.stored_type
        and row.exposed_type.name == shape.exposed_type
        and row.resolved_type.name == shape.resolved_type
        and row.container.name == container
    )


def select_row(rows: tuple[MatrixRow, ...], shape: ProbeShape, mode: str) -> MatrixRow:
    container = {
        "static": "static_container",
        "runtime": "runtime_container",
        "mixed": "container_mixed",
    }[mode]
    matches = [row for row in rows if row_matches(row, shape, mode, container)]
    if len(matches) != 1:
        raise RuntimeError(
            "expected one probe row for "
            f"{shape.name}/{mode}/{container}, found {len(matches)}"
        )
    return matches[0]


def setup_for(row: MatrixRow) -> tuple[str | None, tuple[str, ...]]:
    if row.mode.name == "static":
        return bindings_type(row.plan.static_bindings), ()
    if row.mode.name == "runtime":
        return None, row.plan.runtime_setup
    if row.mode.name == "mixed":
        if row.plan.mixed_static_bindings is None:
            raise RuntimeError(f"row {row.name} has no mixed static bindings")
        return bindings_type(row.plan.mixed_static_bindings), row.plan.mixed_runtime_setup
    raise RuntimeError(f"unknown registration mode: {row.mode.name}")


def resolution_prefix(row: MatrixRow, setup_lines: tuple[str, ...]) -> str:
    if row.mode.name == "mixed":
        resolution = "runtime" if setup_lines else "static"
        return f"{resolution}_resolution_mixed_container"
    return MODE_PREFIXES[row.mode.name]


def generate_probe_rows(rows: tuple[MatrixRow, ...]) -> tuple[GeneratedProbe, ...]:
    probes: list[GeneratedProbe] = []
    for shape in PROBE_SHAPES:
        if shape.limits is None:
            raise RuntimeError(f"probe shape {shape.name} has no limits")
        for mode in MODE_PREFIXES:
            if mode not in shape.limits:
                continue
            row = select_row(rows, shape, mode)
            static_bindings, setup_lines = setup_for(row)
            prefix = resolution_prefix(row, setup_lines)
            probes.append(
                GeneratedProbe(
                    symbol=f"probe_{prefix}_{shape.name}",
                    shape=shape.name,
                    prefix=prefix,
                    container_type=row.container.container_type,
                    static_bindings=static_bindings,
                    setup_lines=setup_lines,
                    expression=shape.expression,
                )
            )
    return tuple(probes)


def expectation_data(probes: tuple[GeneratedProbe, ...]) -> dict[str, object]:
    symbols = {probe.symbol for probe in probes}
    shapes: dict[str, tuple[str, ...]] = {}
    for shape in PROBE_SHAPES:
        shape_prefixes = tuple(
            probe.prefix for probe in probes if probe.shape == shape.name
        )
        if shape_prefixes:
            shapes[shape.name] = shape_prefixes

    probe_limits = {}
    tiny_probes = {}
    faster_than_runtime = []

    for shape in PROBE_SHAPES:
        if shape.limits is None:
            continue
        for probe in (probe for probe in probes if probe.shape == shape.name):
            mode = {
                "static_resolution": "static",
                "runtime_resolution": "runtime",
                "static_resolution_mixed_container": "mixed",
                "runtime_resolution_mixed_container": "mixed",
            }[probe.prefix]
            limit = shape.limits.get(mode)
            if limit is None:
                continue
            probe_limits[probe.symbol] = limit.as_dict()
            if shape.tiny_static and mode == "static":
                tiny_probes[probe.symbol] = 1

        if shape.name in FASTER_THAN_RUNTIME:
            static_symbol = f"probe_{MODE_PREFIXES['static']}_{shape.name}"
            runtime_symbol = f"probe_{MODE_PREFIXES['runtime']}_{shape.name}"
            if static_symbol in symbols and runtime_symbol in symbols:
                faster_than_runtime.append((static_symbol, runtime_symbol))

    return {
        "probe_limits": probe_limits,
        "faster_than_runtime": tuple(faster_than_runtime),
        "shapes": shapes,
        "tiny_probes": tiny_probes,
    }


def write_text_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def generate(source: Path, expectations: Path) -> None:
    script_dir = Path(__file__).resolve().parent
    env = Environment(
        loader=FileSystemLoader(script_dir / "templates"),
        undefined=StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
    )

    rows = generate_rows()
    probes = generate_probe_rows(rows)
    expectations_data = expectation_data(probes)

    write_text_if_changed(
        source,
        env.get_template("probes.cpp.j2").render(probes=probes),
    )
    write_text_if_changed(
        expectations,
        env.get_template("probe_expectations.py.j2").render(
            probe_limits=repr(expectations_data["probe_limits"]),
            faster_than_runtime=repr(expectations_data["faster_than_runtime"]),
            shapes=repr(expectations_data["shapes"]),
            tiny_probes=repr(expectations_data["tiny_probes"]),
        ),
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--expectations", required=True, type=Path)
    args = parser.parse_args()

    generate(args.source, args.expectations)


if __name__ == "__main__":
    main()
