"""The typed, language-agnostic specification of Dingo.

The generator imports the validated spec from the repository root:

    import spec
    spec.load_spec().cpp.capabilities

The content of each section lives in spec/content. See
docs/architecture/spec-driven-design.md.
"""

import dataclasses
import importlib

from spec.schema import *  # noqa: F403 - the schema types are part of the package API
from spec.schema import CppSection, Spec, __all__ as _schema_names
from spec.validate import validate

__all__ = [*_schema_names, "SpecError", "build_spec", "load_spec", "validate"]


class SpecError(ValueError):
    """The spec failed validation. The message lists every error."""


def section(name: str) -> tuple:
    return importlib.import_module(f"spec.content.{name}").__dict__[name]


def build_spec() -> Spec:
    """Assembles the spec from spec/content without validating it."""
    sections = {
        field.name: section(field.name)
        for field in dataclasses.fields(Spec)
        if field.name != "cpp"
    }
    cpp = {
        field.name: section(f"cpp_{field.name}")
        for field in dataclasses.fields(CppSection)
    }
    cpp["capabilities"] = tuple(sorted(cpp["capabilities"], key=lambda c: c.id))
    return Spec(**sections, cpp=CppSection(**cpp))


def load_spec() -> Spec:
    """Returns the validated spec, or raises SpecError listing all errors."""
    spec = build_spec()
    errors = validate(spec)
    if errors:
        raise SpecError("\n".join(errors))
    return spec
