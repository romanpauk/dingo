"""Covering arrays: combinations of the values of dimensions that together
hold every combination of values of any `strength` of them."""

from itertools import combinations, product


def covering(strength, *dimensions):
    """The combinations of a value of each dimension that hold every
    combination of values of `strength` dimensions, chosen greedily and so
    the same on every run."""
    candidates = list(product(*dimensions))

    def held(candidate):
        return {tuple((i, candidate[i]) for i in indexes)
                for indexes in combinations(range(len(candidate)), strength)}

    uncovered = set().union(*(held(c) for c in candidates))
    chosen = []
    while uncovered:
        best = max(candidates, key=lambda c: len(held(c) & uncovered))
        chosen.append(best)
        uncovered -= held(best)
    return set(chosen)
