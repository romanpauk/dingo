#!/usr/bin/env python3

import argparse
import json
import os
import tempfile
import unittest
from pathlib import Path

from compile_time_check import (
    digest_bytes,
    extract_trace_metrics,
    make_budget,
    metric_limit,
    resolve_compiler,
    validate_identity,
)


class CompileTimeCheckTest(unittest.TestCase):
    def test_extracts_summary_and_named_family_counts(self) -> None:
        trace = {
            "traceEvents": [
                {"name": "Total InstantiateClass", "args": {"count": 12}},
                {"name": "Total InstantiateFunction", "args": {"count": 7}},
                {"name": "Total ParseClass", "args": {"count": 31}},
                {
                    "name": "InstantiateClass",
                    "args": {"detail": "dingo::detail::binding_selection<int>"},
                },
                {
                    "name": "InstantiateFunction",
                    "args": {"detail": "dingo::detail::binding_selection<long>"},
                },
            ]
        }

        metrics = extract_trace_metrics(
            trace, {"selection": ["binding_selection<"]}
        )

        self.assertEqual(metrics["instantiate_class"], 12)
        self.assertEqual(metrics["instantiate_function"], 7)
        self.assertEqual(metrics["parse_class"], 31)
        self.assertEqual(metrics["family.selection"], 2)

    def test_deterministic_counters_are_exact(self) -> None:
        self.assertEqual(metric_limit("instantiate_class", 100), 100)
        self.assertEqual(metric_limit("family.direct", 0), 0)

    def test_memory_limit_has_five_percent_headroom(self) -> None:
        self.assertEqual(metric_limit("max_rss_kb", 1000), 1050)

    def test_budget_uses_a_single_reference_measurement(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            project_root = Path(directory)
            fixture_dir = project_root / "test" / "compile_time"
            fixture_dir.mkdir(parents=True)
            manifest_path = fixture_dir / "manifest.json"
            manifest_path.write_text(
                json.dumps(
                    {
                        "fixtures": [
                            {"name": "fixture", "source": "fixture.cpp"}
                        ],
                        "standard": 20,
                    }
                )
            )
            measurement_path = project_root / "measurement.json"
            measurement_path.write_text(
                json.dumps(
                    {
                        "compiler": {"family": "clang", "major": 22},
                        "fixtures": {
                            "fixture": {
                                "digest": "fixture-digest",
                                "metrics": {
                                    "instantiate_class": 10,
                                    "max_rss_kb": 1000,
                                },
                            }
                        },
                        "manifest_digest": digest_bytes(manifest_path.read_bytes()),
                        "schema": 1,
                    }
                )
            )
            output_path = project_root / "budget.json"

            budget = make_budget(
                argparse.Namespace(
                    measurement=measurement_path,
                    output=output_path,
                    project_root=project_root,
                    reference_commit="reference-commit",
                )
            )

            self.assertEqual(budget["schema"], 2)
            self.assertEqual(budget["reference_commit"], "reference-commit")
            self.assertNotIn("baseline_commit", budget)
            self.assertEqual(
                budget["fixtures"]["fixture"]["metrics"]["instantiate_class"],
                {"limit": 10, "reference": 10},
            )
            self.assertEqual(
                budget["fixtures"]["fixture"]["metrics"]["max_rss_kb"],
                {"limit": 1050, "reference": 1000},
            )

    def test_compiler_identity_ignores_patch_version_text(self) -> None:
        expected = {
            "family": "clang",
            "major": 22,
            "target": "x86_64-linux-gnu",
            "version": "clang version 22.0.0",
        }
        observed = {**expected, "version": "clang version 22.1.3"}
        self.assertEqual(validate_identity(expected, observed), [])

    def test_rejects_ccache_compiler_shim(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            ccache = Path(directory) / "ccache"
            ccache.write_text("#!/bin/sh\n")
            ccache.chmod(0o755)
            compiler = Path(directory) / "clang++"
            os.symlink(ccache, compiler)

            with self.assertRaisesRegex(RuntimeError, "not a ccache shim"):
                resolve_compiler(str(compiler))


if __name__ == "__main__":
    unittest.main()
