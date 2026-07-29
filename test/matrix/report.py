#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT

"""Render the generated matrix coverage report."""

from __future__ import annotations

from collections import Counter
from dataclasses import dataclass

from constructor_detection import (
    ConstructorArgumentConversionRow,
    ConstructorDetectionRow,
)
from dependency_composition import (
    DependencyCompositionCoverage,
    render_dependency_composition_coverage,
)
from exclusions import (
    MATRIX_EXCLUSIONS,
    Compiler,
    compiler_exclusions,
    exclusion_cases,
)
from schema import LimitationDisposition
from shared_cyclical import SharedCyclicalRow


MATRIX_REPORT = "matrix-coverage.md"


@dataclass(frozen=True, slots=True)
class CoverageLimit:
    area: str
    disposition: LimitationDisposition
    reason: str


COVERAGE_LIMITS = (
    CoverageLimit(
        area="Constructor detection",
        disposition=LimitationDisposition.COMPILER_LIMITATION,
        reason=(
            "The portable constructor-detection backend is compiled only when "
            "`!defined(_MSC_VER)`."
        ),
    ),
    CoverageLimit(
        area="Invocation",
        disposition=LimitationDisposition.COMPILER_LIMITATION,
        reason=(
            "The `std::move_only_function` scenario runs only when "
            "`__cpp_lib_move_only_function` is available."
        ),
    ),
    CoverageLimit(
        area="Dependency compositions",
        disposition=LimitationDisposition.KNOWN_GAP,
        reason=(
            "Stored value, reference, pointer, and qualified source forms, "
            "including const-pointee smart pointers, are represented but not "
            "crossed recursively."
        ),
    ),
    CoverageLimit(
        area="Arrays",
        disposition=LimitationDisposition.KNOWN_GAP,
        reason=(
            "The recursive composition axis uses only `std::array<T, 2>`; the "
            "zero-size boundary is not represented."
        ),
    ),
    CoverageLimit(
        area="Index injection",
        disposition=LimitationDisposition.KNOWN_GAP,
        reason="Runtime string-literal keys remain a disabled future feature.",
    ),
)


def _disposition_name(disposition: LimitationDisposition) -> str:
    return disposition.value.replace("_", " ").title()


