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
    return {
        "id": name,
        "description_lines": capability.description.strip().splitlines(),
        "builtin": capability.builtin,
        "feature_macro": capability.feature_macro,
        "minimum": capability.minimum,
    }


def capabilities(spec):
    """include/dingo/core/base/capabilities.gen.h from cpp.capabilities."""
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
            path="include/dingo/core/base/capabilities.gen.h",
            content=content,
            spec_ids=("cpp.capabilities",),
        )
    ]


def variants(spec):
    """include/dingo/core/base/variants.gen.h from the language-tier variants.

    Each variant point gets a DINGO_VARIANT_<ID> macro that defaults to the
    capability selecting it and can be defined to override the selection.
    """
    capability_ids = {capability.id for capability in spec.cpp.capabilities}
    fields = []
    for point in sorted(spec.variant_points, key=lambda p: p.id):
        if point.kind.value != "language_tier":
            continue
        if point.capability not in capability_ids:
            raise GeneratorError(
                f"variant_points.{point.id}: capability {point.capability!r} "
                "does not exist"
            )
        fields.append(
            {
                "id": point.id,
                "capability": point.capability,
                "justification_lines": point.justification.strip().splitlines(),
            }
        )
    return [
        GeneratedFile(
            path="include/dingo/core/base/variants.gen.h",
            content=render("variants.gen.h.j2", variants=fields),
            spec_ids=("variant_points",),
        )
    ]


def next_overlay(spec):
    """include/next/<path> for every public include path of the spec.

    Each forwards to the facade of the next implementation under
    include/dingo/core/api, or reports that it is not provided yet.
    """
    include_paths = {
        entry.id for entry in spec.public_api if entry.kind.value == "include_path"
    }
    files = []
    for binding in sorted(spec.cpp.api_bindings, key=lambda b: b.spelling):
        if binding.api not in include_paths:
            continue
        include_path = binding.spelling
        if not include_path.startswith("dingo/"):
            raise GeneratorError(
                f"cpp.api_bindings.{binding.id}: {include_path} is not under dingo/"
            )
        facade = "dingo/core/api/" + include_path[len("dingo/") :]
        files.append(
            GeneratedFile(
                path=f"include/next/{include_path}",
                content=render(
                    "next_overlay.h.j2", include_path=include_path, facade=facade
                ),
                spec_ids=(f"public_api.{binding.api}",),
            )
        )
    return files


EMITTERS = (capabilities, variants, next_overlay)
