#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT

"""Generate behavioral scenarios outside the registration matrix."""

from __future__ import annotations

from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

from jinja2 import Template

from axes.scenarios import SCENARIO_CONTAINERS, SCENARIOS
from family import (
    SourceShard,
    assert_axis_members_used,
    assert_unique_axis_members,
    render_family_executables,
)
from schema import GeneratedExecutable, ScenarioContainer, ScenarioSpec


@dataclass(frozen=True, slots=True)
class ScenarioRow:
    scenario: ScenarioSpec
    container: ScenarioContainer
    name: str

    @property
    def suite(self) -> str:
        return self.scenario.suite


def generate_scenario_rows(
    scenarios: tuple[ScenarioSpec, ...] = SCENARIOS,
    containers: tuple[ScenarioContainer, ...] = SCENARIO_CONTAINERS,
) -> tuple[ScenarioRow, ...]:
    assert_unique_axis_members(
        "scenario",
        "scenario",
        tuple(f"{case.suite}/{case.name}" for case in scenarios),
    )
    assert_unique_axis_members(
        "scenario", "container", tuple(container.name for container in containers)
    )

    known_containers = {container.name for container in containers}
    rows: list[ScenarioRow] = []
    for scenario in scenarios:
        missing = sorted(scenario.supported_containers - known_containers)
        if missing:
            raise ValueError(
                f"scenario {scenario.suite}/{scenario.name} references unknown "
                f"containers: {missing}"
            )
        for container in containers:
            if container.name not in scenario.supported_containers:
                continue
            rows.append(
                ScenarioRow(
                    scenario=scenario,
                    container=container,
                    name=scenario.test_name.format(container=container.name),
                )
            )

    assert_unique_axis_members(
        "scenario",
        "generated test",
        tuple(f"{row.scenario.suite}/{row.name}" for row in rows),
    )
    assert_axis_members_used(
        "scenario",
        "scenario",
        {f"{scenario.suite}/{scenario.name}" for scenario in scenarios},
        {f"{row.scenario.suite}/{row.scenario.name}" for row in rows},
    )
    assert_axis_members_used(
        "scenario",
        "container",
        {container.name for container in containers},
        {row.container.name for row in rows},
    )
    return tuple(rows)


def _source_name(scenario: ScenarioSpec) -> str:
    if scenario.name == "default":
        return scenario.suite
    return f"{scenario.suite}_{scenario.name}"


def generate_scenario_executables(
    out_dir: Path,
    source_template: Template,
    runner_template: Template,
    claimed_sources: set[Path] | None = None,
) -> tuple[GeneratedExecutable, ...]:
    rows = generate_scenario_rows()
    source_groups: dict[tuple[str, str], list[ScenarioRow]] = defaultdict(list)
    for row in rows:
        source_groups[(row.scenario.suite, row.scenario.name)].append(row)

    shards: list[SourceShard] = []
    for source_rows in source_groups.values():
        scenario = source_rows[0].scenario
        source_name = _source_name(scenario)
        cases = tuple(source_rows)
        shards.append(
            SourceShard(
                executable=f"dingo_matrix_test_{scenario.suite}",
                name=source_name,
                source_context={
                    "cases": cases,
                    "system_headers": tuple(
                        sorted(
                            {
                                header
                                for row in cases
                                for header in row.scenario.system_headers
                            }
                        )
                    ),
                    "headers": tuple(
                        sorted(
                            {
                                *(
                                    header
                                    for row in cases
                                    for header in row.scenario.headers
                                ),
                                *(
                                    header
                                    for row in cases
                                    for header in row.container.headers
                                ),
                            }
                        )
                    ),
                },
                runner_context={"cases": cases},
            )
        )
    return render_family_executables(
        out_dir,
        source_template,
        runner_template,
        tuple(shards),
        claimed_sources,
    )
