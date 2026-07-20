#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Project dependency compositions into resolution and invocation tests."""

from __future__ import annotations

from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path

from jinja2 import Template

from axes.containers import CONTAINERS
from axes.dependency_compositions import (
    DEPENDENCY_COMPOSITIONS,
    DEPENDENCY_COMPOSITION_REQUEST_STRATEGIES,
    dependency_composition_depth,
    render_dependency_composition,
    render_dependency_composition_request,
)
from axes.scopes import SCOPES
from family import (
    SourceShard,
    assert_axis_members_used,
    assert_unique_axis_members,
    render_case_family_executables,
)
from plugins import (
    RegistrationRecipe,
    build_registration_plan_from_recipe,
    render_registration_plan,
)
from schema import (
    ContainerSpec,
    DependencyComposition,
    DependencyCompositionRequestStrategy,
    DependencyCompositionResolutionLimitation,
    GeneratedExecutable,
    LimitationDisposition,
    MixedRegistrationPlacement,
    RegistrationSpec,
    ScopeSpec,
)


@dataclass(frozen=True, slots=True)
class DependencyCompositionOperation:
    name: str
    policy: str


@dataclass(frozen=True, slots=True)
class DependencyCompositionScopeRule:
    scope: str
    request_strategies: frozenset[str]
    requires_movable: bool = False
    non_movable_reason: str | None = None
    non_movable_disposition: LimitationDisposition | None = None
    requires_runtime_registration: bool = False
    static_registration_reason: str | None = None
    static_registration_disposition: LimitationDisposition | None = None


@dataclass(frozen=True, slots=True)
class DependencyCompositionRow:
    composition: DependencyComposition
    operation: DependencyCompositionOperation
    container: ContainerSpec
    scope: ScopeSpec
    request_strategy: DependencyCompositionRequestStrategy
    type_name: str
    request_type: str
    name: str
    supported: bool
    unsupported_reason: str | None
    unsupported_disposition: LimitationDisposition | None


@dataclass(frozen=True, slots=True)
class GeneratedDependencyCompositionCase:
    suite: str
    name: str
    container_type: str
    static_bindings: str | None
    setup_lines: tuple[str, ...]
    policy: str
    system_headers: tuple[str, ...]
    headers: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class DependencyCompositionCoverageCount:
    supported: int
    functionality_gaps: int
    intentional_constraints: int


@dataclass(frozen=True, slots=True)
class DependencyCompositionCoverageCell:
    name: str
    count: DependencyCompositionCoverageCount


@dataclass(frozen=True, slots=True)
class DependencyCompositionCoverageAxis:
    name: str
    cells: tuple[DependencyCompositionCoverageCell, ...]


@dataclass(frozen=True, slots=True)
class DependencyCompositionCoverage:
    overall: DependencyCompositionCoverageCount
    axes: tuple[DependencyCompositionCoverageAxis, ...]
    limitations: tuple[
        tuple[LimitationDisposition, str, int], ...
    ]


DEPENDENCY_COMPOSITION_OPERATIONS = (
    DependencyCompositionOperation("resolve", "resolution::composition_resolve"),
    DependencyCompositionOperation("invoke", "resolution::composition_invoke"),
)


_CONTAINERS_BY_NAME = {container.name: container for container in CONTAINERS}
DEPENDENCY_COMPOSITION_CONTAINERS = tuple(
    _CONTAINERS_BY_NAME[name]
    for name in (
        "runtime_container",
        "container_runtime",
        "static_container",
        "container_static",
        "container_mixed",
    )
)


_SCOPES_BY_NAME = {scope.name: scope for scope in SCOPES}
DEPENDENCY_COMPOSITION_SCOPES = tuple(
    _SCOPES_BY_NAME[name] for name in ("shared", "unique", "external")
)


