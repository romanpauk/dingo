#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Checks that the rules of the specification hold together: every request
resolves from the layer sources of a binding as it does from the stored
object, at the same cost, and as defined when a layer is empty."""

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).parents[1]))

from spec import semantics  # noqa: E402
from spec.generator.conversions import units  # noqa: E402
from spec.vocabulary import Entry, Position  # noqa: E402


def defined(unit, request):
    """Whether a request resolves in a call and for the caller as defined
    when a layer of what the registration stores is empty."""
    bound = request.interface.bound
    if bound.element == "other":
        return None
    return tuple(
        semantics.fewest(unit.scope, unit.form, unit.leaf, bound,
                         request.shape, request.category, Position.DIRECT,
                         entry, semantics.LIMITATIONS,
                         interfaces=unit.interfaces_declared, defined=True)
        for entry in (Entry.INVOKE, Entry.RESOLVE))


def answers(cells):
    """What the specification says of each cell: whether it resolves in a
    call and for the caller, its fewest copies and moves, and whether it is
    defined when a layer is empty."""
    return [(unit.expectation(request), unit.converted(request),
             defined(unit, request))
            for unit, request in cells]


class LayerSourcesTest(unittest.TestCase):
    def test_requests_resolve_from_layer_sources(self):
        cells = [(unit, request) for unit in units()
                 for request in unit.requests()]
        from_stored = answers(cells)
        reachable = semantics.reachable
        semantics.reachable = semantics.requested
        try:
            from_sources = answers(cells)
        finally:
            semantics.reachable = reachable
        differing = [
            f"{unit.base}/{request.interface.name}/{request.name}: "
            f"{stored} from the stored object, {sources} from the layer "
            "sources"
            for (unit, request), stored, sources in zip(cells, from_stored,
                                                        from_sources)
            if stored != sources]
        if differing:
            self.fail(f"{len(differing)} cells differ:\n" +
                      "\n".join(differing[:20]))

    def test_a_binding_of_the_leaf_publishes_few_layer_sources(self):
        """A binding of the leaf publishes at most three layer sources of each
        layer of what it stores: the layer, borrowed or consumed, a pointer
        into it, and the optional it holds, which do not fail when the layer
        is empty. One of the abstract interface publishes too what the
        request cannot make of the leaf it cannot name."""
        for unit in units():
            for interface in {request.interface.bound
                              for request in unit.requests()}:
                if interface.element != "leaf":
                    continue
                context = semantics.Context(
                    semantics.LEAVES[unit.leaf], semantics.SCOPES[unit.scope],
                    interface.element == "interface", semantics.LIMITATIONS)
                start = semantics.source(unit.scope, unit.form,
                                         unit.interfaces_declared)
                forms = {(thing.layers, thing.origin.owned)
                         for thing, _ in semantics.layer_sources(start,
                                                                 context)
                         if thing.element == interface.element}
                layers = len(semantics.FORMS[unit.form])
                self.assertLessEqual(len(forms), 3 * (layers + 1), unit.base)


if __name__ == "__main__":
    unittest.main()
