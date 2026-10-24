"""Names in the spec, its tests, the generator, the C++ library, and the docs.

A banned name fails the test, with the file, the line, and the word. The library
speaks in the words of the conversion rules: a stored type is a type tree of
wrappers over a leaf, a wrapper has wrapper semantics, the semantics have
operations, and an operation turns a reachable type into another along a route.
The words they replaced are banned, so they do not return.

The scanned paths are the ones already renamed; a rename adds its paths to
SCANNED, and the last one makes it the whole library.
"""

import ast
import io
import re
import sys
import tokenize
from pathlib import Path

import pytest

# The banned words, matched case-insensitively as a word of a name or of prose
# (a name is split at underscores and capitals; a plural matches too). `kind`
# is not banned: key_kind, factory_kind, and constructor_kind are names of this
# library.
BANNED = (
    "algebra",
    "vocabulary",
    "prepared",
    "layer",
    "helper",
    "util",
    "manager",
    "kernel",
    "shape",
    "term",
    "projection",
)
# Names that were renamed and must not return. They are matched as a whole run of
# words of a snake case name, so `route_form` is found in `route_form_data` and
# not in `route form`.
COMPOUNDS = (
    "route_plan",
    "route_form",
    "kind_form",
    "layer_kind",
    "layer_kinds",
)
# Phrases that contain a banned word in another sense, with the place they come
# from. They are removed before the words are matched.
ALLOWED_PHRASES = (
    # The module of the standard library that loads a module from a path.
    "importlib.util",
)
# Directories that must not exist, as names of a directory under the include
# directory or under test/next: the components of the library are directly under
# include/dingo and the tests of a component are in test/next/<component>.
BANNED_DIRECTORIES = (
    "core",
    "api",
    "vocabulary",
    "projection",
    "engine",
    "primitives",
    "request",
)

REPO_ROOT = Path(__file__).resolve().parents[2]
# Scanned recursively, as files or directories; each must exist.
SCANNED = (
    "README.md",
    "benchmark",
    "docs",
    "examples",
    "include/dingo",
    "spec",
    "test/contracts",
    "test/differential",
    "test/lit/generated",
    "test/matrix/common",
    "test/matrix/containers",
    "test/matrix/scenarios/lookup",
    "test/matrix/scenarios/runtime",
    "test/next",
    "test/routes",
    "test/selection",
    "test/support",
    "tools/generator",
)
# Not scanned: the file records the names of the tests of the suite that the
# library replaced, which are what they were.
EXCLUDED = ("docs/architecture/test-migration.md",)
SUFFIXES = frozenset({".py", ".h", ".hpp", ".cpp", ".j2", ".md", ".txt", ".cmake", ".t"})
SELF = Path(__file__).resolve()

WORD = re.compile(r"[A-Za-z][A-Za-z0-9]*")
PART = re.compile(r"[A-Z]+(?![a-z])|[A-Z]?[a-z]+")


def banned_in(text):
    """The banned words in the identifiers and words of `text`."""
    found = []
    cleaned = text
    for phrase in ALLOWED_PHRASES:
        cleaned = re.sub(re.escape(phrase), " ", cleaned, flags=re.IGNORECASE)
    for word in WORD.findall(cleaned):
        parts = [part.lower() for part in PART.findall(word)]
        for banned in BANNED:
            if any(part in (banned, banned + "s") for part in parts):
                found.append(banned)
    lowered = cleaned.lower()
    for compound in COMPOUNDS:
        if re.search(r"(?<![a-z0-9])" + compound + r"(?![a-z0-9])", lowered):
            found.append(compound)
    return found


def violations_of_text(path, text, line_offset=0):
    """(path, line, word, where) for every banned word of the lines of `text`."""
    result = []
    for number, line in enumerate(text.splitlines(), start=1 + line_offset):
        for word in banned_in(line):
            result.append((path, number, word, "text"))
    return result


