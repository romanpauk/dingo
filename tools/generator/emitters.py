"""Emitters of the generated files.

Each emitter takes the loaded spec and returns a list of GeneratedFile. The
emitter computes every value in Python and hands the prepared fields to a
template that only lays them out.
"""

import re
import string
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


def form_layer(pattern, where):
    """Where a pattern of a route form is: "element" for a variable, "kind"
    for the kind that has the form, "target" for the kind it leads to.

    Patterns are a variable or one layer applied to a variable; anything else
    needs the generator to grow.
    """
    if pattern.form.value == "variable":
        return "element"
    if pattern.form.value != "layer" or len(pattern.args) != 1:
        raise GeneratorError(f"{where}: pattern is not a layer of one argument")
    if pattern.name == "K":
        return "kind"
    if pattern.name == "T":
        return "target"
    raise GeneratorError(f"{where}: pattern names the layer kind {pattern.name}")


# The property bits of a layer kind, in the order of LayerTrait, then whether
# it is variadic and has an extent.
LAYER_PROPERTIES = (
    "owning",
    "sharing",
    "nullable",
    "borrowable",
    "in_place",
    "unsized",
    "constructible",
    "copyable",
    "immovable",
    "wrapper",
    "variadic",
    "extent",
)
LAYER_PROPERTY_BITS = {name: 1 << index for index, name in enumerate(LAYER_PROPERTIES)}


def layer_class_fields(layer_class):
    """A layer class as the property bits a layer has, and those it lacks."""
    if layer_class is None:
        return None
    has = sum(LAYER_PROPERTY_BITS[t.value] for t in layer_class.has)
    lacks = sum(LAYER_PROPERTY_BITS[t.value] for t in layer_class.lacks)
    for name, value in (("variadic", layer_class.variadic), ("extent", layer_class.extent)):
        if value is True:
            has |= LAYER_PROPERTY_BITS[name]
        elif value is False:
            lacks |= LAYER_PROPERTY_BITS[name]
    return {"has": has, "lacks": lacks}


def storage_form_fields(form):
    return {
        "id": form.id,
        "outer": layer_class_fields(form.outer),
        "inner": layer_class_fields(form.inner),
    }


def layer_property_names(kind):
    """The property names of a layer kind, in the order of their bits."""
    names = {trait.value for trait in kind.traits}
    if kind.variadic:
        names.add("variadic")
    if kind.has_extent:
        names.add("extent")
    return [name for name in LAYER_PROPERTIES if name in names]


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


def form_fields(name, rule, of_kind, where):
    """The fields of a route form, or of a rule of a leaf, from its template
    or its route."""
    for pattern in (rule.from_shape, rule.to_shape):
        if pattern.category is None:
            raise GeneratorError(f"{where}: pattern has no value category")
    return {
        "id": name,
        "of_kind": of_kind,
        "from_layer": form_layer(rule.from_shape, where),
        "from_category": rule.from_shape.category.value,
        "to_layer": form_layer(rule.to_shape, where),
        "to_category": rule.to_shape.category.value,
        "access": rule.access.value,
        "exposure": REQUEST_CATEGORY_BITS[rule.exposure.value] if rule.exposure else 0,
        "requires": sum(REQUIREMENT_BITS[r.predicate.value] for r in rule.requires),
        "upcast": rule.via is not None and rule.via.value == "upcast",
        "converts": rule.via is not None and rule.via.value == "convert",
        "cached": rule.cache is not None,
        "kind_properties": sum(
            LAYER_PROPERTY_BITS[t.value] for t in getattr(rule, "kind_traits", ())
        ),
    }


