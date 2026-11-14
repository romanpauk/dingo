"""Compares the semantics of the library with the specification, for every
stored object and every request.

The library decides what a request resolves to by dingo/semantics/model.h,
of no C++ type. This check gives it every stored object the specification
makes, with and without the known limitations, and every request of every
shape of every interface a registration binds, and compares its answer for
every category and lifetime with the answer of spec/semantics.py: the fewest
copies and moves of the leaf of what satisfies the request, or that nothing
does. The domain is finite, so the comparison is complete.
"""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).parents[1]))

from spec import semantics  # noqa: E402
from spec.generator import driver  # noqa: E402
from spec.vocabulary import (Category, InterfaceSet, Scope,  # noqa: E402
                             Selection, StoredForm)

LAYERS = list(semantics.LAYERS)
ELEMENTS = {"leaf": 0, "interface": 1}
LIFETIMES = (semantics.Lifetime.REQUEST, semantics.Lifetime.STABLE)
ORIGINS = {semantics.Origin.BORROWED: 0, semantics.Origin.MADE: 1,
           semantics.Origin.COPIED: 2}
LIMITS = {frozenset(): 0, semantics.LIMITATIONS: 3}
SOURCE = Path(__file__).parents[1] / "test" / "model" / "semantics.cpp"


def bounds():
    """Every interface a registration binds, of the leaf or of its abstract
    interface: the other alternative of an alternative type is no element
    the semantics reach."""
    found = {semantics.LEAF, semantics.INTERFACE}
    for form in StoredForm:
        for interfaces in InterfaceSet:
            for selection in Selection:
                found.update(semantics.bound_interfaces(form, interfaces,
                                                        selection))
    return sorted(bound for bound in found if bound.element in ELEMENTS)


def starts():
    """Every stored object the specification makes, with the context of its
    registration."""
    for scope in Scope:
        for form in StoredForm:
            for leaf, traits in semantics.LEAVES.items():
                for abstract in (False, True):
                    for interfaces in InterfaceSet:
                        for limits in LIMITS:
                            context = semantics.Context(
                                traits, semantics.SCOPES[scope], abstract,
                                limits)
                            if not semantics.makes_storage(form, context):
                                continue
                            yield (f"{scope.value}/{form.value}/{leaf.value}/"
                                   f"{'interface' if abstract else 'leaf'}/"
                                   f"{interfaces.value}/{LIMITS[limits]}",
                                   context,
                                   semantics.source(scope, form, interfaces))


def expected(reached, context, layers, element):
    """The answers of the specification for a request, by its search of
    every thing the stored object reaches."""
    things = [thing for thing in reached if thing.element == element]
    answers = []
    for category in Category:
        for lifetime in LIFETIMES:
            costs = [semantics.affected(cost, effect)
                     for thing, effect in semantics.satisfactions(
                         things, category, lifetime, layers, context)
                     for cost in reached[thing]]
            answers.append("%d,%d" % min(costs) if costs else "-")
    return " ".join(answers)


def encode(context, start, layers, element):
    """A request as the program reads it."""
    numbers = [LIMITS[context.limits], int(context.leaf.copyable),
               int(context.leaf.movable), int(context.scope.makes),
               int(context.scope.publishes_early), int(context.abstract),
               len(start.layers), *(LAYERS.index(name) for name in start.layers),
               ELEMENTS[start.element], start.lifetime.value,
               ORIGINS[start.origin],
               len(layers), *(LAYERS.index(name) for name in layers),
               ELEMENTS[element]]
    return " ".join(map(str, numbers))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--flags", default="-std=c++17 -O2 -Iinclude",
                        help="compiler flags, with include paths")
    options = parser.parse_args()
    options.work.mkdir(parents=True, exist_ok=True)
    program = options.work / "semantics"
    subprocess.run(driver.compiler_command(
        options.compiler, *shlex.split(options.flags), str(SOURCE), "-o",
        str(program)), check=True)

    cases = []
    lines = []
    goals = [(semantics.SHAPES[shape] + bound.layers, bound.element,
              f"{shape.value}/{bound.element}{list(bound.layers)}")
             for shape in semantics.SHAPES for bound in bounds()]
    for name, context, start in starts():
        reached = semantics.reachable(start, context)
        for layers, element, goal in goals:
            cases.append((f"{name}/{goal}",
                          expected(reached, context, layers, element)))
            lines.append(encode(context, start, layers, element))
    run = subprocess.run([str(program)], input="\n".join(lines) + "\n",
                         capture_output=True, text=True, check=True)
    answers = run.stdout.splitlines()
    if len(answers) != len(cases):
        print(f"{len(answers)} answers to {len(cases)} requests")
        return 1
    differ = [(name, want, got.strip())
              for (name, want), got in zip(cases, answers)
              if want != got.strip()]
    resolved = sum(answer != "-" for _, want in cases
                   for answer in want.split())
    print(f"{len(cases)} requests, {len(cases) * 14} answers, {resolved} of "
          f"them resolved, {len(differ)} requests differ")
    for name, want, got in differ[:20]:
        print(f"  {name}\n    specification {want}\n    library       {got}")
    return 1 if differ else 0


if __name__ == "__main__":
    sys.exit(main())
