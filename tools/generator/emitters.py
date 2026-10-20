"""Emitters of the generated files.

Each emitter takes the loaded spec and returns a list of GeneratedFile. The
emitter computes every value in Python and hands the prepared fields to a
template that only lays them out.
"""

import re
import textwrap

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
    """include/dingo/core/base/variants.gen.h from the language tiers and the
    compiler variants.

    Each variant point gets a DINGO_VARIANT_<ID> macro that can be defined to
    override the selection. A language tier defaults it to the capability
    selecting it; a compiler variant is selected by detecting the compiler
    defect when the macro is not defined.
    """
    capability_ids = {capability.id for capability in spec.cpp.capabilities}
    fields = []
    for point in sorted(spec.variant_points, key=lambda p: p.id):
        if point.kind.value == "compiler_variant":
            fields.append(
                {
                    "id": point.id,
                    "capability": None,
                    "justification_lines": point.justification.strip().splitlines(),
                }
            )
            continue
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


def pattern_layer(pattern, where):
    """The layer kind of a rule or form pattern, or "leaf" for a variable.

    Patterns are a variable or one layer kind applied to a variable; anything
    else needs the generator to grow.
    """
    if pattern.form.value == "variable":
        return "leaf"
    if pattern.form.value != "layer" or len(pattern.args) != 1:
        raise GeneratorError(f"{where}: pattern is not a layer of one argument")
    return pattern.name


def storage_form_fields(form):
    where = f"storage_forms.{form.id}"
    pattern = form.pattern
    if pattern.form.value == "variable":
        return {"id": form.id, "outer": "leaf", "inner": "leaf"}
    outer = pattern_layer(pattern, where)
    inner = pattern.args[0]
    if inner.form.value == "variable":
        return {"id": form.id, "outer": outer, "inner": "leaf"}
    return {"id": form.id, "outer": outer, "inner": pattern_layer(inner, where)}


REQUEST_CATEGORY_BITS = {"value": 1, "lvalue": 2, "rvalue": 4, "pointer": 8}
REQUIREMENT_BITS = {"copyable": 1, "array": 2, "addressable": 4, "leaf": 8, "stable": 16, "borrowable": 32}


def scope_fields(scope, form_ids):
    exposed = {exposure.form: exposure for exposure in scope.exposures}
    return {
        "id": scope.id,
        "slot": scope.slot.value,
        "materializes": scope.materializes.value,
        "stable": scope.stable,
        "consumes": scope.source_access.value == "consume",
        "guard": scope.guard.value,
        "retains": scope.retains_unresolved_source,
        "exposure_rule": scope.exposure_rule.value,
        "nested_layers": scope.nested_layers,
        "needs_instance": "instance" in scope.needs,
        "exposures": [
            sum(REQUEST_CATEGORY_BITS[c.value] for c in exposed[form].categories)
            if form in exposed
            else 0
            for form in form_ids
        ],
    }


def route_fields(rule):
    where = f"routes.{rule.id}"
    for pattern in (rule.from_shape, rule.to_shape):
        if pattern.category is None:
            raise GeneratorError(f"{where}: pattern has no value category")
    return {
        "id": rule.id,
        "from_layer": pattern_layer(rule.from_shape, where),
        "from_category": rule.from_shape.category.value,
        "to_layer": pattern_layer(rule.to_shape, where),
        "to_category": rule.to_shape.category.value,
        "access": rule.access.value,
        "exposure": REQUEST_CATEGORY_BITS[rule.exposure.value] if rule.exposure else 0,
        "requires": sum(REQUIREMENT_BITS[r.predicate.value] for r in rule.requires),
        "upcast": rule.via is not None and rule.via.value == "upcast",
        "cached": rule.cache is not None,
    }


def selection_fields(rule):
    """The fields of a selection rule: its steps, and the status of each
    outcome count."""
    outcomes = {outcome.count.value: outcome.status.value for outcome in rule.outcomes}
    return {
        "id": rule.id,
        "mode": rule.mode.value,
        "first_non_empty": rule.combine.value == "first_non_empty",
        "steps": [
            {
                "sources": [source.value for source in step.sources],
                "when_constructible": step.when is not None
                and step.when.value == "constructible",
            }
            for step in rule.steps
        ],
        "empty": outcomes["empty"],
        "one": outcomes["one"],
        "many": outcomes["many"],
    }