def python_identifiers(tree):
    """(line, name) of every identifier a Python tree defines or uses."""
    for node in ast.walk(tree):
        names = []
        if isinstance(node, ast.Name):
            names.append(node.id)
        elif isinstance(node, ast.Attribute):
            # The module an attribute belongs to is part of its name, so an
            # allowed phrase such as importlib.util is found whole.
            if isinstance(node.value, ast.Name):
                names.append(node.value.id + "." + node.attr)
            else:
                names.append(node.attr)
        elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            names.append(node.name)
        elif isinstance(node, ast.arg):
            names.append(node.arg)
        elif isinstance(node, ast.keyword) and node.arg:
            names.append(node.arg)
        elif isinstance(node, ast.alias):
            names.extend((node.name, node.asname or ""))
        elif isinstance(node, ast.ImportFrom) and node.module:
            names.append(node.module)
        elif isinstance(node, (ast.Global, ast.Nonlocal)):
            names.extend(node.names)
        for name in names:
            if name:
                yield getattr(node, "lineno", 0), name


def violations_of_python(path, source, scan_prose=True):
    result = []
    tree = ast.parse(source)
    for line, name in python_identifiers(tree):
        for word in banned_in(name):
            result.append((path, line, word, "identifier"))
    if scan_prose:
        # Strings are prose (docstrings and descriptions) and comments are prose.
        for node in ast.walk(tree):
            if isinstance(node, ast.Constant) and isinstance(node.value, str):
                for offset, line in enumerate(node.value.splitlines()):
                    for word in banned_in(line):
                        result.append((path, node.lineno + offset, word, "string"))
        for token in tokenize.generate_tokens(io.StringIO(source).readline):
            if token.type == tokenize.COMMENT:
                for word in banned_in(token.string):
                    result.append((path, token.start[0], word, "comment"))
    return result


def violations_of_file(path, relative=None):
    relative = relative or path
    result = [(relative, 0, word, "file name") for word in banned_in(path.name)]
    if path.suffix not in SUFFIXES:
        return result
    source = path.read_text()
    if path.suffix == ".py":
        # This file names the banned words in BANNED, so its prose is not scanned.
        result += violations_of_python(relative, source, scan_prose=path.resolve() != SELF)
    else:
        result += violations_of_text(relative, source)
    return result


def scanned_files(root, scanned):
    excluded = {root / entry for entry in EXCLUDED}
    for entry in scanned:
        base = root / entry
        if base.is_file():
            files = [base]
        elif base.is_dir():
            files = (
                path
                for path in sorted(base.rglob("*"))
                if path.is_file() and "__pycache__" not in path.parts
            )
        else:
            continue
        yield from (path for path in files if path not in excluded)


def violations(root=REPO_ROOT, scanned=SCANNED):
    result = []
    for path in scanned_files(root, scanned):
        relative = path.relative_to(root)
        result += [
            (relative, 0, word, "directory name")
            for part in relative.parts[:-1]
            for word in banned_in(part)
        ]
        result += violations_of_file(path, relative)
    return result


def banned_directories(root=REPO_ROOT):
    """The directories of the library or of its tests that are named as one that
    was removed."""
    result = []
    for base in (root / "include" / "dingo", root / "test" / "next"):
        if base.is_dir():
            result += [
                (path.relative_to(root), 0, path.name, "directory")
                for path in sorted(base.rglob("*"))
                if path.is_dir() and path.name in BANNED_DIRECTORIES
            ]
    return result


def describe(found):
    return "\n".join(
        f"{path}:{line}: {word!r} ({where})" for path, line, word, where in found
    )


def test_no_banned_name_is_used():
    found = violations()
    assert not found, "banned names:\n" + describe(found)


def test_no_removed_directory_exists():
    found = banned_directories()
    assert not found, "removed directories:\n" + describe(found)


def test_every_scanned_path_exists():
    missing = [entry for entry in SCANNED if not (REPO_ROOT / entry).exists()]
    assert not missing, f"scanned paths that do not exist: {missing}"


