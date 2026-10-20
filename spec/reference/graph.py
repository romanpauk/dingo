"""The reference implementation of the graph rules.

A binding of a graph has a scope and the bindings it depends on. A cycle is
legal if and only if every binding in it has a scope whose slot the cycle
rule allows;
the reference finds the shortest illegal cycle through the first binding
that has one, as the C++ implementation reports it.
"""

from collections import deque
from dataclasses import dataclass

from spec.schema import Spec


@dataclass(frozen=True, slots=True)
class Node:
    name: str
    scope: str
    depends_on: tuple[str, ...] = ()


def illegal_cycle(spec: Spec, nodes: list[Node]) -> list[str] | None:
    """The path of the shortest cycle through the first node, in order, whose
    scope the cycle rule does not allow, ending with the node it starts
    with; None when every cycle is legal."""
    rule = next(rule for rule in spec.graph_rules if rule.id == "cycle")
    slots = {scope.id: scope.slot for scope in spec.scopes}
    edges = {node.name: node.depends_on for node in nodes}
    for start in nodes:
        if slots[start.scope] in rule.legal_slots:
            continue
        parent: dict[str, str] = {}
        queue = deque([start.name])
        while queue:
            node = queue.popleft()
            for target in edges.get(node, ()):
                if target in parent:
                    continue
                parent[target] = node
                if target == start.name:
                    path = [start.name]
                    at = parent[start.name]
                    while at != start.name:
                        path.append(at)
                        at = parent[at]
                    path.append(start.name)
                    return list(reversed(path))
                queue.append(target)
    return None
