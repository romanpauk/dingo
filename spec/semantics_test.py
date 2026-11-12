#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Checks that the rules of the specification hold together: every request
resolves from the layer sources of a binding as it does from the stored
object, at the same cost."""

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).parents[1]))

from spec import semantics  # noqa: E402
from spec.generator.conversions import units  # noqa: E402


def answers(cells):
    """What the specification says of each cell: whether it resolves in a
    call and for the caller, and its fewest copies and moves."""
    return [(unit.expectation(request), unit.converted(request))
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

    def test_a_binding_publishes_few_layer_sources(self):
        """A binding publishes at most a layer source of each layer of what it
        stores, borrowed and consumed, of the leaf and of its interface."""
        for unit in units():
            for interface in {request.interface.bound
                              for request in unit.requests()}:
                if interface.element == "other":
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
                self.assertLessEqual(len(forms), 2 * (layers + 1), unit.base)


if __name__ == "__main__":
    unittest.main()
