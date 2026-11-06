"""Pairwise covering arrays: combinations of the values of dimensions that
together hold every pair of values of two of them."""

from itertools import product


def covering_pairs(*dimensions):
    """The combinations of a value of each dimension that hold every pair of
    values of two dimensions, chosen greedily and so the same on every run."""
    combinations = list(product(*dimensions))

    def pairs(combination):
        return {(i, combination[i], j, combination[j])
                for i in range(len(combination))
                for j in range(i + 1, len(combination))}

    uncovered = set().union(*(pairs(c) for c in combinations))
    chosen = []
    while uncovered:
        best = max(combinations, key=lambda c: len(pairs(c) & uncovered))
        chosen.append(best)
        uncovered -= pairs(best)
    return set(chosen)
