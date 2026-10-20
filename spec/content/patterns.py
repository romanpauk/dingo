"""The shape patterns of the routes and their forms, a helper of the content
modules: a variable binds any shape, a layer applies a kind to its arguments.
"""

from spec.schema import PatternForm, ShapePattern, ValueCategory

LVALUE = ValueCategory.LVALUE
RVALUE = ValueCategory.RVALUE
PRVALUE = ValueCategory.PRVALUE

# The kind a form is of, and the kind it leads to, in the patterns of a form.
KIND = "K"
TARGET = "T"


def variable(name, category=None):
    return ShapePattern(form=PatternForm.VARIABLE, name=name, category=category)


def layer(name, *args, category=None):
    return ShapePattern(form=PatternForm.LAYER, name=name, args=args, category=category)


def leaf_variable(category):
    return variable("L", category)


def substitute(pattern, names):
    """The pattern with the layer names of `names` replaced."""
    if pattern is None:
        return None
    if pattern.form is not PatternForm.LAYER:
        return pattern
    return ShapePattern(
        form=pattern.form,
        name=names.get(pattern.name, pattern.name),
        args=tuple(substitute(arg, names) for arg in pattern.args),
        category=pattern.category,
    )