DEPENDENCY_COMPOSITION_SCOPE_RULES = (
    DependencyCompositionScopeRule(
        scope="shared",
        request_strategies=frozenset({"stable"}),
        requires_movable=True,
        non_movable_reason=(
            "shared storage cannot materialize a composition that is not "
            "movable"
        ),
        non_movable_disposition=(
            LimitationDisposition.INTENTIONAL_CONSTRAINT
        ),
    ),
    DependencyCompositionScopeRule(
        scope="unique",
        request_strategies=frozenset({"value", "rvalue"}),
        requires_movable=True,
        non_movable_reason=(
            "unique storage cannot materialize a composition that is not "
            "movable"
        ),
        non_movable_disposition=(
            LimitationDisposition.INTENTIONAL_CONSTRAINT
        ),
    ),
    DependencyCompositionScopeRule(
        scope="external",
        request_strategies=frozenset({"stable"}),
        requires_runtime_registration=True,
        static_registration_reason=(
            "external storage requires runtime registration and cannot be "
            "provided by a static-only container"
        ),
        static_registration_disposition=(
            LimitationDisposition.INTENTIONAL_CONSTRAINT
        ),
    ),
)


_MIXED_ANCHOR_TYPE = "dependency_composition_mixed_anchor"
DEPENDENCY_COMPOSITION_COVERAGE_REPORT = "dependency-composition-coverage.md"
DEPENDENCY_COMPOSITION_PROFILES = frozenset({"full", "portable"})
DEPENDENCY_COMPOSITION_EXECUTABLES_PER_OPERATION = 4


ProjectionObligation = tuple[str, ...]
DependencyCompositionShardKey = tuple[str, str, str]


def _operator_limitation(
    composition: DependencyComposition,
    request_strategy: DependencyCompositionRequestStrategy,
) -> DependencyCompositionResolutionLimitation | None:
    def matches_filters(
        node: DependencyComposition,
        limitation: DependencyCompositionResolutionLimitation,
    ) -> bool:
        if (
            limitation.request_strategies
            and request_strategy.name not in limitation.request_strategies
        ):
            return False
        return not limitation.operand_operators or any(
            operand.operator is not None
            and operand.operator.name in limitation.operand_operators
            for operand in node.operands
        )

    if composition.operator is None:
        return None
    for limitation in composition.operator.resolution_limitations:
        if (
            limitation.position == "request_composed_operand"
            and any(
                operand.operator is not None
                for operand in composition.operands
            )
            and matches_filters(composition, limitation)
        ):
            return limitation

    def nested_limitation(
        node: DependencyComposition,
    ) -> DependencyCompositionResolutionLimitation | None:
        if node.operator is not None:
            for limitation in node.operator.resolution_limitations:
                if limitation.position == "nested" and matches_filters(
                    node, limitation
                ):
                    return limitation
        for operand in node.operands:
            limitation = nested_limitation(operand)
            if limitation is not None:
                return limitation
        return None

    for operand in composition.operands:
        limitation = nested_limitation(operand)
        if limitation is not None:
            return limitation
    return None


def _container_mode(container: ContainerSpec) -> str:
    if len(container.modes) != 1:
        raise ValueError(
            "dependency composition container "
            f"{container.name} must select exactly one registration mode"
        )
    mode = next(iter(container.modes))
    if mode not in {"runtime", "static", "mixed"}:
        raise ValueError(
            "dependency composition container "
            f"{container.name} has an unsupported registration mode: {mode}"
        )
    return mode


