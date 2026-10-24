"""The type tree patterns of the rules and their operations, used by the content
modules: a variable binds any type tree, a wrapper applies a semantics to its arguments.
"""

from spec.schema import PatternKind, TypeTreePattern, ValueCategory

LVALUE = ValueCategory.LVALUE
RVALUE = ValueCategory.RVALUE
PRVALUE = ValueCategory.PRVALUE

# The kind a form is of, and the kind it leads to, in the patterns of a form.
KIND = "K"
TARGET = "T"


def variable(name, category=None):
    return TypeTreePattern(kind=PatternKind.VARIABLE, name=name, category=category)


def wrapper(name, *args, category=None):
    return TypeTreePattern(kind=PatternKind.SEMANTICS, name=name, args=args, category=category)


def leaf_variable(category):
    return variable("L", category)


def substitute(pattern, names):
    """The pattern with the wrapper names of `names` replaced."""
    if pattern is None:
        return None
    if pattern.kind is not PatternKind.SEMANTICS:
        return pattern
    return TypeTreePattern(
        kind=pattern.kind,
        name=names.get(pattern.name, pattern.name),
        args=tuple(substitute(arg, names) for arg in pattern.args),
        category=pattern.category,
    )
