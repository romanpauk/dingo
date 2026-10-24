"""Fixed limits in the library.

Everything the library sizes is known when it is compiled, so what it holds is
as large as what it is made of: an array has the elements it has, found by
counting them and filling an array of that many, or by a room given as an exact
template argument. A constant that caps a count, an array sized by a literal or
by such a constant, a count in a type too narrow for it, and a call that aborts
where a room ends are fixed limits, and each is a place the library stops
working for a type or a binding that is larger.

A new one fails the test, with the file, the line, and the kind. The ones that
remain are in LIMITS with the reason, and the list only shrinks: a limit that is
removed must be removed from it too.
"""

import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
SCANNED = ("include/dingo",)
SUFFIXES = frozenset({".h", ".hpp"})

# (kind, pattern): a line of the library that matches is a fixed limit.
PATTERNS = (
    (
        "a constant that caps a count",
        re.compile(
            r"\b(?:max|largest|smallest)_\w+\s*=\s*[0-9]"
            r"|\b\w*capacity\s*=\s*[1-9]"
        ),
    ),
    (
        "an array sized by a literal or by a limit",
        re.compile(
            r"(?<!return)(?<!else)(?<!case)\s[A-Za-z_][\w:<>,]*(?:\s*[*&]+\s*|\s+)\w+"
            r"\[\s*(?:[1-9][0-9]*|\w*(?:max|capacity|limit)\w*)\s*\]"
            r"\s*(?:=|;|\{)"
        ),
    ),
    (
        "a count in a type that holds at most 65535",
        re.compile(r"std::uint(?:8|16)_t\s+(?:\w*_)?(?:arity|argument|count|size|index)\b"),
    ),
    ("an abort where a room ends", re.compile(r"\bstd::abort\s*\(")),
)

# The limits that remain: path, the line as written, and why it is still there.
LIMITS = {
    ("include/dingo/conversion/route.h", "inline constexpr std::size_t largest_set_room = 1024;"):
        "the semantics are collected with a recursion that stops; a constant says when it fits",
    ("include/dingo/conversion/route.h", "std::abort();"):
        "the stops of the two recursions above",
    ("include/dingo/conversion/route.h", "inline constexpr std::size_t largest_derivation_room = 1024;"):
        "the derivation recurses on a runtime flag; a constant says when it fits",
    ("include/dingo/conversion/route.h", "inline constexpr std::size_t max_created_wrappers = 16;"):
        "the plan of a request keeps its forms in a word; keep them per request type",
    ("include/dingo/conversion/route.h", "route_step created[max_created_wrappers] = {};"):
        "sized by the request",
    ("include/dingo/conversion/route.h", "route_step chain[max_created_wrappers] = {};"):
        "sized by the request",
    ("include/dingo/conversion/route.h", "std::uint8_t argument = 0;"):
        "the alternative or interface a rule reached, as wide as a count",
    ("include/dingo/lookup/declarations.h", "static constexpr std::size_t capacity = 32;"):
        "the declarations of the lookups of a container are counted",
    ("include/dingo/lookup/declarations.h", "lookup_descriptor items[capacity] = {};"):
        "the declarations of the lookups of a container are counted",
    ("include/dingo/selection/selection_rules.gen.h", "inline constexpr std::size_t max_selection_steps = 5;"):
        "generated from the spec, which has the counts",
    ("include/dingo/selection/selection_rules.gen.h", "inline constexpr std::size_t max_step_sources = 2;"):
        "generated from the spec, which has the counts",
    ("include/dingo/selection/selection_rules.gen.h", "selection_source sources[max_step_sources];"):
        "generated from the spec, which has the counts",
    ("include/dingo/selection/selection_rules.gen.h", "selection_step steps[max_selection_steps];"):
        "generated from the spec, which has the counts",
    ("include/dingo/type/type_tree.h", "inline constexpr std::size_t type_tree_capacity = 16;"):
        "the type tree of tests and of type_tree_v; no type of the library is limited by it",
    ("include/dingo/type/type_tree.h", "inline constexpr std::size_t max_semantics_operations = 32;"):
        "a wrapper semantics keeps a target for each operation it has",
    ("include/dingo/type/type_tree.h", "const semantics_descriptor *targets[max_semantics_operations] = {};"):
        "a wrapper semantics keeps a target for each operation it has",
    ("include/dingo/type/type_tree.h", "std::uint8_t arity = 0;"):
        "the arguments of a wrapper, as wide as a count",
}


def hits():
    """(path, stripped line, kind) of every fixed limit in the scanned paths."""
    found = []
    for scanned in SCANNED:
        for path in sorted((REPO_ROOT / scanned).rglob("*")):
            if path.suffix not in SUFFIXES:
                continue
            relative = path.relative_to(REPO_ROOT).as_posix()
            for line in path.read_text().splitlines():
                text = line.strip()
                if text.startswith("//"):
                    continue
                for kind, pattern in PATTERNS:
                    if pattern.search(line):
                        found.append((relative, text, kind))
    return found


def test_the_library_has_no_fixed_limit_that_is_not_listed():
    unexpected = [
        f"{path}: {kind}: {text}"
        for path, text, kind in hits()
        if (path, text) not in LIMITS
    ]
    assert not unexpected, "\n".join(
        ["a fixed limit is a size the library cannot grow; size it from what it holds:", *unexpected]
    )


def test_a_listed_limit_is_still_in_the_library():
    present = {(path, text) for path, text, _ in hits()}
    stale = [f"{path}: {text}" for path, text in LIMITS if (path, text) not in present]
    assert not stale, "\n".join(["a limit that is gone is removed from LIMITS:", *stale])