def render_matrix_report(
    composition: DependencyCompositionCoverage,
    detection_rows: tuple[ConstructorDetectionRow, ...],
    conversion_rows: tuple[ConstructorArgumentConversionRow, ...],
    shared_cyclical_rows: tuple[SharedCyclicalRow, ...],
    *,
    profile: str,
    compiled_composition_rows: int,
    compiler: Compiler = Compiler(),
    omitted_composition_cases: frozenset[tuple[str, str]] = frozenset(),
) -> str:
    enabled_exclusions = compiler_exclusions(compiler)
    enabled_cases = exclusion_cases(enabled_exclusions)
    unexpected_omissions = omitted_composition_cases - enabled_cases
    if unexpected_omissions:
        raise ValueError(
            "omitted composition cases are not covered by enabled exclusions: "
            f"{sorted(unexpected_omissions)}"
        )
    detection_limitations = Counter(
        (
            row.limitation.disposition,
            row.limitation.guard,
            row.unsupported_reason,
        )
        for row in detection_rows
        if not row.supported and row.limitation is not None
    )
    conversion_limitations = Counter(
        (
            row.category.limitation_disposition,
            row.category.limitation_guard,
            row.category.limitation_reason,
        )
        for row in conversion_rows
        if row.category.limitation_reason is not None
    )
    shared_cyclical_limitations = Counter(
        (
            row.unsupported_disposition,
            row.unsupported_reason,
        )
        for row in shared_cyclical_rows
        if not row.supported
    )
    if any(
        disposition is None or reason is None
        for disposition, reason in shared_cyclical_limitations
    ):
        raise ValueError("shared cyclical limitation is incomplete")
    summary = Counter(
        {
            ("Dependency compositions", disposition): sum(
                count
                for item_disposition, _, count in composition.limitations
                if item_disposition is disposition
            )
            for disposition in LimitationDisposition
        }
    )
    for (disposition, _, _), count in detection_limitations.items():
        summary[("Constructor detection", disposition)] += count
    for (disposition, _, _), count in conversion_limitations.items():
        if disposition is not None:
            summary[("Constructor argument conversion", disposition)] += count
    for (disposition, _), count in shared_cyclical_limitations.items():
        if disposition is not None:
            summary[("Shared cyclical", disposition)] += count
    for limit in COVERAGE_LIMITS:
        summary[("Coverage limits", limit.disposition)] += 1
    summary[
        ("Omitted compiler cases", LimitationDisposition.COMPILER_LIMITATION)
    ] = len(omitted_composition_cases)

    lines = [
        "# Matrix Coverage",
        "",
        "This build-local report includes modeled behavior, skipped tests, ",
        "compiler exclusions, and known boundaries outside the current axes.",
        "",
        f"Toolchain: `{compiler.description}`.",
        "",
        "## Limitation Summary",
        "",
        "| Family | Known Gap | Intentional Constraint | Compiler Limitation |",
        "| --- | ---: | ---: | ---: |",
    ]
    for family in (
        "Dependency compositions",
        "Constructor detection",
        "Constructor argument conversion",
        "Shared cyclical",
        "Omitted compiler cases",
        "Coverage limits",
    ):
        lines.append(
            f"| {family} | "
            f"{summary[(family, LimitationDisposition.KNOWN_GAP)]} | "
            f"{summary[(family, LimitationDisposition.INTENTIONAL_CONSTRAINT)]} | "
            f"{summary[(family, LimitationDisposition.COMPILER_LIMITATION)]} |"
        )

    lines.extend(
        (
            "",
            *render_dependency_composition_coverage(
                composition,
                profile=profile,
                compiled_rows=compiled_composition_rows,
                heading_level=2,
            )
            .rstrip()
            .splitlines(),
            "",
            "## Constructor Detection",
            "",
            "| Total Cells | Supported Everywhere | Unconditional Limitations | "
            "Conditional Limitations |",
            "| ---: | ---: | ---: | ---: |",
        )
    )
    conditional_detection = sum(
        count
        for (_, guard, _), count in detection_limitations.items()
        if guard is not None
    )
    unconditional_detection = sum(detection_limitations.values()) - (
        conditional_detection
    )
    lines.append(
        f"| {len(detection_rows)} | "
        f"{sum(row.supported for row in detection_rows)} | "
        f"{unconditional_detection} | {conditional_detection} |"
    )
    lines.extend(
        (
            "",
            "### Detection Limitations",
            "",
            "| Disposition | Condition | Reason | Affected Cells |",
            "| --- | --- | --- | ---: |",
        )
    )
    lines.extend(
        f"| {_disposition_name(disposition)} | "
        f"{guard or 'All toolchains'} | {reason} | {count} |"
        for (disposition, guard, reason), count in sorted(
            detection_limitations.items(),
            key=lambda item: (
                item[0][0].value,
                item[0][1] or "",
                item[0][2],
            ),
        )
        if reason is not None
    )

    lines.extend(
        (
            "",
            "## Shared Cyclical",
            "",
            "| Total Cells | Supported | Skipped Constraints |",
            "| ---: | ---: | ---: |",
            f"| {len(shared_cyclical_rows)} | "
            f"{sum(row.supported for row in shared_cyclical_rows)} | "
            f"{sum(shared_cyclical_limitations.values())} |",
            "",
            "### Shared Cyclical Constraints",
            "",
            "| Disposition | Reason | Affected Cells |",
            "| --- | --- | ---: |",
        )
    )
    lines.extend(
        f"| {_disposition_name(disposition)} | {reason} | {count} |"
        for (disposition, reason), count in sorted(
            shared_cyclical_limitations.items(),
            key=lambda item: (
                item[0][0].value if item[0][0] is not None else "",
                item[0][1] or "",
            ),
        )
        if disposition is not None and reason is not None
    )

    lines.extend(
        (
            "",
            "## Constructor Argument Conversion",
            "",
            "| Total Cells | Unconditional Cells | Conditionally Skipped Cells |",
            "| ---: | ---: | ---: |",
            f"| {len(conversion_rows)} | "
            f"{len(conversion_rows) - sum(conversion_limitations.values())} | "
            f"{sum(conversion_limitations.values())} |",
            "",
            "### Conversion Limitations",
            "",
            "| Disposition | Condition | Reason | Affected Cells |",
            "| --- | --- | --- | ---: |",
        )
    )
    lines.extend(
        f"| {_disposition_name(disposition)} | {guard} | {reason} | {count} |"
        for (disposition, guard, reason), count in sorted(
            conversion_limitations.items(),
            key=lambda item: (
                item[0][0].value if item[0][0] is not None else "",
                item[0][1] or "",
                item[0][2] or "",
            ),
        )
        if disposition is not None and guard is not None and reason is not None
    )

    lines.extend(
        (
            "",
            "## Compiler Exclusions",
            "",
            "Named exclusions remain visible even when their cases are not "
            "selected by the active projection.",
            "",
            "| Exclusion | Condition | Enabled | Declared Cases | Omitted Cases "
            "| Reason |",
            "| --- | --- | --- | ---: | ---: | --- |",
        )
    )
    lines.extend(
        f"| `{exclusion.name}` | {exclusion.condition} | "
        f"{'yes' if exclusion.name in enabled_exclusions else 'no'} | "
        f"{len(exclusion.cases)} | "
        f"{len(exclusion.cases & omitted_composition_cases)} | "
        f"{exclusion.reason} |"
        for exclusion in MATRIX_EXCLUSIONS
    )

    lines.extend(
        (
            "",
            "## Coverage Limits",
            "",
            "These known behaviors are excluded by an axis or condition, or are "
            "not represented by the current matrix.",
            "",
            "| Disposition | Area | Limit |",
            "| --- | --- | --- |",
        )
    )
    lines.extend(
        f"| {_disposition_name(limit.disposition)} | {limit.area} | "
        f"{limit.reason} |"
        for limit in COVERAGE_LIMITS
    )
    return "\n".join(line.rstrip() for line in lines) + "\n"


__all__ = (
    "COVERAGE_LIMITS",
    "MATRIX_REPORT",
    "render_matrix_report",
)
