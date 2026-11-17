"""Verifies that the names of the library and of its specification have no
connecting words: a name describes what a thing is or does, not how it relates
to another, as of, with or to would.

The names of the C++ headers are every identifier of their code, without
comments and strings, but those of the standard library, which the code
reaches as std::name. The names of the Python specification are those its
code binds: functions, classes, parameters, assigned names and attributes,
and the fields of its classes; a test is named as the sentence it checks.

A name fails when one of its words is a connecting word, or when it is one
alone. KEPT lists the names kept, each with why.

usage: names.py [--include DIR] [--spec DIR]
"""

import argparse
import ast
import pathlib
import re
import sys

CONNECTING = frozenset({
    "of", "with", "or", "and", "at", "as", "by", "to", "from", "in", "into",
    "onto", "for", "on", "if", "per", "via", "then",
})

# Names kept with a connecting word, and why.
KEPT = {
    "for_each": "the name of the standard algorithm",
    "in_place_call": "std::in_place, which it is constructed of",
    "is_in_place_call_v": "std::in_place, which it is constructed of",
    "constructs_in_place": "std::in_place construction",
    "lookup_key_out_of_range_exception":
        "public and documented; std::out_of_range",
    "OUT_OF_MEMORY": "what a compiler that runs out of memory reports",
    "out_of_memory": "what a compiler that runs out of memory reports",
}

# Operators C++ spells as words, and Python keywords, which are no names.
CPP_WORDS = frozenset({"and", "or", "not", "for", "if"})


def words(name):
    """The words of a name: its parts between underscores, and the parts of
    each by its capitals."""
    parts = []
    for part in name.strip("_").split("_"):
        parts += re.findall(r"[A-Z]+(?![a-z])|[A-Z]?[a-z]+|\d+", part) or [part]
    return [part.lower() for part in parts if part]


def connecting(name):
    """Whether a name has a connecting word, or is one alone."""
    if name in KEPT:
        return False
    return any(word in CONNECTING for word in words(name))


def code_of(text):
    """C++ code without its comments and strings, of the same lines."""
    def blank(match):
        return re.sub(r"[^\n]", " ", match.group(0))
    text = re.sub(r"//[^\n]*", blank, text)
    text = re.sub(r"/\*.*?\*/", blank, text, flags=re.S)
    text = re.sub(r"'(\\.|[^'\\\n])'", blank, text)
    return re.sub(r'"(\\.|[^"\\\n])*"', blank, text)


def cpp_names(root):
    """The names of the headers under root that fail, with where."""
    files = sorted(root.rglob("*.h"))
    codes = {path: code_of(path.read_text()) for path in files}
    external = set()
    for code in codes.values():
        external.update(re.findall(r"\bstd::(\w+)", code))
        # A member a standard type defines, called on an object of it.
        external.update(re.findall(r"(?:\.|->)\s*(at)\s*\(", code))
    failed = []
    for path, code in codes.items():
        for match in re.finditer(r"\b[A-Za-z_]\w*\b", code):
            name = match.group(0)
            if name in external or name in CPP_WORDS or not connecting(name):
                continue
            # The preprocessor's own words.
            line = code.count("\n", 0, match.start()) + 1
            failed.append((path, line, name))
    return failed


class Binder(ast.NodeVisitor):
    """The names a Python module binds."""

    def __init__(self):
        self.names = []

    def bind(self, name, node):
        if name:
            self.names.append((node.lineno, name))

    def visit_FunctionDef(self, node):
        if not node.name.startswith("test_"):
            self.bind(node.name, node)
        arguments = node.args
        for argument in (arguments.posonlyargs + arguments.args +
                         arguments.kwonlyargs):
            self.bind(argument.arg, argument)
        for argument in (arguments.vararg, arguments.kwarg):
            if argument:
                self.bind(argument.arg, argument)
        self.generic_visit(node)

    visit_AsyncFunctionDef = visit_FunctionDef

    def visit_ClassDef(self, node):
        self.bind(node.name, node)
        self.generic_visit(node)

    def visit_Name(self, node):
        if isinstance(node.ctx, ast.Store):
            self.bind(node.id, node)

    def visit_Attribute(self, node):
        if isinstance(node.ctx, ast.Store):
            self.bind(node.attr, node)
        self.generic_visit(node)

    def visit_alias(self, node):
        if node.asname:
            self.bind(node.asname, node)


def python_names(root):
    """The names the Python modules under root bind that fail."""
    failed = []
    for path in sorted(root.rglob("*.py")):
        binder = Binder()
        binder.visit(ast.parse(path.read_text(), str(path)))
        for line, name in sorted(set(binder.names)):
            if connecting(name):
                failed.append((path, line, name))
    return failed


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--include", type=pathlib.Path,
                        default=pathlib.Path("include"))
    parser.add_argument("--spec", type=pathlib.Path,
                        default=pathlib.Path("spec"))
    options = parser.parse_args()
    failed = cpp_names(options.include) + python_names(options.spec)
    seen = set()
    for path, line, name in failed:
        if (path, name) in seen:
            continue
        seen.add((path, name))
        print(f"{path}:{line}: {name} has a connecting word")
    if failed:
        print(f"{len(seen)} names have connecting words; name what a thing "
              "is or does, or keep the name in tools/names.py with why",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
