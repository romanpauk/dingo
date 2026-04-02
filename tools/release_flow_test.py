#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import importlib.util
import pathlib
import tempfile
import unittest


MODULE_PATH = pathlib.Path(__file__).with_name("release_flow.py")
SPEC = importlib.util.spec_from_file_location("release_flow", MODULE_PATH)
release_flow = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(release_flow)


class ReleaseFlowTests(unittest.TestCase):
    def test_build_plan_for_new_release_line_from_master(self) -> None:
        plan = release_flow.build_release_plan(
            "1.2.0",
            "master",
            None,
            release_branch_exists=False,
            tag_exists=False,
        )

        self.assertEqual(plan["release_line"], "1.2")
        self.assertEqual(plan["release_branch"], "releases/1.2")
        self.assertEqual(plan["tag"], "v1.2.0")
        self.assertEqual(plan["source_kind"], "master")
        self.assertEqual(plan["release_branch_exists"], "false")
        self.assertEqual(plan["tag_exists"], "false")

    def test_master_release_requires_patch_zero(self) -> None:
        with self.assertRaisesRegex(ValueError, "x.y.0"):
            release_flow.build_release_plan(
                "1.2.3",
                "master",
                None,
                release_branch_exists=False,
                tag_exists=False,
            )

    def test_existing_release_branch_allows_master_resume(self) -> None:
        plan = release_flow.build_release_plan(
            "1.2.0",
            "master",
            None,
            release_branch_exists=True,
            tag_exists=True,
        )
        self.assertEqual(plan["release_branch_exists"], "true")
        self.assertEqual(plan["tag_exists"], "true")

    def test_patch_release_requires_matching_branch(self) -> None:
        with self.assertRaisesRegex(ValueError, "does not match release line"):
            release_flow.build_release_plan(
                "1.3.1",
                "releases/1.2",
                None,
                release_branch_exists=True,
                tag_exists=False,
            )

    def test_invalid_source_ref_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "source_ref must be"):
            release_flow.build_release_plan(
                "1.2.0",
                "feature/example",
                None,
                release_branch_exists=False,
                tag_exists=False,
            )

    def test_classify_ref_state(self) -> None:
        self.assertEqual(
            release_flow.classify_ref_state("abc123", None),
            "missing",
        )
        self.assertEqual(
            release_flow.classify_ref_state("abc123", "abc123"),
            "matches",
        )
        self.assertEqual(
            release_flow.classify_ref_state("abc123", "def456"),
            "conflict",
        )

    def test_set_cmake_version_inserts_and_updates_version(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            path = pathlib.Path(tempdir) / "CMakeLists.txt"
            path.write_text("cmake_minimum_required(VERSION 3.21)\nproject(dingo)\n")

            changed = release_flow.set_cmake_version(path, "2.4.0")
            self.assertTrue(changed)
            self.assertIn("project(dingo VERSION 2.4.0)\n", path.read_text())

            changed = release_flow.set_cmake_version(path, "2.4.0")
            self.assertFalse(changed)


if __name__ == "__main__":
    unittest.main()