def generate_dependency_composition_rows(
    compositions: tuple[DependencyComposition, ...] = DEPENDENCY_COMPOSITIONS,
    operations: tuple[
        DependencyCompositionOperation, ...
    ] = DEPENDENCY_COMPOSITION_OPERATIONS,
    containers: tuple[
        ContainerSpec, ...
    ] = DEPENDENCY_COMPOSITION_CONTAINERS,
    scopes: tuple[ScopeSpec, ...] = DEPENDENCY_COMPOSITION_SCOPES,
    scope_rules: tuple[
        DependencyCompositionScopeRule, ...
    ] = DEPENDENCY_COMPOSITION_SCOPE_RULES,
    request_strategies: tuple[
        DependencyCompositionRequestStrategy, ...
    ] = DEPENDENCY_COMPOSITION_REQUEST_STRATEGIES,
) -> tuple[DependencyCompositionRow, ...]:
    assert_unique_axis_members(
        "dependency composition",
        "request strategy",
        tuple(strategy.name for strategy in request_strategies),
    )
    assert_unique_axis_members(
        "dependency composition",
        "composition",
        tuple(composition.name for composition in compositions),
    )
    assert_unique_axis_members(
        "dependency composition",
        "operation",
        tuple(operation.name for operation in operations),
    )
    assert_unique_axis_members(
        "dependency composition",
        "container",
        tuple(container.name for container in containers),
    )
    assert_unique_axis_members(
        "dependency composition",
        "scope",
        tuple(scope.name for scope in scopes),
    )
    assert_unique_axis_members(
        "dependency composition",
        "scope rule",
        tuple(rule.scope for rule in scope_rules),
    )
    request_strategies_by_name = {
        strategy.name: strategy for strategy in request_strategies
    }
    scope_rules_by_name = {rule.scope: rule for rule in scope_rules}
    if scope_rules_by_name.keys() != {scope.name for scope in scopes}:
        raise ValueError(
            "dependency composition scope rules do not match the scope axis"
        )
    for container in containers:
        _container_mode(container)
    for rule in scope_rules:
        if rule.requires_movable != (
            rule.non_movable_reason is not None
            and rule.non_movable_disposition is not None
        ) or (
            rule.non_movable_disposition is not None
            and not isinstance(
                rule.non_movable_disposition,
                LimitationDisposition,
            )
        ):
            raise ValueError(
                "dependency composition scope rule "
                f"{rule.scope} has an incomplete movable requirement"
            )
        if rule.requires_runtime_registration != (
            rule.static_registration_reason is not None
            and rule.static_registration_disposition is not None
        ) or (
            rule.static_registration_disposition is not None
            and not isinstance(
                rule.static_registration_disposition,
                LimitationDisposition,
            )
        ):
            raise ValueError(
                "dependency composition scope rule "
                f"{rule.scope} has an incomplete registration requirement"
            )
        unknown_request_strategies = (
            rule.request_strategies - request_strategies_by_name.keys()
        )
        if not rule.request_strategies or unknown_request_strategies:
            raise ValueError(
                "dependency composition scope rule "
                f"{rule.scope} has invalid request strategies: "
                f"{sorted(unknown_request_strategies)}"
            )

    rows_list: list[DependencyCompositionRow] = []
    for operation in operations:
        for container in containers:
            container_mode = _container_mode(container)
            for scope in scopes:
                scope_rule = scope_rules_by_name[scope.name]
                for request_strategy_name in sorted(
                    scope_rule.request_strategies
                ):
                    request_strategy = request_strategies_by_name[
                        request_strategy_name
                    ]
                    for composition in compositions:
                        unsupported_reason = None
                        unsupported_disposition = None
                        if (
                            scope_rule.requires_runtime_registration
                            and container_mode == "static"
                        ):
                            unsupported_reason = (
                                scope_rule.static_registration_reason
                            )
                            unsupported_disposition = (
                                scope_rule.static_registration_disposition
                            )
                        elif (
                            scope_rule.requires_movable
                            and not composition.movable
                        ):
                            unsupported_reason = scope_rule.non_movable_reason
                            unsupported_disposition = (
                                scope_rule.non_movable_disposition
                            )
                        elif (
                            composition.operator is not None
                            and request_strategy.name
                            not in composition.operator.supported_request_strategies
                        ):
                            unsupported_reason = (
                                f"{composition.operator.name} compositions do "
                                f"not support {request_strategy.name} requests"
                            )
                            unsupported_disposition = (
                                composition.operator.unsupported_request_disposition
                            )
                        else:
                            limitation = _operator_limitation(
                                composition,
                                request_strategy,
                            )
                            if limitation is not None:
                                unsupported_reason = limitation.reason
                                unsupported_disposition = (
                                    limitation.disposition
                                )
                        if (unsupported_reason is None) != (
                            unsupported_disposition is None
                        ):
                            raise ValueError(
                                "dependency composition limitation has an "
                                "incomplete disposition"
                            )
                        rows_list.append(
                            DependencyCompositionRow(
                                composition=composition,
                                operation=operation,
                                container=container,
                                scope=scope,
                                request_strategy=request_strategy,
                                type_name=render_dependency_composition(
                                    composition
                                ),
                                request_type=(
                                    render_dependency_composition_request(
                                        composition,
                                        request_strategy,
                                    )
                                ),
                                name=(
                                    f"{container.name}_{scope.name}_"
                                    f"{request_strategy.name}_"
                                    f"{composition.name}"
                                ),
                                supported=unsupported_reason is None,
                                unsupported_reason=unsupported_reason,
                                unsupported_disposition=(
                                    unsupported_disposition
                                ),
                            )
                        )
    rows = tuple(rows_list)
    for axis, declared, used in (
        (
            "composition",
            {composition.name for composition in compositions},
            {row.composition.name for row in rows},
        ),
        (
            "operation",
            {operation.name for operation in operations},
            {row.operation.name for row in rows},
        ),
        (
            "request strategy",
            {strategy.name for strategy in request_strategies},
            {row.request_strategy.name for row in rows},
        ),
        (
            "container",
            {container.name for container in containers},
            {row.container.name for row in rows},
        ),
        (
            "scope",
            {scope.name for scope in scopes},
            {row.scope.name for row in rows},
        ),
    ):
        assert_axis_members_used("dependency composition", axis, declared, used)
    return rows


