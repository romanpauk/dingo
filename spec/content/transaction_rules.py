"""Content of the transaction rules section.

A request that starts at a container is a transaction of the container: when
the outermost request of the container fails, the bindings registered into the
container and the objects its bindings constructed during it are undone, as if
the request had not been made. The effects on other containers are their own:
a parent keeps an object a failed request of its child constructed by the
parent's bindings, and a child keeps a binding registered into it while a
request of its parent failed.
"""

from spec.schema import TransactionEffect, TransactionRule

transaction_rules: tuple[TransactionRule, ...] = (
    TransactionRule(
        id="failed_request",
        description=(
            "A failed outermost request of a container removes the bindings "
            "registered into the container during it, destroys the objects its "
            "bindings constructed and keep, and destroys the objects it kept for "
            "requests, in the reverse order they were made; a request that "
            "returns keeps them."
        ),
        operations=("resolve", "construct", "construct_collection", "invoke"),
        undoes=(
            TransactionEffect.REGISTRATION,
            TransactionEffect.CONSTRUCTION,
            TransactionEffect.RETENTION,
        ),
    ),
)
