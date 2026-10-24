#!/usr/bin/env python3
"""Compares the observed selection by key with the reference implementation.

test/selection/extract.py records what the current library does for every
container domain, list of keyed bindings, declaration setting, and request.
This script maps every case to the model of spec/reference/selection.py, a
level of bindings with its lookup declarations and its parent, computes the
outcome with the reference, and compares:

- registration: whether each binding is registered. A
  lookup_already_registered_exception is a rejection; a compile error of the
  registrations, or a combination the library cannot spell
  ("not_expressible"), matches a reference that rejects one of them.
- selection: the implementation a singular request selects, or the ordered
  implementations of a collection. type_ambiguous_exception is an ambiguous
  request, type_not_found_exception a request that names no binding, and a
  compile error matches either. A crash or another exception never matches.
  A difference of a collection the reference finds empty is reported as the
  field empty_collection, of any other collection as collection, and a
  registration the library accepts where the reference rejects a duplicate
  as the field duplicate.

The selection is compared only when the registration matches.

When an outcome is reported is compared too, for a case the library can
spell: a compile error of the registrations must be a rejection the reference
decides when the container is compiled, a static binding rejected by a static one, and a rejection it
decides so must be a compile error; a failed selection is a compile error if
and only if the reference decides it when the request is compiled. A
difference of either is reported as the field registration_time or
selection_time. A static container takes no runtime registration; the mixed,
runtime, and parent ones do.

Mapping: the bindings of a static container and the static bindings of a
mixed one have static=True; a parent case has a child level whose parent
level holds the parent bindings; the many setting declares every key domain
of the universe many in Level.declarations (on both levels of a parent case);
the default setting declares nothing, which the spec treats as cardinality
one. The library of the baseline cannot declare lookups in a mixed container,
so its many setting there is "not_expressible", a difference the spec
declares.

Every difference must be declared by a behavior change of the spec whose
`cases` match `selection:<case id>#<field>`, and every behavior change with a
`selection:` pattern must match a difference. The exit status is 0 when both
hold, 1 otherwise. With --summary the undeclared differences are grouped by
pattern with a few examples each.

Usage:
    conform.py [--observed FILE] [--report FILE] [--summary] [--exact]
"""

import argparse
import collections
import fnmatch
import importlib.util
import json
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
# Importing the spec writes no bytecode into the source tree.
sys.dont_write_bytecode = True
sys.path.insert(0, str(PROJECT_ROOT))
sys.path.insert(0, str(Path(__file__).resolve().parent))

import spec  # noqa: E402
# Loaded under its own name, so it does not clash with test/routes/extract.py.
_extract_spec = importlib.util.spec_from_file_location(
    "selection_extract", Path(__file__).resolve().parent / "extract.py"
)
selection_extract = importlib.util.module_from_spec(_extract_spec)
sys.modules["selection_extract"] = selection_extract
_extract_spec.loader.exec_module(selection_extract)
bindings_label = selection_extract.bindings_label
parse_case_id = selection_extract.parse_case_id
from spec.reference.selection import (  # noqa: E402
    KEY_OUT_OF_RANGE,
    NO_KEY,
    Binding,
    Key,
    Level,
    Model,
    Request,
)
from spec.schema import Cardinality, Status  # noqa: E402

OBSERVED_DIR = PROJECT_ROOT / "build" / "selection" / "observed"
INTERFACE = "iface"

# The keys of case ids as keys of the reference.
KEYS = {
    "none": NO_KEY,
    "tag_a": Key("tag", "tag_a"),
    "tag_b": Key("tag", "tag_b"),
    "fixed7": Key("fixed", "int", 7),
    "fixed8": Key("fixed", "int", 8),
    "runtime7": Key("runtime", "int", 7),
    "runtime8": Key("runtime", "int", 8),
}

# The many setting declares every key domain of the universe many.
MANY_DOMAINS = sorted({key.domain() for key in KEYS.values()}, key=str)


