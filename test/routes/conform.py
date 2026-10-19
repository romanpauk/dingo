#!/usr/bin/env python3
"""Compares the observed route behavior with the reference implementation.

test/routes/extract.py records what the current library does for every scope,
stored shape, and request in the runtime, static, and mixed containers. This
script maps those C++ spellings to the abstract shapes of the spec, computes
the outcome of every case with the reference implementation in
spec/reference/routes.py, and compares them:

- registration: whether the binding can be registered
- served: whether the request is served. type_not_found_exception means the
  request does not name the binding, type_not_convertible_exception that it
  names it but is not served, and a compile error matches either. A crash or
  another exception never matches.
- copies, constructions on the first and on a repeated request, the identity of
  the leaf of the result on both, and whether a repeated request yields the
  same leaf
- moves and temporaries: a case must not make more moves or destroy more
  temporaries than the same case in the container that makes the fewest

Every difference must be declared by a behavior change of the spec whose
`cases` match `routes:<case id>#<field>`, and every behavior change with a
`routes:` pattern must match a difference. The exit status is 0 when both hold, 1
otherwise.

The table is extracted into the build tree, not committed. Without --observed,
the single table under build/routes/observed is used.

With --exact, the table is of the next implementation, which implements the
reference without behavior changes: every difference fails, grouped by pattern.

Usage:
    conform.py [--observed FILE] [--report FILE] [--summary] [--exact]
"""

import argparse
import collections
import fnmatch
import json
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
# Importing the spec writes no bytecode into the source tree.
sys.dont_write_bytecode = True
sys.path.insert(0, str(PROJECT_ROOT))

import spec  # noqa: E402
from spec.reference.routes import (  # noqa: E402
    Leaf,
    Model,
    Request,
    RequestKind,
    Shape,
    resolve,
)

OBSERVED_DIR = PROJECT_ROOT / "build" / "routes" / "observed"


def default_observed():
    """The single extracted table under build/routes/observed, or None."""
    tables = sorted(OBSERVED_DIR.glob("*.jsonl")) if OBSERVED_DIR.is_dir() else []
    return tables[0] if len(tables) == 1 else None

# The C++ test types of the extractor and their properties.
LEAVES = {
    "leaf": Leaf("leaf"),
    "move_only_leaf": Leaf("move_only_leaf", copyable=False),
    "other_leaf": Leaf("other_leaf"),
    "impl": Leaf("impl", bases=("iface", "other_iface")),
    "iface": Leaf("iface", copyable=False, constructible=False),
    "other_iface": Leaf("other_iface", copyable=False, constructible=False),
}

# The wrappers the extractor uses, with the layer kind and the wrapper family
# they map to through the wrapper-traits extension point.
WRAPPERS = {
    "std::unique_ptr": ("own", "std"),
    "std::shared_ptr": ("share", "std"),
    "std::optional": ("opt", "std"),
    "std::variant": ("alt", "std"),
    "user_unique": ("own", "user"),
    "user_shared": ("share", "user"),
    "user_optional": ("opt", "user"),
}


# C++ spellings


def split_arguments(text):
    depth, start, parts = 0, 0, []
    for index, char in enumerate(text):
        if char == "<":
            depth += 1
        elif char == ">":
            depth -= 1
        elif char == "," and depth == 0:
            parts.append(text[start:index].strip())
            start = index + 1
    parts.append(text[start:].strip())
    return parts


def parse_type(text):
    """Parses a C++ type spelling into (request kind or None, Shape)."""
    text = text.strip()
    if text.startswith("const "):
        text = text[len("const ") :]
    # Base type: an identifier with optional template arguments.
    index = 0
    while index < len(text) and (text[index].isalnum() or text[index] in "_:"):
        index += 1
    name = text[:index]
    if index < len(text) and text[index] == "<":
        depth, end = 0, index
        for end in range(index, len(text)):
            if text[end] == "<":
                depth += 1
            elif text[end] == ">":
                depth -= 1
                if depth == 0:
                    break
        arguments = [parse_shape(arg) for arg in split_arguments(text[index + 1 : end])]
        kind, family = WRAPPERS[name]
        shape = Shape(layer=kind, args=tuple(arguments), family=family)
        rest = text[end + 1 :]
    else:
        shape = Shape(leaf=LEAVES[name])
        rest = text[index:]
    return parse_declarator(shape, rest.replace(" ", ""))


def parse_declarator(shape, rest):
    kind = None
    while rest:
        if rest.startswith("const"):
            rest = rest[len("const") :]
        elif rest.startswith("&&"):
            kind, rest = RequestKind.RVALUE_REFERENCE, rest[2:]
        elif rest.startswith("&"):
            kind, rest = RequestKind.LVALUE_REFERENCE, rest[1:]
        elif rest.startswith("*"):
            shape, rest = Shape(layer="ptr", args=(shape,)), rest[1:]
        elif rest.startswith("[]"):
            shape, rest = Shape(layer="array_unbounded", args=(shape,)), rest[2:]
        elif rest.startswith("["):
            end = rest.index("]")
            shape = Shape(layer="array", args=(shape,), extent=int(rest[1:end]))
            rest = rest[end + 1 :]
        elif rest.startswith("("):
            end = rest.index(")")
            inner, after = rest[1:end], rest[end + 1 :]
            close = after.index("]")
            shape = Shape(layer="array", args=(shape,), extent=int(after[1:close]))
            rest = inner + after[close + 1 :]
        else:
            raise ValueError(f"cannot parse declarator {rest!r}")
    return kind, shape


def parse_shape(text):
    kind, shape = parse_type(text)
    if kind == RequestKind.LVALUE_REFERENCE:
        return Shape(layer="ref", args=(shape,))
    if kind is not None:
        raise ValueError(f"{text!r} is not a storable shape")
    return shape


