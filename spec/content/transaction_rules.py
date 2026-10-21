"""Content of the transaction rules section.

Every request that starts at a container is a transaction of the container:
when it fails, the bindings registered into the container and the objects its
bindings constructed during it are undone, as if the request had not been made,
whether or not a request that made it catches the failure. A request that
returns keeps its effects, which the request that made it undoes if it fails.
The effects on other containers are their own:
a parent keeps an object a failed request of its child constructed by the
parent's bindings, and a child keeps a binding registered into it while a
request of its parent failed.
"""

from spec.schema import TransactionEffect, TransactionRule

transaction_rules: tuple[TransactionRule, ...] = (
    TransactionRule(
        id="failed_request",
        description=(
            "A failed request of a container removes the bindings registered "
            "into the container during it, destroys the objects its bindings "
            "constructed and keep, and destroys the objects it kept for "
            "requests, in the reverse order they were made, also when the "
            "request that made it catches the failure; a request that returns "
            "keeps them until the request that made it returns or fails."
        ),
        operations=("resolve", "construct", "construct_collection", "invoke"),
        undoes=(
            TransactionEffect.REGISTRATION,
            TransactionEffect.CONSTRUCTION,
            TransactionEffect.RETENTION,
        ),
    ),
)