def default_observed():
    """The single extracted table under build/selection/observed, or None."""
    tables = sorted(OBSERVED_DIR.glob("*.jsonl")) if OBSERVED_DIR.is_dir() else []
    return tables[0] if len(tables) == 1 else None


def impl_index(name):
    return int(name.rsplit("_", 1)[1])


def declarations(domain, setting):
    if setting == "many":
        return {(INTERFACE, d): Cardinality.MANY for d in MANY_DOMAINS}
    return {}


# The bound of the dense array of the int keys of the bounded setting.
BOUND = 8


def bounds(setting):
    if setting == "bounded":
        return {(INTERFACE, ("value", "int")): BOUND}
    return {}


def to_reference(case):
    """The reference of a case: (the level asked, request, mode, {binding:
    implementation index}, [(level, Binding)] in binding order). The request
    of iface, an abstract interface, has no factory."""
    domain, setting, bindings, mode, key = parse_case_id(case)
    static = domain in ("static", "static_parent")
    child = Level(
        declarations=declarations(domain, setting),
        bounds=bounds(setting),
        open=not static,
    )
    parent = None
    if domain in ("parent", "static_parent"):
        parent = Level(
            declarations=declarations(domain, setting),
            bounds=bounds(setting),
            open=not static,
        )
        child.parent = parent
    placed = []
    impls = {}
    for position, binding in enumerate(bindings):
        reference = Binding(
            f"b{position}",
            (INTERFACE,),
            KEYS[binding.key],
            static=static or binding.place == "static",
        )
        impls[reference] = impl_index(binding.impl)
        placed.append((parent if binding.place == "parent" else child, reference))
    request = Request(INTERFACE, KEYS[key])
    return child, request, mode, impls, placed


def expected(model, case):
    """(registrations, selection, times) of the reference: the registrations
    as "ok", "rejected", or "out_of_range" in binding order; the selection as
    an implementation index, a list of them, "ambiguous", or "not_found"; and
    whether a rejection and the selection are decided when compiled, as
    {"registration": bool, "selection": bool}."""
    level, request, mode, impls, placed = to_reference(case)
    collection = mode == "all"
    registrations = []
    compiled_rejection = False
    for target, binding in placed:
        _, compiled = model.rejection(target, binding)
        ok, reason = model.register(target, binding)
        compiled_rejection = compiled_rejection or (not ok and compiled)
        registrations.append(
            "ok" if ok else "out_of_range" if reason == KEY_OUT_OF_RANGE else "rejected"
        )
    selection = model.select(level, request, collection=collection, construct=mode == "make")
    times = {"registration": compiled_rejection, "selection": selection.compiled}
    if selection.status is Status.AMBIGUOUS:
        return registrations, "ambiguous", times
    if selection.status is Status.NOT_FOUND:
        return registrations, "not_found", times
    indices = [impls[b] for b in selection.bindings]
    return registrations, indices if collection else indices[0], times


def observed_registrations(row):
    if row["registration"] in ("compile_error", "not_expressible"):
        return row["registration"]
    return [
        "ok"
        if outcome == "ok"
        else "rejected"
        if outcome == "throws:lookup_already_registered_exception"
        else "out_of_range"
        if outcome == "throws:type_index_out_of_range_exception"
        else outcome
        for outcome in row["registrations"]
    ]


def observed_selection(row):
    selection = row["selection"]
    if selection == "ok":
        return row["selected"]
    return {
        "throws:type_ambiguous_exception": "ambiguous",
        "throws:type_not_found_exception": "not_found",
    }.get(selection, selection)


def same_registrations(observed, reference):
    if observed in ("compile_error", "not_expressible"):
        return "rejected" in reference or "out_of_range" in reference
    return observed == reference


def same_selection(observed, reference):
    if observed == "compile_error":
        return reference in ("ambiguous", "not_found")
    return observed == reference


