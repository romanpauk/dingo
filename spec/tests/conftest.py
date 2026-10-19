"""Test configuration of the spec tests.

`tmp_path` is replaced by a directory under the build tree, given by the
environment variable DINGO_SPEC_TEST_DIR and defaulting to build/spec-test, so
the tests never write to the system temporary directory.
"""

import os
import shutil
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture
def tmp_path(request):
    root = Path(os.environ.get("DINGO_SPEC_TEST_DIR", REPO_ROOT / "build" / "spec-test"))
    path = root / request.node.name
    shutil.rmtree(path, ignore_errors=True)
    path.mkdir(parents=True)
    yield path
    shutil.rmtree(path, ignore_errors=True)