def parse_request(text):
    kind, shape = parse_type(text)
    return Request(kind or RequestKind.VALUE, shape)


# Comparison


def observed_outcome(row):
    registration = row["registration"] == "ok"
    resolve_text = row["resolve"]
    if not registration:
        served = None
    elif resolve_text == "ok":
        served = True
    elif resolve_text == "throws:type_not_convertible_exception":
        served = False
    elif resolve_text == "throws:type_not_found_exception":
        served = "not_found"
    elif resolve_text == "compile_error":
        served = "unserved"
    else:
        served = resolve_text  # a crash or an unexpected exception
    return registration, served


def expected_served(outcome):
    if outcome.served:
        return True
    return False if outcome.found else "not_found"


def same_service(observed, expected):
    if observed == "unserved":
        return expected in (False, "not_found")
    return observed == expected


def expected_fields(outcome):
    return {
        "copies": outcome.copies,
        "constructions": list(outcome.constructions),
        "identity": list(outcome.identity),
        "stable": outcome.stable,
    }


def observed_fields(row):
    counts, warm = row["counts"], row["warm_counts"]
    return {
        "copies": counts["copy"],
        "constructions": [counts["default"], warm["default"]],
        "identity": [row["identity"], row["warm_identity"]],
        "stable": bool(row["stable"]),
    }


def compare(rows, model):
    differences = []
    served_counts = collections.defaultdict(dict)
    for row in rows:
        stored = parse_shape(row["stored"])
        request = parse_request(row["request"])
        interfaces = tuple(
            LEAVES[name] for name in row.get("interfaces", "").split(",") if name
        )
        outcome = resolve(model, row["scope"], stored, request, interfaces)
        registered, served = observed_outcome(row)
        case = row["id"]
        if registered != outcome.registered:
            differences.append((case, "registration", registered, outcome.registered))
            continue
        if not registered:
            continue
        expected = expected_served(outcome)
        if not same_service(served, expected):
            differences.append((case, "served", served, expected))
            continue
        if served is not True:
            continue
        want, have = expected_fields(outcome), observed_fields(row)
        for name in want:
            if want[name] != have[name]:
                differences.append((case, name, have[name], want[name]))
        key = tuple(case.split("/", 1)[1].split("/"))
        served_counts[key][row["domain"]] = (row["counts"]["move"], row["counts"]["destroy"])
    for key, by_domain in served_counts.items():
        for index, name in enumerate(("moves", "temporaries")):
            least = min(counts[index] for counts in by_domain.values())
            for domain, counts in sorted(by_domain.items()):
                if counts[index] > least:
                    case = "/".join((domain, *key))
                    differences.append((case, name, counts[index], least))
    return sorted(differences, key=lambda d: (d[0], d[1]))


def pattern_of(difference):
    case, name, have, want = difference
    domain, leaf, scope, stored, request = case.split("/", 4)
    kind = "interfaces" if "[" in leaf else "user" if "user_" in stored else "std"
    return (f"{scope} {kind}", name, json.dumps(have), json.dumps(want))


def main():
    return main_with(sys.argv[1:])


def main_with(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--observed", type=Path, default=None)
    parser.add_argument("--report", type=Path)
    parser.add_argument(
        "--summary", action="store_true", help="group the differences by pattern"
    )
    parser.add_argument(
        "--exact",
        action="store_true",
        help="no behavior change applies: the table is of the next implementation",
    )
    args = parser.parse_args(argv)

    observed = args.observed or default_observed()
    if observed is None:
        print(
            f"no single extracted table under {OBSERVED_DIR}; run "
            "test/routes/extract.py --work-dir build/routes, or pass --observed",
            file=sys.stderr,
        )
        return 2

    loaded = spec.load_spec()
    model = Model(loaded)
    rows = [json.loads(line) for line in observed.read_text().splitlines()]
    differences = compare(rows, model)

    changes = (
        []
        if args.exact
        else [
            (change, [p for p in change.cases if p.startswith("routes:")])
            for change in loaded.behavior_changes
        ]
    )
    matched = collections.Counter()
    undeclared = []
    for difference in differences:
        path = f"routes:{difference[0]}#{difference[1]}"
        owners = [c.id for c, patterns in changes if any(fnmatch.fnmatchcase(path, p) for p in patterns)]
        for owner in owners:
            matched[owner] += 1
        if not owners:
            undeclared.append(difference)
    stale = [c.id for c, patterns in changes if patterns and not matched[c.id]]

    print(f"compared {len(rows)} cases: {len(differences)} differences, "
          f"{len(differences) - len(undeclared)} declared")
    for change_id, count in sorted(matched.items()):
        print(f"  {change_id}: {count}")
    if args.summary:
        groups = collections.defaultdict(list)
        for difference in undeclared:
            groups[pattern_of(difference)].append(difference[0])
        for (scope, name, have, want), cases in sorted(groups.items(), key=lambda g: -len(g[1])):
            print(f"{len(cases):5} {scope} {name}: observed {have}, reference {want}")
            for case in cases[:3]:
                print(f"        {case}")
    else:
        for case, name, have, want in undeclared[:200]:
            print(f"undeclared: {case}: {name}: observed {have}, reference {want}")
        if len(undeclared) > 200:
            print(f"... {len(undeclared) - 200} more")
    for change_id in stale:
        print(f"stale behavior change: {change_id}")
    if args.report:
        args.report.write_text(json.dumps(
            [dict(zip(("case", "field", "observed", "reference"), d)) for d in differences],
            indent=1,
        ) + "\n")
    return 1 if undeclared or stale else 0


if __name__ == "__main__":
    sys.exit(main())