def compare(rows, model):
    differences = []
    for row in rows:
        case = row["id"]
        want_registrations, want_selection, times = expected(model, case)
        have_registrations = observed_registrations(row)
        if not same_registrations(have_registrations, want_registrations):
            duplicate = isinstance(have_registrations, list) and all(
                outcome == "ok" for outcome in have_registrations
            )
            # The reference rejects every binding the registrations differ
            # in for a key outside the bound of its domain.
            out_of_range = isinstance(have_registrations, list) and all(
                want == "out_of_range"
                for have, want in zip(have_registrations, want_registrations)
                if have != want
            )
            field = (
                "duplicate"
                if duplicate
                else "out_of_range"
                if out_of_range
                else "registration"
            )
            differences.append((case, field, have_registrations, want_registrations))
            continue
        if have_registrations == "not_expressible":
            continue
        compiled = have_registrations == "compile_error"
        if compiled != times["registration"]:
            differences.append(
                (case, "registration_time", have_registrations, reported(times["registration"]))
            )
            continue
        if compiled:
            continue
        have_selection = observed_selection(row)
        if not same_selection(have_selection, want_selection):
            field = (
                "empty_collection"
                if want_selection == []
                else "collection"
                if isinstance(want_selection, list)
                else "selection"
            )
            differences.append((case, field, have_selection, want_selection))
        elif want_selection in ("ambiguous", "not_found") and (
            have_selection == "compile_error"
        ) != times["selection"]:
            differences.append(
                (case, "selection_time", have_selection, reported(times["selection"]))
            )
    return sorted(differences, key=lambda d: (d[0], d[1]))


def reported(compiled):
    return "when compiled" if compiled else "when resolved"


def outcome_class(value, name="selection"):
    """A registration outcome as "ok" or "rejects", a selection outcome
    without its implementation indices."""
    if isinstance(value, list) and name == "registration":
        failed = [v for v in value if v != "ok"]
        return "ok" if not failed else "rejects" if failed == ["rejected"] * len(failed) else json.dumps(value)
    if isinstance(value, int) and not isinstance(value, bool):
        return "one"
    if isinstance(value, list) and all(isinstance(v, int) for v in value):
        return f"list[{len(value)}]"
    return json.dumps(value)


def named_places(case, model):
    """The places of the bindings the reference registers and the request of
    a case names, in binding order."""
    domain, _, bindings, _, key = parse_case_id(case)
    _, _, _, _, placed = to_reference(case)
    places = []
    for (target, binding), observed in zip(placed, bindings):
        if model.register(target, binding)[0] and binding.key.names(KEYS[key]):
            places.append(observed.place)
    return places


def place_label(domain, binding):
    kind = KEYS[binding.key].kind
    return f"{kind}@{binding.place}" if domain in ("mixed", "parent", "static_parent") else kind


class Patterns:
    """Groups differences by their likely cause.

    A registration difference of two bindings is attributed to one of them
    when that binding alone shows the same observed registration; otherwise
    it is grouped by the key kinds and places of both and whether their keys
    name each other. A selection difference is grouped by the request mode
    and key kind and the places of the bindings it names in the reference."""

    def __init__(self, rows, model):
        self.rows = {row["id"]: row for row in rows}
        self.model = model

    def registration(self, case, have):
        domain, setting, bindings, mode, key = parse_case_id(case)
        if domain == "mixed" and setting != "default":
            return f"{setting} setting"
        labels = [place_label(domain, b) for b in bindings]
        if have in ("compile_error", "not_expressible"):
            for binding, label in zip(bindings, labels):
                alone = (binding.__class__(bindings[0].impl, binding.key, binding.place),)
                row = self.rows.get(
                    f"{domain}/{setting}/{bindings_label(domain, alone)}/{mode}:{key}"
                )
                if row and observed_registrations(row) == have:
                    return f"{label} alone"
        if len(bindings) == 2:
            first, second = (KEYS[b.key] for b in bindings)
            relation = (
                "same key"
                if first.names(second)
                else "same domain"
                if first.domain() == second.domain()
                else "other domains"
            )
            if bindings[0].impl == bindings[1].impl:
                relation += ", one implementation"
            return f"{'+'.join(labels)} {relation}"
        return "+".join(labels) or "empty"

    def selection(self, case):
        domain, _, _, mode, key = parse_case_id(case)
        places = named_places(case, self.model)
        named = "+".join(places) if domain in ("mixed", "parent", "static_parent") else str(len(places))
        return f"{mode}:{KEYS[key].kind} names {named or 'none'}"

    def __call__(self, difference):
        case, name, have, want = difference
        domain, setting, *_ = parse_case_id(case)
        feature = (
            self.registration(case, have)
            if name == "registration"
            else self.selection(case)
        )
        observed, reference = outcome_class(have, name), outcome_class(want, name)
        if observed == reference and observed.startswith("list"):
            observed += " reordered" if sorted(have) == sorted(want) else " other"
        return (f"{domain}/{setting}", name, observed, reference, feature)


