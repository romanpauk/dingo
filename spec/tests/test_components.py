"""The components of the library: the directories of include/dingo, the
dependencies tools/dependencies.py enforces, and the overview that describes them."""

import importlib.util
import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
INCLUDE = REPO_ROOT / "include" / "dingo"
OVERVIEW = REPO_ROOT / "docs" / "architecture" / "overview.md"
# The directories of the public headers that no component holds.
PUBLIC_DIRECTORIES = {"registration", "rtti"}


def checker():
    found = importlib.util.spec_from_file_location("component_check", REPO_ROOT / "tools" / "dependencies.py")
    module = importlib.util.module_from_spec(found)
    found.loader.exec_module(module)
    return module


def test_every_directory_is_a_component_or_public():
    directories = {path.name for path in INCLUDE.iterdir() if path.is_dir()}
    assert directories - PUBLIC_DIRECTORIES == set(checker().COMPONENTS)


def test_the_dependencies_name_components_that_come_before():
    order = list(checker().COMPONENTS)
    for position, (name, dependencies) in enumerate(checker().COMPONENTS.items()):
        assert set(dependencies) <= set(order[:position]), name


def test_the_overview_describes_every_component_in_order():
    text = OVERVIEW.read_text()
    positions = [text.find(f"`{name}/`") for name in checker().COMPONENTS]
    assert all(position >= 0 for position in positions), positions
    assert positions == sorted(positions)


def test_the_overview_names_the_dependencies_of_a_component():
    text = OVERVIEW.read_text()
    entry = re.search(r"`conversion/`: .*?Spec: `rules`", text, re.S).group(0)
    named = set(re.findall(r"`([a-z]+)`", entry.split("Depends on")[1]))
    assert set(checker().COMPONENTS["conversion"]) == named & set(checker().COMPONENTS)
