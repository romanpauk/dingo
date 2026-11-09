"""The dimensions only the probes have, an enum per axis, beside those of the
specification in spec/vocabulary.py. The value of a member is its name in a
cell; how it is spelled in C++ is the templates' concern."""

from spec.vocabulary import Dimension


class Keeps(Dimension):
    """How the scope of a cell keeps what it resolves."""
    INSTANCE = "instance"
    ADDRESSES = "addresses"
    NOTHING = "nothing"


class Kind(Dimension):
    """A class generated around a parameter type, whose constructor the
    library detects."""
    SINGLE = "single"
    EXPLICIT_SINGLE = "explicit_single"
    AGGREGATE = "aggregate"
    PAIR = "pair"

    @property
    def arity(self):
        return 2 if self is Kind.PAIR else 1


class Detection(Dimension):
    """How the library detects a constructor."""
    SHAPE = "shape"
    SIGNATURE = "signature"


class Construction(Dimension):
    """The constructor a class is constructed with: the one a factory names
    explicitly, or the one detection finds."""
    EXPLICIT = "explicit"
    SHAPE = "shape"
    SIGNATURE = "signature"


class Making(Dimension):
    """How a class is made: by an entry point of a container, or by
    resolving its registration."""
    RESOLVE = "resolve"
    CONSTRUCT = "construct"
    INVOKE = "invoke"
    REGISTERED = "registered"


class Outcome(Dimension):
    """What the library answered for a cell."""
    INVALID = "invalid"
    NOT_REGISTRABLE = "not_registrable"
    REJECTED = "rejected"
    DOES_NOT_COMPILE = "does_not_compile"
    CRASHED = "crashed"
    VIOLATED = "violated"
    RESOLVED = "resolved"
    TYPE_NOT_FOUND = "type_not_found"
    TYPE_NOT_CONVERTIBLE = "type_not_convertible"
    TYPE_AMBIGUOUS = "type_ambiguous"
    EXCEPTION = "exception"
    # A request a binding publishes and selects that no probe makes, and one
    # it publishes that selects another binding.
    UNPROBED = "unprobed"
    UNREACHABLE = "unreachable"
    DETECTED = "detected"
    MISDETECTED = "misdetected"
    # Misdetected as a known limitation says, and a known limitation that
    # no longer occurs.
    LIMITED = "limited"
    STALE_LIMITATION = "stale_limitation"
    # Not the outcome the semantics expect.
    UNEXPECTED = "unexpected"
    # What the conversion layer of a binding answers a request: a resolution
    # with an available conversion, a wrapper composed of one, a value taken
    # out of a pointer the binding hands over, a reference into the context
    # of a resolution, one whose conversion is not available, none, and a
    # different answer of the two containers; a request the binding of
    # another interface answers.
    PUBLISHED = "published"
    COMPOSED = "composed"
    ADOPTED = "adopted"
    PLACED = "placed"
    UNCONVERTIBLE = "unconvertible"
    UNPUBLISHED = "unpublished"
    DIVERGED = "diverged"
    SELECTS_ANOTHER = "selects_another"

    @property
    def is_defect(self):
        return self in (Outcome.DOES_NOT_COMPILE, Outcome.CRASHED,
                        Outcome.VIOLATED, Outcome.UNPROBED,
                        Outcome.UNREACHABLE, Outcome.MISDETECTED,
                        Outcome.STALE_LIMITATION, Outcome.UNEXPECTED,
                        Outcome.DIVERGED)
