#!/usr/bin/env python3
"""Reads the routes the reference implementation selects, and says whether they
have the normal form that docs/architecture/route-normal-form.md proposes.

The operations of a route are of four stages: peel (borrow, take, decay,
release), a rule of a leaf (copy, upcast_reference, convert_borrowed,
convert_consumed), an upcast under a kept wrapper (upcast_copied,
upcast_consumed, retain), and build (wrap, adopt, address, share,
release_whole). Every served route of the cases that conversion_cases.py
enumerates is classified by the word its stages spell, and the words that are
not peel, then at most one rule of a leaf and at most one upcast, then build are
reported.

    normal_form.py                 the wrappers of the library
    normal_form.py --user-kinds    and the wrapper semantics of the user of the
                                   route probes, with their mutations
    normal_form.py --thunks        the thunks a binding has for each reachable
                                   type, and one for each type tree node and
                                   each interface or none, for typical bindings

It reads and prints, and changes nothing.
"""

import argparse
import collections
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT), str(ROOT / "test" / "next"), str(Path(__file__).resolve().parent)]

import conversion_cases as cases  # noqa: E402
import spec  # noqa: E402
from spec.reference.routes import (  # noqa: E402
    Model,
    Request,
    derivation_bound,
    derive,
    leaf,
    resolve,
    wrapper,
)

PEEL = {"borrow", "take", "decay", "release"}
LEAF_RULE = {"copy", "upcast_reference", "convert_borrowed", "convert_consumed"}
UPCAST = {"upcast_copied", "upcast_consumed", "retain"}
BUILD = {"wrap", "adopt", "address", "share", "release_whole"}
STAGES = (("P", PEEL), ("C", LEAF_RULE), ("U", UPCAST), ("B", BUILD))
# Peel, then a rule of a leaf and an upcast in either order, then build.
NORMAL_FORM = re.compile(r"P*(C?U?|U?C?)B*")


def stage_of(operation):
    for letter, operations in STAGES:
        if operation in operations:
            return letter
    raise KeyError(operation)


def operations_of(model, rules):
    return [
        model.rules_by_id[rule].operation.value
        if model.rules_by_id[rule].operation is not None
        else rule
        for rule in rules
    ]


def universe(singles, variadics=(), arrays=()):
    """The stored type trees and the request type trees over the wrappers of one
    argument `singles`, the variadic ones `variadics`, and the arrays `arrays`,
    each a pair of a semantics and a family. A stack holds at most one wrapper
    of the user."""

    def extras(inner):
        for semantics, family in variadics:
            yield wrapper(semantics, inner, leaf(cases.OTHER), family=family)
        for semantics, family in arrays:
            yield wrapper(semantics, inner, extent=2, family=family)

    def stored_trees(value):
        base = leaf(value)
        yield base
        yield wrapper("ptr", base)
        yield wrapper("ref", base)
        yield wrapper("array", base, extent=2)
        yield wrapper("alt", base, leaf(cases.OTHER), family="std")
        yield from extras(base)
        for outer, outer_family in singles:
            yield wrapper(outer, base, family=outer_family)
            if outer != "opt":
                yield wrapper(outer, wrapper("array_unbounded", base), family=outer_family)
            if outer in cases.WRAPPERS:
                for extra in extras(base):
                    yield wrapper(outer, extra, family=outer_family)
            for inner, inner_family in singles:
                if outer not in cases.WRAPPERS and inner not in cases.WRAPPERS:
                    continue
                yield wrapper(outer, wrapper(inner, base, family=inner_family), family=outer_family)

    def request_trees(stored, interfaces):
        value = list(cases.chain(stored))[-1]
        leaves = [value, *[leaf(interface) for interface in interfaces]]
        trees = set(cases.chain(stored))
        for target in leaves:
            trees.add(target)
            for semantics, family in singles:
                trees.add(wrapper(semantics, target, family=family))
            trees.update(extras(target))
        trees = {tree for tree in trees if tree.wrapper != "array_unbounded"}
        return sorted(
            trees | {wrapper("ptr", tree) for tree in trees if tree.wrapper != "ref"},
            key=repr,
        )

    return stored_trees, request_trees


