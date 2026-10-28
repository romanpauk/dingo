#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate the exhaustive storage-form coverage probes.

The catalog owns the scope policies and the storage forms. This module derives
each form's exposure from the scope and form capabilities, then renders one
assertion per form. A missing form, a changed exposure, or a form that stops
publishing a request list fails to compile instead of silently dropping out of
the matrix.

This is intentionally a coverage generator, not a code multiplier: it emits one
assertion per form and one consumer to instantiate them.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path

from spec.gen.shared.runner import emit, render_template
from spec.gen.shared.hierarchy import CasePath
from spec.gen.shared.validation import validate, validate_catalog
from spec.model.semantics.forms import (
    LVALUE,
    POINTER,
    RVALUE,
    VALUE,
    exposure,
)

from spec.model.grammar.catalog import (
    FORM_BY_NAME,
    FORM_ENUM,
    FORMS,
    SCOPES,
)


def scope_exposures(scope) -> list[int]:
    rows = []
    for form_name in FORM_ENUM:
        if form_name == "unknown":
            rows.append(0)
            continue
        rows.append(exposure(scope, FORM_BY_NAME[form_name]))
    return rows


def build_rows() -> list[dict[str, object]]:
    rows = []
    for scope in SCOPES:
        for form in FORMS:
            bits = exposure(scope, form)
            enabled = bits != 0
            if scope.name == "shared_cyclical":
                # shared_cyclical publishes its own forms: the leaf and a
                # sharing handle, independent of the generic form lists.
                has_value = form.name in {
                    "shared_handle",
                    "shared_array_handle",
                }
                has_lvalue = enabled
                has_rvalue = False
                has_pointer = enabled
            else:
                has_value = bool(bits & VALUE) and form.has_value
                has_lvalue = bool(bits & LVALUE) and form.has_lvalue
                has_rvalue = bool(bits & RVALUE) and form.has_rvalue_reference
                has_pointer = bool(bits & POINTER) and form.has_pointer
            rows.append(
                {
                    "scope_name": scope.name,
                    "scope_type": scope.cpp_type,
                    "scope_header": scope.header,
                    "scope_stable": scope.stable,
                    "scope_consumes": scope.consumes,
                    "scope_guard": scope.guard,
                    "scope_retains": scope.retains_unresolved_source,
                    "scope_allows_cycles": scope.allows_cycles,
                    "form_name": form.name,
                    "form_type": form.cpp_type(),
                    "form_headers": form.headers,
                    "exposure": bits,
                    "enabled": enabled,
                    "value": has_value,
                    "lvalue": has_lvalue,
                    "rvalue": has_rvalue,
                    "pointer": has_pointer,
                }
            )
    return rows


def build_shards() -> list[dict[str, object]]:
    shards = []
    for row in build_rows():
        path = CasePath(
            phase="classification",
            container="",
            scope=str(row["scope_name"]),
            storage=str(row["form_name"]),
            category="storage",
            request="facts",
        )
        shards.append({**row, "path": path.file, "suite": path.suite,
                       "case_path": path.path})
    return shards


def render(shard: dict[str, object]) -> str:
    headers = sorted(
        {str(shard["scope_header"])}
        | {
            str(header)
            for header in shard["form_headers"]
            if not str(header).startswith("dingo/")
        }
    )
    return render_template(
        "storage_facts.cpp.j2",
        row=shard,
        headers=headers,
    )


def render_header(output: Path) -> str:
    validate()
    return render_template(
        "scopes.gen.h.j2",
        scopes=[
            {
                "name": scope.name,
                "stable": scope.stable,
                "consumes": scope.consumes,
                "guard": scope.guard,
                "retains": scope.retains_unresolved_source,
                "allows_cycles": scope.allows_cycles,
                "exposures": scope_exposures(scope),
            }
            for scope in SCOPES
        ],
        shared_cyclical_lists=scope_exposures(
            next(scope for scope in SCOPES if scope.name == "shared_cyclical")
        ),
    )


def format_cpp(content: str, executable: str | None) -> str:
    if executable is None:
        return content
    result = subprocess.run(
        [executable, "--style=file"],
        input=content,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        raise SystemExit(f"clang-format failed: {result.stderr}")
    return result.stdout


def manifest() -> str:
    import json

    return json.dumps(
        {
            "shards": [
                {
                    "path": shard["path"],
                    "suite": shard["suite"],
                    "case": shard["case_path"],
                }
                for shard in build_shards()
            ]
        },
        indent=2,
        sort_keys=True,
    ) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument(
        "--header",
        type=Path,
        help="committed scope-policy header to generate or check",
    )
    parser.add_argument(
        "--include",
        type=Path,
        help="include directory to validate the library catalogs against",
    )
    parser.add_argument(
        "--clang-format",
        default=shutil.which("clang-format"),
        help="clang-format executable used for the committed header",
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    if arguments.include is not None:
        validate_catalog(arguments.include)
    for shard in build_shards():
        emit(
            arguments.out_dir / str(shard["path"]),
            render(shard),
            check=arguments.check,
        )
    if arguments.manifest is not None:
        emit(arguments.manifest, manifest(), check=arguments.check)
    header_content = (
        format_cpp(render_header(arguments.header), arguments.clang_format)
        if arguments.header is not None
        else None
    )
    if arguments.check:
        if arguments.header is not None and (
            not arguments.header.exists()
            or arguments.header.read_text(encoding="utf-8") != header_content
        ):
            raise SystemExit(f"{arguments.header} is out of date")
        return
    if arguments.header is not None and header_content is not None:
        emit(arguments.header, header_content, check=False)


if __name__ == "__main__":
    main()