@pytest.mark.parametrize("banned", BANNED)
def test_a_banned_name_is_found_in_every_place(tmp_path, banned):
    (tmp_path / "spec").mkdir()
    (tmp_path / "docs").mkdir()
    (tmp_path / "spec" / f"{banned}_names.py").write_text(
        f"def f(x):\n    return x\n\n\ndef g_{banned}():\n    '''Doc.'''\n"
        f"    return 1  # a {banned} comment\n\n\nclass {banned.capitalize()}:\n    pass\n"
    )
    (tmp_path / "docs" / "rules.md").write_text(f"# Title\n\nA line about the {banned}.\n")
    found = violations(tmp_path, ("spec", "docs"))
    where = {(Path(path).name, line, place) for path, line, word, place in found if word == banned}
    assert ("rules.md", 3, "text") in where
    assert (f"{banned}_names.py", 0, "file name") in where
    assert (f"{banned}_names.py", 5, "identifier") in where
    assert (f"{banned}_names.py", 7, "comment") in where
    assert (f"{banned}_names.py", 10, "identifier") in where


def test_a_plural_and_a_compound_are_found():
    assert banned_in("UserLayer") == ["layer"]
    assert banned_in("reference_layers") == ["layer"]
    assert banned_in("helpers and utils") == ["helper", "util"]
    assert banned_in("shutil <utility> messages") == []
    assert banned_in("reachable_terms") == ["term"]
    assert banned_in("Shapes") == ["shape"]
    assert banned_in("kernels_create") == ["kernel"]
    assert banned_in("TermIndex") == ["term"]
    # A word that only contains a banned word is not found.
    assert banned_in("terminate, determine, terminal, shaped, reshaped") == []


def test_kind_is_not_banned():
    assert banned_in("key_kind, FactoryKind, constructor_kind, kinds") == []


def test_the_module_of_the_standard_library_is_allowed():
    assert banned_in("importlib.util.module_from_spec") == []
    assert banned_in("a util module") == ["util"]


@pytest.mark.parametrize("compound", COMPOUNDS)
def test_a_renamed_name_is_found_in_every_place(tmp_path, compound):
    (tmp_path / "spec").mkdir()
    (tmp_path / "docs").mkdir()
    (tmp_path / "spec" / f"{compound}.py").write_text(
        f"def f(x):\n    return x\n\n\ndef {compound}_name():\n    '''Doc.'''\n"
        f"    return 1  # {compound} again\n"
    )
    (tmp_path / "docs" / "rules.md").write_text(f"# Title\n\nA line about `{compound}`.\n")
    found = violations(tmp_path, ("spec", "docs"))
    where = {(Path(path).name, line, place) for path, line, word, place in found if word == compound}
    assert ("rules.md", 3, "text") in where
    assert (f"{compound}.py", 0, "file name") in where
    assert (f"{compound}.py", 5, "identifier") in where
    assert (f"{compound}.py", 7, "comment") in where


def test_a_file_is_scanned_by_its_path(tmp_path):
    (tmp_path / "docs").mkdir()
    (tmp_path / "docs" / "clean.md").write_text("A shape.\n")
    (tmp_path / "docs" / "other.md").write_text("A shape.\n")
    found = violations(tmp_path, ("docs/clean.md",))
    assert [(Path(path).name, word) for path, line, word, place in found] == [("clean.md", "shape")]


def test_a_removed_directory_is_found(tmp_path, monkeypatch):
    (tmp_path / "include/dingo/core/base").mkdir(parents=True)
    (tmp_path / "test/next/api").mkdir(parents=True)
    monkeypatch.setattr(sys.modules[__name__], "BANNED_DIRECTORIES", ("core", "api"))
    found = banned_directories(tmp_path)
    assert {(str(path), name) for path, line, name, place in found} == {
        ("include/dingo/core", "core"),
        ("test/next/api", "api"),
    }


def test_a_missing_directory_is_skipped(tmp_path):
    assert violations(tmp_path, ("spec",)) == []
    assert banned_directories(tmp_path) == []


def test_a_message_names_the_file_the_line_and_the_word():
    message = describe([(Path("spec/x.py"), 3, "layer", "identifier")])
    assert message == "spec/x.py:3: 'layer' (identifier)"