def _composition_structure(
    composition: DependencyComposition,
) -> tuple[str, ...]:
    if composition.operator is None:
        raise ValueError(
            f"composition {composition.name} has no outer operator"
        )
    return (
        composition.operator.name,
        *(
            operand.operator.name if operand.operator else "leaf"
            for operand in composition.operands
        ),
        f"depth_{dependency_composition_depth(composition)}",
        f"copyable_{composition.copyable}",
        f"movable_{composition.movable}",
    )


def _projection_obligations(
    row: DependencyCompositionRow,
    profile: str,
) -> frozenset[ProjectionObligation]:
    if row.composition.operator is None:
        raise ValueError(
            f"composition {row.composition.name} has no outer operator"
        )
    operation = row.operation.name
    obligations = {
        (
            "operator_environment",
            operation,
            row.composition.operator.name,
            row.container.name,
            row.scope.name,
            row.request_strategy.name,
        ),
        (
            "mobility_scope",
            operation,
            f"copyable_{row.composition.copyable}",
            f"movable_{row.composition.movable}",
            row.scope.name,
            row.request_strategy.name,
        ),
    }
    if profile == "full":
        obligations.add(
            (
                "composition_operation",
                operation,
                row.composition.name,
            )
        )
    else:
        obligations.add(
            (
                "structure_operation",
                operation,
                *_composition_structure(row.composition),
            )
        )
    return frozenset(obligations)


def _projection_row_key(row: DependencyCompositionRow) -> tuple[str, ...]:
    if row.composition.operator is None:
        raise ValueError(
            f"composition {row.composition.name} has no outer operator"
        )
    return (
        row.operation.name,
        row.composition.operator.name,
        row.composition.name,
        row.container.name,
        row.scope.name,
        row.request_strategy.name,
    )


