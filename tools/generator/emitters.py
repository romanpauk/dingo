"""Emitters of the generated files.

Each emitter takes the loaded spec and returns a list of GeneratedFile. The
emitter computes every value in Python and hands the prepared fields to a
template that only lays them out.
"""

import re

from framework import GeneratedFile, GeneratorError, render

SNAKE_CASE = re.compile(r"[a-z][a-z0-9]*(_[a-z0-9]+)*")


def capability_fields(capability):
    """Prepares the template fields of one cpp.capabilities entry."""
    name = capability.id
    if not SNAKE_CASE.fullmatch(name):
        raise GeneratorError(f"cpp.capabilities.{name}: id is not snake_case")
    has_feature = capability.feature_macro is not None
    if has_feature == (capability.builtin is not None):
        raise GeneratorError(
            f"cpp.capabilities.{name}: exactly one of feature_macro and "
            "builtin must be set"
        )
    if has_feature and capability.minimum is None:
        raise GeneratorError(f"cpp.capabilities.{name}: feature_macro needs minimum")
    if capability.header is not None and not has_feature:
        raise GeneratorError(f"cpp.capabilities.{name}: header needs feature_macro")
    fields = {
        "spec_id": f"cpp.capabilities.{name}",
        "macro": f"DINGO_HAS_{name.upper()}",
        "description_lines": capability.description.strip().splitlines(),
        "builtin": capability.builtin,
        "condition": None,
    }
    if has_feature:
        macro = capability.feature_macro
        fields["condition"] = (
            f"defined({macro}) && {macro} >= {capability.minimum}L"
        )
    return fields


def capabilities(spec):
    """include/dingo/core/capabilities.gen.h from cpp.capabilities."""
    entries = sorted(spec.cpp.capabilities, key=lambda capability: capability.id)
    headers = sorted(
        {capability.header for capability in entries if capability.header}
    )
    content = render(
        "capabilities.gen.h.j2",
        headers=headers,
        capabilities=[capability_fields(capability) for capability in entries],
    )
    return [
        GeneratedFile(
            path="include/dingo/core/capabilities.gen.h",
            content=content,
            spec_ids=("cpp.capabilities",),
        )
    ]


EMITTERS = (capabilities,)