def route_form_entries(spec):
    """The route forms and the rules of a leaf in the order of the algebra,
    the order of their first routes."""
    templates = {template.form: template for template in spec.route_forms}
    entries, seen = [], set()
    for route in spec.routes:
        key = route.form if route.form is not None else route.id
        if key in seen:
            continue
        seen.add(key)
        if route.form is None:
            entries.append(form_fields(route.id, route, False, f"routes.{route.id}"))
        else:
            template = templates[route.form]
            entries.append(
                form_fields(route.form.value, template, True, f"route_forms.{template.id}")
            )
    for form, template in templates.items():
        if form not in seen:
            entries.append(form_fields(form.value, template, True, f"route_forms.{template.id}"))
    return entries


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
                "when": step.when.value if step.when is not None else "always",
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
        SelectionMode,
        SelectionSource,
        Slot,
        StepCondition,
        ValueCategory,
    )

    forms = sorted(spec.storage_forms, key=lambda f: f.id)
    content = render(
        "vocabulary.gen.h.j2",
        layer_properties=[
            {"name": name, "bit": bit} for name, bit in LAYER_PROPERTY_BITS.items()
        ],
        value_categories=[c.value for c in ValueCategory],
        request_categories=[
            {"name": name, "bit": bit} for name, bit in REQUEST_CATEGORY_BITS.items()
        ],
        accesses=[a.value for a in Access],
        storage_forms=[storage_form_fields(form) for form in forms],
        slots=[s.value for s in Slot],
        guards=[g.value for g in RecursionGuard],
        exposure_rules=[r.value for r in ExposureRule],
        requirements=[{"name": name, "bit": bit} for name, bit in REQUIREMENT_BITS.items()],
        route_forms=route_form_entries(spec),
        key_kinds=[
            {"id": kind.id, "has_value": kind.value_time is not None}
            for kind in spec.key_kinds
        ],
        selection_sources=[source.value for source in SelectionSource],
        selection_modes=[mode.value for mode in SelectionMode],
        step_conditions=["always", *(condition.value for condition in StepCondition)],
        selection_rules=[selection_fields(rule) for rule in spec.selection_rules],
        max_selection_steps=max((len(r.steps) for r in spec.selection_rules), default=1),
        max_step_sources=max(
            (len(step.sources) for r in spec.selection_rules for step in r.steps),
            default=1,
        ),
    )
    algebra = render(
        "route_forms.gen.h.j2",
        layer_properties=[
            {"name": name, "bit": bit} for name, bit in LAYER_PROPERTY_BITS.items()
        ],
        value_categories=[c.value for c in ValueCategory],
        accesses=[a.value for a in Access],
        requirements=[{"name": name, "bit": bit} for name, bit in REQUIREMENT_BITS.items()],
        route_forms=route_form_entries(spec),
    )
    return [
        GeneratedFile(
            path="include/dingo/core/vocabulary/route_forms.gen.h",
            content=algebra,
            spec_ids=("layer_kinds", "route_forms", "routes"),
        ),
        GeneratedFile(
            path="include/dingo/core/vocabulary/vocabulary.gen.h",
            content=content,
            spec_ids=("key_kinds", "scopes", "selection_rules", "storage_forms"),
        ),
    ]


def layer_kinds(spec):
    """include/dingo/core/vocabulary/layer_kinds.gen.h from the layer kinds and
    cpp.kind_projections: the tag of each layer kind of the library and its
    layer_kind_traits, the declaration a kind of the user makes too."""
    spellings = {p.kind: p.spelling for p in spec.cpp.kind_projections}
    namespaces = {spelling.rpartition("::")[0] for spelling in spellings.values()}
    if len(namespaces) != 1:
        raise GeneratorError("cpp.kind_projections: tags span several namespaces")
    names = {kind: spelling.rpartition("::")[2] for kind, spelling in spellings.items()}
    kinds = [
        {
            "id": kind.id,
            "name": names[kind.id],
            "properties": layer_property_names(kind),
            "forms": [
                {"form": form.form.value, "target": names.get(form.target)}
                for form in kind.forms
            ],
        }
        for kind in spec.layer_kinds
    ]
    return [
        GeneratedFile(
            path="include/dingo/core/vocabulary/layer_kinds.gen.h",
            content=render(
                "layer_kinds.gen.h.j2", namespace=namespaces.pop(), kinds=kinds
            ),
            spec_ids=("cpp.kind_projections", "layer_kinds"),
        )
    ]


def scope_tags(spec):
    """include/dingo/core/vocabulary/scope_tags.gen.h from cpp.scope_projections
    and the scopes: the tag of each built-in scope and its scope_traits, which
    hold its descriptor."""
    forms = sorted(spec.storage_forms, key=lambda form: form.id)
    form_ids = [form.id for form in forms]
    scopes = {scope.id: scope_fields(scope, form_ids) for scope in spec.scopes}
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
        tags.append(
            {"scope": projection.scope, "name": name, "fields": scopes[projection.scope]}
        )
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