def project_dependency_composition_rows(
    rows: tuple[DependencyCompositionRow, ...],
    profile: str,
) -> tuple[DependencyCompositionRow, ...]:
    """Select supported C++ witnesses while keeping the model exhaustive."""
    if profile not in DEPENDENCY_COMPOSITION_PROFILES:
        raise ValueError(
            "unknown dependency composition profile: "
            f"{profile}; expected one of {sorted(DEPENDENCY_COMPOSITION_PROFILES)}"
        )
    candidates = tuple(row for row in rows if row.supported)
    if not candidates:
        raise RuntimeError(
            "dependency composition projection has no supported rows"
        )
    obligations_by_row = {
        row: _projection_obligations(row, profile) for row in candidates
    }
    rows_by_obligation: dict[
        ProjectionObligation, list[DependencyCompositionRow]
    ] = defaultdict(list)
    for row, obligations in obligations_by_row.items():
        for obligation in obligations:
            rows_by_obligation[obligation].append(row)

    uncovered = set(rows_by_obligation)
    selected: list[DependencyCompositionRow] = []
    selected_rows: set[DependencyCompositionRow] = set()
    for obligation in sorted(
        rows_by_obligation,
        key=lambda item: (len(rows_by_obligation[item]), item),
    ):
        if obligation not in uncovered:
            continue
        row = min(
            (
                candidate
                for candidate in rows_by_obligation[obligation]
                if candidate not in selected_rows
            ),
            key=lambda candidate: (
                -len(obligations_by_row[candidate] & uncovered),
                _projection_row_key(candidate),
            ),
        )
        selected.append(row)
        selected_rows.add(row)
        uncovered -= obligations_by_row[row]

    if uncovered:
        raise RuntimeError(
            "dependency composition projection cannot satisfy: "
            + ", ".join(" / ".join(item) for item in sorted(uncovered))
        )

    projected = tuple(sorted(selected, key=_projection_row_key))
    covered = set().union(
        *(_projection_obligations(row, profile) for row in projected)
    )
    expected = set().union(*obligations_by_row.values())
    if covered != expected:
        raise RuntimeError(
            "dependency composition projection did not satisfy its contract"
        )
    return projected


def _coverage_count(
    rows: tuple[DependencyCompositionRow, ...],
) -> DependencyCompositionCoverageCount:
    count = DependencyCompositionCoverageCount(
        supported=sum(row.supported for row in rows),
        functionality_gaps=sum(
            row.unsupported_disposition is LimitationDisposition.KNOWN_GAP
            for row in rows
        ),
        intentional_constraints=sum(
            row.unsupported_disposition
            is LimitationDisposition.INTENTIONAL_CONSTRAINT
            for row in rows
        ),
    )
    if (
        count.supported
        + count.functionality_gaps
        + count.intentional_constraints
        != len(rows)
    ):
        raise ValueError("composition rows have an incomplete support category")
    return count