def vocabulary(spec):
    """include/dingo/core/vocabulary/vocabulary.gen.h from the semantic sections."""
    from spec.schema import (
        Access,
        ExposureRule,
        RecursionGuard,
        SelectionSource,
        Slot,
        ValueCategory,
    )

    layer_kinds = [
        {
            "id": kind.id,
            "owning": "owning" in {t.value for t in kind.traits},
            "sharing": "sharing" in {t.value for t in kind.traits},
            "nullable": "nullable" in {t.value for t in kind.traits},
            "borrowable": "borrowable" in {t.value for t in kind.traits},
            "variadic": kind.variadic,
            "has_extent": kind.has_extent,
        }
        for kind in sorted(spec.layer_kinds, key=lambda k: k.id)
    ]
    forms = sorted(spec.storage_forms, key=lambda f: f.id)
    form_ids = [form.id for form in forms]
    content = render(
        "vocabulary.gen.h.j2",
        layer_kinds=layer_kinds,
        value_categories=[c.value for c in ValueCategory],
        request_categories=[
            {"name": name, "bit": bit} for name, bit in REQUEST_CATEGORY_BITS.items()
        ],
        accesses=[a.value for a in Access],
        storage_forms=[storage_form_fields(form) for form in forms],
        slots=[s.value for s in Slot],
        guards=[g.value for g in RecursionGuard],
        exposure_rules=[r.value for r in ExposureRule],
        scopes=[scope_fields(scope, form_ids) for scope in sorted(spec.scopes, key=lambda s: s.id)],
        requirements=[{"name": name, "bit": bit} for name, bit in REQUIREMENT_BITS.items()],
        routes=[route_fields(rule) for rule in spec.routes],
        key_kinds=[
            {"id": kind.id, "has_value": kind.value_time is not None}
            for kind in spec.key_kinds
        ],
        selection_sources=[source.value for source in SelectionSource],
        selection_rules=[selection_fields(rule) for rule in spec.selection_rules],
        max_selection_steps=max((len(r.steps) for r in spec.selection_rules), default=1),
        max_step_sources=max(
            (len(step.sources) for r in spec.selection_rules for step in r.steps),
            default=1,
        ),
    )
    return [
        GeneratedFile(
            path="include/dingo/core/vocabulary/vocabulary.gen.h",
            content=content,
            spec_ids=(
                "key_kinds",
                "layer_kinds",
                "routes",
                "scopes",
                "selection_rules",
                "storage_forms",
            ),
        )
    ]


def scope_tags(spec):
    """include/dingo/core/vocabulary/scope_tags.gen.h from cpp.scope_projections."""
    tags = []
    namespaces = set()
    for projection in sorted(spec.cpp.scope_projections, key=lambda p: p.scope):
        namespace, _, name = projection.spelling.rpartition("::")
        if not namespace or not name:
            raise GeneratorError(
                f"cpp.scope_projections.{projection.id}: {projection.spelling} "
                "is not a qualified name"
            )
        namespaces.add(namespace)
        tags.append({"scope": projection.scope, "name": name})
    if len(namespaces) != 1:
        raise GeneratorError("cpp.scope_projections: tags span several namespaces")
    return [
        GeneratedFile(
            path="include/dingo/core/vocabulary/scope_tags.gen.h",
            content=render("scope_tags.gen.h.j2", namespace=namespaces.pop(), tags=tags),
            spec_ids=("cpp.scope_projections",),
        )
    ]


def cpp_string(text):
    """The C++ string literal of text."""
    escaped = text.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def diagnostics(spec):
    """include/dingo/core/vocabulary/diagnostics.gen.h from the diagnostics.

    Each diagnostic gets a function that throws its exception with its message,
    whose placeholders are the parameters of the function.
    """
    fields = [
        {
            "id": diagnostic.id,
            "condition_lines": textwrap.wrap(diagnostic.condition, 76),
            "exception": diagnostic.exception,
            "message": cpp_string(diagnostic.message),
            "placeholders": list(diagnostic.placeholders),
        }
        for diagnostic in sorted(spec.diagnostics, key=lambda d: d.id)
    ]
    return [
        GeneratedFile(
            path="include/dingo/core/vocabulary/diagnostics.gen.h",
            content=render("diagnostics.gen.h.j2", diagnostics=fields),
            spec_ids=("diagnostics",),
        )
    ]


EMITTERS = (capabilities, variants, vocabulary, scope_tags, diagnostics, next_overlay)