def message_pieces(message):
    """The text of a message around its placeholders, as C++ literals."""
    pieces = [""]
    for literal, name, _, _ in string.Formatter().parse(message):
        pieces[-1] += literal
        if name is not None:
            pieces.append("")
    return [cpp_string(piece) for piece in pieces]


def diagnostics(spec):
    """include/dingo/core/vocabulary/diagnostics.gen.h and
    compile_diagnostics.gen.h from the diagnostics.

    A diagnostic reported at runtime gets a function that throws its exception
    with its message, whose placeholders are the parameters of the function.
    One reported when compiled gets a code of the diag enumeration, its text
    around its placeholders, its carrier, and the report that fail<Code,
    Subjects...>() makes: a static_assert with the formatted message where
    the compiler takes one, else with the literal message, whose carrier names
    the subjects.
    """
    from spec.schema import BindingTime

    fields = [
        {
            "id": diagnostic.id,
            "condition_lines": textwrap.wrap(diagnostic.condition, 76),
            "exception": diagnostic.exception,
            "message": cpp_string(diagnostic.message),
            "placeholders": list(diagnostic.placeholders),
            "runtime": BindingTime.RUNTIME in diagnostic.reported,
            "compile": BindingTime.COMPILE in diagnostic.reported,
            "pieces": message_pieces(diagnostic.message),
            "carrier": diagnostic.fallback_carrier,
        }
        for diagnostic in sorted(spec.diagnostics, key=lambda d: d.id)
    ]
    carriers = sorted({field["carrier"] for field in fields if field["compile"]})
    return [
        GeneratedFile(
            path="include/dingo/core/vocabulary/compile_diagnostics.gen.h",
            content=render(
                "compile_diagnostics.gen.h.j2", diagnostics=fields, carriers=carriers
            ),
            spec_ids=("diagnostics",),
        ),
        GeneratedFile(
            path="include/dingo/core/vocabulary/diagnostics.gen.h",
            content=render("diagnostics.gen.h.j2", diagnostics=fields),
            spec_ids=("diagnostics",),
        )
    ]


def check_pattern(text):
    """A FileCheck pattern of text that a compiler may spell with a space
    between closing angle brackets or before an array bound."""
    pattern = ""
    for index, letter in enumerate(text):
        if letter == ">" and index > 0 and text[index - 1] == ">":
            pattern += "{{ ?}}>"
        elif letter == "[":
            pattern += "{{ ?\\[}}"
        else:
            pattern += letter
    return pattern


def diagnostic_tests(spec):
    """test/lit/generated/diagnostics/<example>.cpp and <example>.literal.cpp
    from cpp.diagnostic_examples: the program of an example, which its
    diagnostic rejects when it is compiled, with the message formatted with
    its subjects where the compiler formats one, and literal where it does
    not."""
    diagnostics = {diagnostic.id: diagnostic for diagnostic in spec.diagnostics}
    files = []
    for example in sorted(spec.cpp.diagnostic_examples, key=lambda e: e.id):
        diagnostic = diagnostics[example.diagnostic]
        formatted = "".join(
            literal + (check_pattern(example.subjects[index]) if name is not None else "")
            for index, (literal, name, _, _) in enumerate(
                string.Formatter().parse(diagnostic.message)
            )
        )
        for tier, message in (("formatted", formatted), ("literal", diagnostic.message)):
            suffix = "" if tier == "formatted" else ".literal"
            files.append(
                GeneratedFile(
                    path=f"test/lit/generated/diagnostics/{example.id}{suffix}.cpp",
                    content=render(
                        "diagnostic_test.cpp.j2",
                        diagnostic=example.diagnostic,
                        tier=tier,
                        message=message,
                        includes=example.includes,
                        program=example.program,
                    ),
                    spec_ids=(
                        f"diagnostics.{example.diagnostic}",
                        f"cpp.diagnostic_examples.{example.id}",
                    ),
                )
            )
    return files


EMITTERS = (
    capabilities,
    variants,
    vocabulary,
    layer_kinds,
    scope_tags,
    diagnostics,
    diagnostic_tests,
    next_overlay,
)
