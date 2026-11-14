"""Compares the semantics of the library with the specification, for every
stored object and every request.

The library decides what a request resolves to by dingo/semantics/model.h,
of no C++ type: a layer is its properties, in the universe of the layers a
search may make. This check gives it two domains and compares its answer for
every category and lifetime with the answer of spec/semantics.py: the fewest
copies and moves of the leaf of what satisfies the request, or that nothing
does. Both domains are finite, so the comparison is complete.

- The standard layers: every stored object the specification makes, with and
  without the known limitations, and every request of every shape of every
  interface a registration binds.
- Every layer a type could define: each combination of the properties of a
  layer, beside the layers of C++ itself, stored and requested in every
  stack of it and of a pointer, of every scope and leaf.
"""

import argparse
import itertools
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

# The layers of C++ itself, at their indices in every universe of the
# library.
LANGUAGE = ("pointer", "array_pointer", "array", "array2")
ELEMENTS = {"leaf": 0, "interface": 1}
LIFETIMES = (semantics.Lifetime.REQUEST, semantics.Lifetime.STABLE)
ORIGINS = {semantics.Origin.BORROWED: 0, semantics.Origin.MADE: 1,
           semantics.Origin.COPIED: 2}
OWNERS = {semantics.Owner.NONE: 0, semantics.Owner.SOLE: 1,
          semantics.Owner.SHARED: 2}
LIMITS = {frozenset(): 0, semantics.LIMITATIONS: 3}
SOURCE = Path(__file__).parents[1] / "test" / "model" / "semantics.cpp"


def properties(layer, universe):
    """A layer as the program reads it: its properties, and the layers of the
    universe an owned value of it converts to, as a mask."""
    converts = sum(1 << universe.index(name) for name in layer.converts_to
                   if name in universe)
    return [OWNERS[layer.owner], int(layer.shares_element),
            int(layer.copyable), int(layer.made_from_value),
            int(layer.holds_element_only), int(layer.relocatable),
            int(layer.gives_element), int(layer.bounded),
            int(layer.may_be_empty), int(layer.has_empty_value),
            int(layer.adopts), int(bool(layer.decays_to)),
            int(layer.adopted_with is not None),
            int(bool(layer.adopted_with)), converts]


def order(names):
    """A universe as the library orders it: the layers of C++ itself first."""
    return LANGUAGE + tuple(name for name in names if name not in LANGUAGE)


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


def standard():
    """The stored objects the specification makes, with the context of their
    registration, and the requests of them."""
    goals = [(semantics.SHAPES[shape] + bound.layers, bound.element,
              f"{shape.value}/{bound.element}{list(bound.layers)}")
             for shape in semantics.SHAPES for bound in bounds()]
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
                                   semantics.source(scope, form, interfaces),
                                   goals)


def made_layers():
    """Every layer a type could define, by its properties, beside the layers
    of C++ itself: it gives its element, is no raw pointer and no array, and
    is possible, as semantics.possible says."""
    emptiness = ((False, False), (True, False), (True, True))
    for index, (owner, shares, copyable, made, holds, relocatable, empty,
                adopts) in enumerate(itertools.product(
                    semantics.Owner, (False, True), (False, True),
                    (False, True), (False, True), (False, True), emptiness,
                    (False, True))):
        layer = semantics.Layer(
            f"made{index}", owner, shares_element=shares, copyable=copyable,
            made_from_value=made, holds_element_only=holds,
            relocatable=relocatable, may_be_empty=empty[0],
            has_empty_value=empty[1], adopts=adopts)
        if semantics.possible(layer):
            yield layer


def lifetime(scope):
    return (semantics.Lifetime.REQUEST if scope.makes
            else semantics.Lifetime.STABLE)


def makes(layers, context):
    """Whether a scope makes a stack, as makes_storage says of a stored
    form."""
    if context.scope.given or not layers:
        return True
    outer = semantics.LAYERS[layers[0]]
    if outer.bounded and not context.leaf.default_constructible:
        return False
    return not (context.scope.publishes_early and
                context.leaf.made_by_factory and outer.shares_element)


