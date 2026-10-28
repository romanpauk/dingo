#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from spec.gen.resolution.consumers import (
    CONSUMERS,
    NON_COMPOSABLE,
    build_files,
    render,
)


def _case(slug: str) -> dict[str, object]:
    return {
        "path": f"resolution/runtime/shared/plain/resolve/copyable/concrete/{slug}",
        "name": f"copyable_concrete_{slug}",
        "scope": "shared",
        "storage": "plain",
        "binding": "concrete",
        "traits": "copyable",
        "request": "copyable &",
        "instance": "",
        "registration": (
            "container.register_type<dingo::scope<dingo::shared>, "
            "dingo::storage<copyable>>();"
        ),
    }


def _manifest() -> dict[str, object]:
    return {
        "shards": [
            {
                "scope": "shared",
                "storage": "plain",
                "cases": [_case("t"), _case("t_ref")],
            }
        ]
    }


def test_consumers_share_the_resolve_rows() -> None:
    manifest = _manifest()
    rows = {
        case["path"]
        for shard in manifest["shards"]
        for case in shard["cases"]
    }
    for consumer in CONSUMERS:
        files = build_files(manifest, consumer)
        rendered = {case["claim_path"] for file in files for case in file.cases}
        expected = {
            row.replace("/resolve/", f"/{consumer}/", 1)
            for row in rows
            if row not in NON_COMPOSABLE[consumer]
        }
        assert rendered == expected


def test_consumer_cases_live_next_to_resolve() -> None:
    manifest = _manifest()
    for consumer in CONSUMERS:
        for file in build_files(manifest, consumer):
            assert file.path == (
                f"resolution/runtime/{file.scope}/{file.storage}/{consumer}.cpp"
            )
            assert file.suite.endswith(f"_{consumer}")
            for case in file.cases:
                assert case["claim_path"].startswith(
                    f"resolution/runtime/{file.scope}/{file.storage}/"
                    f"{consumer}/"
                )
                content = render(file)
                assert f'RecordProperty("path", "{case["claim_path"]}")' in content


def test_declared_non_composable_rows_name_a_reason() -> None:
    assert set(NON_COMPOSABLE) == set(CONSUMERS)
    for consumer, rows in NON_COMPOSABLE.items():
        for path, reason in rows.items():
            assert path.startswith(f"resolution/runtime/")
            assert f"/resolve/" in path
            assert reason