def build_dependency_composition_coverage(
    rows: tuple[DependencyCompositionRow, ...] | None = None,
) -> DependencyCompositionCoverage:
    if rows is None:
        rows = generate_dependency_composition_rows()

    grouped: dict[str, dict[str, list[DependencyCompositionRow]]] = {
        axis: defaultdict(list)
        for axis in (
            "operation",
            "operator",
            "container",
            "scope",
            "request strategy",
            "copyability",
            "movability",
            "depth",
        )
    }
    for row in rows:
        if row.composition.operator is None:
            raise ValueError(
                f"composition {row.composition.name} has no outer operator"
            )
        values = {
            "operation": row.operation.name,
            "operator": row.composition.operator.name,
            "container": row.container.name,
            "scope": row.scope.name,
            "request strategy": row.request_strategy.name,
            "copyability": (
                "copyable" if row.composition.copyable else "non_copyable"
            ),
            "movability": (
                "movable" if row.composition.movable else "non_movable"
            ),
            "depth": str(dependency_composition_depth(row.composition)),
        }
        for axis, value in values.items():
            grouped[axis][value].append(row)

    axes = tuple(
        DependencyCompositionCoverageAxis(
            name=axis,
            cells=tuple(
                DependencyCompositionCoverageCell(
                    name=value,
                    count=_coverage_count(tuple(group_rows)),
                )
                for value, group_rows in sorted(grouped[axis].items())
            ),
        )
        for axis in grouped
    )
    limitations = Counter(
        (row.unsupported_disposition, row.unsupported_reason)
        for row in rows
        if not row.supported
    )
    if any(
        disposition is None or reason is None
        for disposition, reason in limitations
    ):
        raise ValueError(
            "unsupported composition row has an incomplete disposition"
        )
    if any(
        row.unsupported_disposition is not None
        or row.unsupported_reason is not None
        for row in rows
        if row.supported
    ):
        raise ValueError("supported composition row has a limitation")
    return DependencyCompositionCoverage(
        overall=_coverage_count(rows),
        axes=axes,
        limitations=tuple(
            (disposition, reason, count)
            for (disposition, reason), count in sorted(
                limitations.items(),
                key=lambda item: (item[0][0].value, item[0][1]),
            )
            if disposition is not None and reason is not None
        ),
    )


def render_dependency_composition_coverage(
    coverage: DependencyCompositionCoverage,
    *,
    profile: str | None = None,
    compiled_rows: int | None = None,
) -> str:
    lines = [
        "# Dependency Composition Coverage",
        "",
        "This summary separates supported behavior from functionality gaps and ",
        "intentional constraints. Counts include both resolution and invocation ",
        "operations.",
        "",
        "## Support Status",
        "",
        "| Supported Cases | Functionality-gap Cases | Intentional-constraint Cases |",
        "| ---: | ---: | ---: |",
        (
            f"| {coverage.overall.supported} | "
            f"{coverage.overall.functionality_gaps} | "
            f"{coverage.overall.intentional_constraints} |"
        ),
    ]
    if profile is not None or compiled_rows is not None:
        if profile not in DEPENDENCY_COMPOSITION_PROFILES:
            raise ValueError(
                "coverage report requires a known dependency composition "
                f"profile, got: {profile}"
            )
        if compiled_rows is None or compiled_rows < 1:
            raise ValueError(
                "coverage report requires at least one compiled row"
            )
        lines.extend(
            (
                "",
                "## Tested Supported Cases",
                "",
                "Each profile emits a representative projection of supported ",
                "cases as C++ tests. Supported cases not selected directly are ",
                "covered by the profile's structural and axis obligations.",
                "",
                "| Profile | Directly Tested | Supported Not Directly Tested |",
                "| --- | ---: | ---: |",
            )
        )
        lines.append(
            f"| `{profile}` | {compiled_rows} | "
            f"{coverage.overall.supported - compiled_rows} |"
        )
    for axis in coverage.axes:
        lines.extend(
            (
                "",
                f"## By {axis.name.title()}",
                "",
                f"| {axis.name.title()} | Supported | Functionality Gap | "
                "Intentional Constraint |",
                "| --- | ---: | ---: | ---: |",
            )
        )
        lines.extend(
            f"| `{cell.name}` | {cell.count.supported} | "
            f"{cell.count.functionality_gaps} | "
            f"{cell.count.intentional_constraints} |"
            for cell in axis.cells
        )
    for disposition, title, description in (
        (
            LimitationDisposition.KNOWN_GAP,
            "Functionality Gaps",
            (
                "These intended behaviors cannot currently be tested because "
                "of implementation limitations that should be addressed."
            ),
        ),
        (
            LimitationDisposition.INTENTIONAL_CONSTRAINT,
            "Intentional Constraints",
            (
                "These cases are outside the declared ownership or request "
                "model and are not implementation backlog."
            ),
        ),
    ):
        lines.extend(
            (
                "",
                f"## {title}",
                "",
                description,
                "",
                "| Reason | Affected Cells |",
                "| --- | ---: |",
            )
        )
        lines.extend(
            f"| {_escape_markdown_cell(reason)} | {count} |"
            for item_disposition, reason, count
            in coverage.limitations
            if item_disposition is disposition
        )
    return "\n".join(lines) + "\n"