def swept():
    """Each layer a type could define, stored and requested in every stack of
    it, of a pointer and of an array, of every scope and leaf."""
    for layer in made_layers():
        semantics.LAYERS[layer.name] = layer
        name = layer.name
        universe = LANGUAGE + (name,)
        stored = ((), (name,), (name, "array"), (name, "array2"),
                  (name, name))
        requested = [stack for depth in range(4)
                     for stack in itertools.product((name, "pointer"),
                                                    repeat=depth)]
        requested += [(name, "array"), (name, "array2")]
        goals = [(stack, element, f"{list(stack)}/{element}")
                 for stack in requested for element in ELEMENTS]
        for scope, scope_traits in semantics.SCOPES.items():
            for leaf, traits in semantics.LEAVES.items():
                for abstract in (False, True):
                    context = semantics.Context(
                        traits, scope_traits, abstract, semantics.LIMITATIONS,
                        universe)
                    for layers in stored:
                        if not makes(layers, context):
                            continue
                        start = semantics.Thing(
                            layers, "leaf",
                            (lifetime(scope_traits),) * (len(layers) + 1),
                            (semantics.Origin.MADE if scope_traits.makes
                             else semantics.Origin.BORROWED),
                            instance=True)
                        yield (f"{list(layers)}/{scope.value}/{leaf.value}/"
                               f"{'interface' if abstract else 'leaf'}/{name}",
                               context, start, goals)


def expected(reached, context, layers, element):
    """The answers of the specification for a request, by its search of
    every thing the stored object reaches."""
    things = [thing for thing in reached if thing.element == element]
    answers = []
    for category in Category:
        for lifetime_of in LIFETIMES:
            costs = [semantics.affected(cost, effect)
                     for thing, effect in semantics.satisfactions(
                         things, category, lifetime_of, layers, context)
                     for cost in reached[thing]]
            answers.append("%d,%d" % min(costs) if costs else "-")
    return " ".join(answers)


def encode(context, start, layers, element):
    """A request as the program reads it."""
    universe = order(context.universe)
    made = universe[len(LANGUAGE):]
    numbers = [LIMITS[context.limits], int(context.leaf.copyable),
               int(context.leaf.movable), int(context.scope.makes),
               int(context.scope.publishes_early), int(context.abstract),
               len(made)]
    for name in made:
        numbers += properties(semantics.LAYERS[name], universe)
    numbers += [len(start.layers),
                *(universe.index(name) for name in start.layers),
                ELEMENTS[start.element], start.lifetime.value,
                ORIGINS[start.origin], len(layers),
                *(universe.index(name) for name in layers),
                ELEMENTS[element]]
    return " ".join(map(str, numbers))


def compare(program, domain):
    """The requests of a domain, how many of their answers resolve, and those
    whose answers differ."""
    language = [str(len(LANGUAGE))]
    for name in LANGUAGE:
        language += map(str, properties(semantics.LAYERS[name], LANGUAGE))
    cases = []
    lines = [" ".join(language)]
    for name, context, start, goals in domain:
        reached = semantics.reachable(start, context)
        for layers, element, goal in goals:
            cases.append((f"{name}/{goal}",
                          expected(reached, context, layers, element)))
            lines.append(encode(context, start, layers, element))
    run = subprocess.run([str(program)], input="\n".join(lines) + "\n",
                         capture_output=True, text=True)
    answers = run.stdout.splitlines()
    if run.returncode != 0 or len(answers) != len(cases):
        print(run.stderr or f"{len(answers)} answers to {len(cases)} requests")
        return len(cases), 0, [(name, want, "") for name, want in cases]
    resolved = sum(answer != "-" for _, want in cases
                   for answer in want.split())
    return len(cases), resolved, [
        (name, want, got.strip())
        for (name, want), got in zip(cases, answers) if want != got.strip()]


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
    failed = False
    for title, domain in (("standard layers", standard()),
                          ("every layer a type could define", swept())):
        count, resolved, differ = compare(program, domain)
        print(f"{title}: {count} requests, {count * 14} answers, {resolved} "
              f"of them resolved, {len(differ)} requests differ")
        for name, want, got in differ[:20]:
            print(f"  {name}\n    specification {want}\n    library       {got}")
        failed = failed or bool(differ)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