def classify(model, stored_trees, request_trees, label):
    words = collections.Counter()
    outside = collections.Counter()
    example = {}
    requests = served = 0
    for scope in cases.SCOPES:
        for value, interfaces in cases.BINDING_LEAVES:
            for stored in stored_trees(value):
                for request in request_trees(stored, interfaces):
                    for kind in cases.KINDS:
                        if not cases.valid_request(request, kind):
                            continue
                        requests += 1
                        outcome = resolve(model, scope, stored, Request(kind, request), interfaces)
                        if not outcome.served:
                            continue
                        served += 1
                        operations = operations_of(model, outcome.rules)
                        word = "".join(stage_of(operation) for operation in operations)
                        words[word] += 1
                        if not NORMAL_FORM.fullmatch(word):
                            outside[tuple(operations)] += 1
                            example.setdefault(tuple(operations), (scope, stored, request))
    fitting = served - sum(outside.values())
    print(f"== {label}: {requests} requests, {served} served")
    print(f"   {fitting} fit the normal form ({100.0 * fitting / served:.2f}%), "
          f"{sum(outside.values())} do not, in {len(outside)} sequences")
    print(f"   {len(words)} distinct stage words, the longest of {max(map(len, words))} steps:")
    print("   " + ", ".join(f"{word or '-'}:{count}" for word, count in sorted(words.items())))
    for operations, count in outside.most_common():
        scope, stored, request = example[operations]
        print(f"   outside x{count}: {' > '.join(operations)}   "
              f"e.g. {scope}, stored {stored.wrapper or stored.leaf.name}<...>, "
              f"request {request.wrapper or request.leaf.name}<...>")


def split(kinds):
    singles, variadics, arrays = [], [], []
    for kind in kinds:
        if kind.variadic:
            variadics.append((kind.id, "user"))
        elif kind.has_extent:
            arrays.append((kind.id, "user"))
        elif kind.parameters == 1 and any(trait.value == "mapped" for trait in kind.traits):
            singles.append((kind.id, "user"))
    return singles, variadics, arrays


def nodes(tree):
    return 1 + sum(nodes(argument) for argument in tree.args)


def thunks(model):
    impl = leaf(cases.IMPL)
    interfaces = (cases.IFACE, cases.OTHER_IFACE)
    handle = wrapper("share", impl, family="std")
    unique = wrapper("own", impl, family="std")
    alternatives = wrapper(
        "alt", wrapper("share", impl, family="std"), wrapper("own", impl, family="std"),
        leaf(cases.OTHER), family="std",
    )
    bindings = {
        "shared impl, two interfaces": ("shared", impl, interfaces),
        "shared shared_ptr<impl>, two interfaces": ("shared", handle, interfaces),
        "unique unique_ptr<impl>, two interfaces": ("unique", unique, interfaces),
        "shared optional<shared_ptr<impl>>": ("shared", wrapper("opt", handle, family="std"), interfaces),
        "shared variant<impl, other>": ("shared", wrapper("alt", impl, leaf(cases.OTHER), family="std"), interfaces),
        "shared shared_ptr<variant<sp, up, other>>": ("shared", wrapper("share", alternatives, family="std"), interfaces),
    }
    print(f"{'binding':44}{'reachable types':>17}{'node x (interfaces + 1)':>25}")
    for label, (scope, stored, declared) in bindings.items():
        reachable = derive(model, scope, stored, declared, derivation_bound(stored))
        print(f"{label:44}{len(reachable):>17}{nodes(stored) * (1 + len(declared)):>25}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--user-kinds", action="store_true")
    parser.add_argument("--thunks", action="store_true")
    arguments = parser.parse_args()
    loaded = spec.load_spec()
    singles = [(semantics, family) for semantics in cases.WRAPPERS for family in cases.FAMILIES]
    if arguments.thunks:
        thunks(Model(loaded))
        return 0
    stored_trees, request_trees = universe(singles)
    classify(Model(loaded), stored_trees, request_trees, "the wrappers of the library")
    if arguments.user_kinds:
        from user_kinds import user_kinds

        kinds = tuple(user.kind for user in user_kinds)
        users, variadics, arrays = split(kinds)
        print("user wrappers of one argument:", ", ".join(name for name, _ in users))
        print("variadic:", ", ".join(name for name, _ in variadics),
              "  arrays:", ", ".join(name for name, _ in arrays))
        stored_trees, request_trees = universe(singles + users, variadics, arrays)
        classify(Model(loaded, kinds=kinds), stored_trees, request_trees,
                 "the wrappers of the library and of the user, with the mutations")
    return 0


if __name__ == "__main__":
    sys.exit(main())