def _escape_markdown_cell(value: str) -> str:
    return value.replace("|", "\\|")


def _make_case(
    row: DependencyCompositionRow,
) -> GeneratedDependencyCompositionCase:
    registration_type = row.type_name
    if (
        row.composition.operator is not None
        and row.composition.operator.name in {"pointer", "const_pointer"}
        and row.composition.operands[0].operator is None
    ):
        registration_type = render_dependency_composition(
            row.composition.operands[0]
        )
    storage = f"dingo::storage<{registration_type}>"
    interfaces = f"dingo::interfaces<{registration_type}>"
    factory = (
        "dingo::factory<dingo::function<&make_dependency_composition<"
        f"{registration_type}>>>"
    )
    container_mode = _container_mode(row.container)
    static_registration = RegistrationSpec(
        storage=storage,
        interfaces=interfaces,
        factory=factory,
        include_in_runtime=False,
        include_in_mixed=False,
    )
    if row.scope.name == "external":
        runtime_registration = RegistrationSpec(
            storage=f"dingo::storage<{registration_type} &>",
            interfaces=interfaces,
            runtime_setup=(f"{registration_type} external_value{{}};",),
            runtime_argument="external_value",
            mixed_placement=MixedRegistrationPlacement.RUNTIME,
            include_in_static=False,
        )
    else:
        runtime_registration = RegistrationSpec(
            interfaces=interfaces,
            mixed_placement=MixedRegistrationPlacement.RUNTIME,
            include_in_static=False,
        )
    anchor_storage = f"dingo::storage<{_MIXED_ANCHOR_TYPE}>"
    anchor_factory = (
        "dingo::factory<dingo::function<&make_dependency_composition<"
        f"{_MIXED_ANCHOR_TYPE}>>>"
    )
    mixed_anchor = RegistrationSpec(
        scope="dingo::scope<dingo::shared>",
        storage=anchor_storage,
        interfaces=f"dingo::interfaces<{_MIXED_ANCHOR_TYPE}>",
        factory=anchor_factory,
        include_in_static=False,
        include_in_runtime=False,
    )
    plan = build_registration_plan_from_recipe(
        RegistrationRecipe(
            scope=row.scope,
            storage=storage,
            factory=factory,
            registrations=(
                static_registration,
                runtime_registration,
                mixed_anchor,
            ),
        )
    )
    registration = render_registration_plan(
        container_mode,
        plan,
        context=f"dependency composition row {row.name}",
    )
    return GeneratedDependencyCompositionCase(
        suite=f"dependency_composition_{row.operation.name}",
        name=row.name,
        container_type=row.container.container_type,
        static_bindings=registration.static_bindings,
        setup_lines=registration.setup_lines,
        policy=(
            f"{row.operation.policy}<{row.type_name}, {row.request_type}>"
        ),
        system_headers=row.container.system_headers,
        headers=tuple(
            sorted(
                {
                    *row.container.headers,
                    "matrix/fixtures/dependency_shapes.h",
                    "matrix/policies/resolution.h",
                }
            )
        ),
    )