def examples(members, count):
    """Up to count members of a group, of distinct container configurations
    where there are enough."""
    chosen, seen = [], set()
    for member in members:
        configuration = member[0].rsplit("/", 1)[0]
        if configuration not in seen:
            seen.add(configuration)
            chosen.append(member)
    chosen += [m for m in members if m not in chosen]
    return chosen[:count]


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
        "--coarse",
        action="store_true",
        help="with --summary, group without the cause",
    )
    parser.add_argument(
        "--examples", type=int, default=2, help="examples per pattern with --summary"
    )
    parser.add_argument(
        "--exact",
        action="store_true",
        help="no behavior change applies: every difference fails",
    )
    args = parser.parse_args(argv)

    observed = args.observed or default_observed()
    if observed is None:
        print(
            f"no single extracted table under {OBSERVED_DIR}; run "
            "test/selection/extract.py --work-dir build/selection, or pass --observed",
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
            (change, [p for p in change.cases if p.startswith("selection:")])
            for change in loaded.behavior_changes
        ]
    )
    matched = collections.Counter()
    undeclared = []
    for difference in differences:
        path = f"selection:{difference[0]}#{difference[1]}"
        owners = [
            c.id
            for c, patterns in changes
            if any(fnmatch.fnmatchcase(path, p) for p in patterns)
        ]
        for owner in owners:
            matched[owner] += 1
        if not owners:
            undeclared.append(difference)
    stale = [c.id for c, patterns in changes if patterns and not matched[c.id]]

    print(
        f"compared {len(rows)} cases: {len(differences)} differences, "
        f"{len(differences) - len(undeclared)} declared"
    )
    for change_id, count in sorted(matched.items()):
        print(f"  {change_id}: {count}")
    if args.summary:
        groups = collections.defaultdict(list)
        pattern_of = Patterns(rows, model)
        for difference in undeclared:
            pattern = pattern_of(difference)
            groups[pattern[:4] if args.coarse else pattern].append(difference)
        for pattern, members in sorted(groups.items(), key=lambda g: (-len(g[1]), g[0])):
            setting, name, have, want = pattern[:4]
            type_tree = f" {pattern[4]}" if len(pattern) > 4 else ""
            print(f"{len(members):5} {setting}{type_tree} {name}: observed {have}, reference {want}")
            for case, _, have_value, want_value in examples(members, args.examples):
                print(
                    f"        {case}: observed {json.dumps(have_value)}, "
                    f"reference {json.dumps(want_value)}"
                )
    else:
        for case, name, have, want in undeclared[:200]:
            print(
                f"undeclared: {case}: {name}: observed {json.dumps(have)}, "
                f"reference {json.dumps(want)}"
            )
        if len(undeclared) > 200:
            print(f"... {len(undeclared) - 200} more")
    for change_id in stale:
        print(f"stale behavior change: {change_id}")
    if args.report:
        args.report.write_text(
            json.dumps(
                [
                    dict(zip(("case", "field", "observed", "reference"), d))
                    for d in differences
                ],
                indent=1,
            )
            + "\n"
        )
    return 1 if undeclared or stale else 0


if __name__ == "__main__":
    sys.exit(main())