def generate_dependency_composition_executables(
    out_dir: Path,
    source_template: Template,
    runner_template: Template,
    claimed_sources: set[Path] | None = None,
    profile: str = "full",
) -> tuple[GeneratedExecutable, ...]:
    grouped: dict[DependencyCompositionShardKey, list[DependencyCompositionRow]] = (
        defaultdict(list)
    )
    rows = project_dependency_composition_rows(
        generate_dependency_composition_rows(), profile
    )
    for row in rows:
        if row.composition.operator is None:
            raise ValueError(
                f"composition {row.composition.name} has no outer operator"
            )
        grouped[
            (
                row.operation.name,
                row.composition.operator.name,
                row.container.name,
            )
        ].append(row)

    assigned_shards: list[
        tuple[
            DependencyCompositionShardKey,
            int,
            int,
            list[DependencyCompositionRow],
        ]
    ] = []
    for operation in sorted(
        {operation.name for operation in DEPENDENCY_COMPOSITION_OPERATIONS}
    ):
        operation_shards = tuple(
            (key, shard_rows)
            for key, shard_rows in grouped.items()
            if key[0] == operation
        )
        if len(operation_shards) < DEPENDENCY_COMPOSITION_EXECUTABLES_PER_OPERATION:
            raise RuntimeError(
                f"dependency composition operation {operation} has too few "
                "source shards to balance"
            )
        operation_size = sum(
            len(shard_rows) for _, shard_rows in operation_shards
        )
        maximum_shard_size = (
            operation_size + DEPENDENCY_COMPOSITION_EXECUTABLES_PER_OPERATION - 1
        ) // DEPENDENCY_COMPOSITION_EXECUTABLES_PER_OPERATION
        chunked_shards = tuple(
            (key, chunk, shard_rows[offset : offset + maximum_shard_size])
            for key, shard_rows in sorted(operation_shards)
            for chunk, offset in enumerate(
                range(0, len(shard_rows), maximum_shard_size)
            )
        )
        bucket_sizes = [
            0 for _ in range(DEPENDENCY_COMPOSITION_EXECUTABLES_PER_OPERATION)
        ]
        for key, chunk, shard_rows in sorted(
            chunked_shards,
            key=lambda item: (-len(item[2]), item[0], item[1]),
        ):
            bucket = min(
                range(DEPENDENCY_COMPOSITION_EXECUTABLES_PER_OPERATION),
                key=lambda candidate: (bucket_sizes[candidate], candidate),
            )
            assigned_shards.append((key, chunk, bucket, shard_rows))
            bucket_sizes[bucket] += len(shard_rows)

    rows_by_executable: dict[
        tuple[str, int], list[DependencyCompositionRow]
    ] = defaultdict(list)
    for (operation, _, _), _, bucket, shard_rows in sorted(assigned_shards):
        rows_by_executable[(operation, bucket)].extend(shard_rows)

    shards: list[SourceShard] = []
    for (operation, bucket), executable_rows in sorted(
        rows_by_executable.items()
    ):
        cases = tuple(_make_case(row) for row in executable_rows)
        name = f"dependency_composition_{operation}_{bucket + 1}"
        shards.append(
            SourceShard(
                executable=f"dingo_matrix_test_{name}",
                name=name,
                source_context={
                    "cases": cases,
                    "system_headers": (),
                    "headers": tuple(
                        sorted(
                            {
                                header
                                for case in cases
                                for header in case.headers
                            }
                        )
                    ),
                },
                runner_context={"cases": cases},
            )
        )
    return render_case_family_executables(
        out_dir,
        source_template,
        runner_template,
        tuple(shards),
        claimed_sources,
    )


__all__ = (
    "DEPENDENCY_COMPOSITION_COVERAGE_REPORT",
    "DEPENDENCY_COMPOSITION_CONTAINERS",
    "DEPENDENCY_COMPOSITION_EXECUTABLES_PER_OPERATION",
    "DEPENDENCY_COMPOSITION_OPERATIONS",
    "DEPENDENCY_COMPOSITION_PROFILES",
    "DEPENDENCY_COMPOSITION_SCOPES",
    "DEPENDENCY_COMPOSITION_SCOPE_RULES",
    "DependencyCompositionRow",
    "build_dependency_composition_coverage",
    "generate_dependency_composition_executables",
    "generate_dependency_composition_rows",
    "project_dependency_composition_rows",
    "render_dependency_composition_coverage",
)
