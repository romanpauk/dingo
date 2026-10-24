"""Emitters of the generated files.

Each emitter takes the loaded spec and returns a list of GeneratedFile. The
emitter computes every value in Python and hands the computed fields to a
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
    """include/dingo/base/capabilities.gen.h from cpp.capabilities."""
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
            path="include/dingo/base/capabilities.gen.h",
            content=content,
            spec_ids=("cpp.capabilities",),
        )
    ]


def variants(spec):
    """include/dingo/base/variants.gen.h from the language tiers and the
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
            path="include/dingo/base/variants.gen.h",
            content=render("variants.gen.h.j2", variants=fields),
            spec_ids=("variant_points",),
        )
    ]


def type_position(pattern, where):
    """Where a pattern of an operation is: "element" for a variable,
    "wrapper" for the wrapper of the semantics that has the operation, "target"
    for the wrapper of the semantics it leads to.

    Patterns are a variable or one wrapper applied to a variable; anything else
    needs the generator to grow.
    """
    if pattern.form.value == "variable":
        return "element"
    if pattern.form.value != "semantics" or len(pattern.args) != 1:
        raise GeneratorError(f"{where}: pattern is not a wrapper of one argument")
    if pattern.name == "K":
        return "wrapper"
    if pattern.name == "T":
        return "target"
    raise GeneratorError(f"{where}: pattern names the wrapper semantics {pattern.name}")


# The property bits of a wrapper semantics, in the order of Property, then whether
# it is variadic and has an extent.
PROPERTIES = (
    "owning",
    "sharing",
    "nullable",
    "borrowable",
    "in_place",
    "unsized",
    "constructible",
    "copyable",
    "immovable",
    "mapped",
    "variadic",
    "extent",
)
PROPERTY_BITS = {name: 1 << index for index, name in enumerate(PROPERTIES)}


def property_class_fields(property_class):
    """A property class as the property bits a wrapper has, and those it lacks."""
    if property_class is None:
        return None
    has = sum(PROPERTY_BITS[t.value] for t in property_class.has)
    lacks = sum(PROPERTY_BITS[t.value] for t in property_class.lacks)
    for name, value in (("variadic", property_class.variadic), ("extent", property_class.extent)):
        if value is True:
            has |= PROPERTY_BITS[name]
        elif value is False:
            lacks |= PROPERTY_BITS[name]
    return {"has": has, "lacks": lacks}


def storage_form_fields(form):
    return {
        "id": form.id,
        "outer": property_class_fields(form.outer),
        "inner": property_class_fields(form.inner),
    }


def property_names(kind):
    """The property names of a wrapper semantics, in the order of their bits."""
    names = {trait.value for trait in kind.traits}
    if kind.variadic:
        names.add("variadic")
    if kind.has_extent:
        names.add("extent")
    return [name for name in PROPERTIES if name in names]


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
        "nested_wrappers": scope.nested_wrappers,
        "needs_instance": "instance" in scope.needs,
        "exposures": [
            sum(REQUEST_CATEGORY_BITS[c.value] for c in exposed[form].categories)
            if form in exposed
            else 0
            for form in form_ids
        ],
    }


def form_fields(name, rule, of_semantics, where):
    """The fields of a operation, or of a rule of a leaf, from its template
    or its route."""
    for pattern in (rule.from_type_tree, rule.to_type_tree):
        if pattern.category is None:
            raise GeneratorError(f"{where}: pattern has no value category")
    return {
        "id": name,
        "of_semantics": of_semantics,
        "from_position": type_position(rule.from_type_tree, where),
        "from_category": rule.from_type_tree.category.value,
        "to_position": type_position(rule.to_type_tree, where),
        "to_category": rule.to_type_tree.category.value,
        "access": rule.access.value,
        "exposure": REQUEST_CATEGORY_BITS[rule.exposure.value] if rule.exposure else 0,
        "requires": sum(REQUIREMENT_BITS[r.predicate.value] for r in rule.requires),
        "upcast": rule.via is not None and rule.via.value == "upcast",
        "converts": rule.via is not None and rule.via.value == "convert",
        "cached": rule.cache is not None,
        "semantics_properties": sum(
            PROPERTY_BITS[t.value] for t in getattr(rule, "kind_traits", ())
        ),
    }


def operation_entries(spec):
    """The operations and the rules of a leaf in the order of the rules,
    the order of their first routes."""
    templates = {template.form: template for template in spec.operations}
    entries, seen = [], set()
    for route in spec.rules:
        key = route.form if route.form is not None else route.id
        if key in seen:
            continue
        seen.add(key)
        if route.form is None:
            entries.append(form_fields(route.id, route, False, f"routes.{route.id}"))
        else:
            template = templates[route.form]
            entries.append(
                form_fields(route.form.value, template, True, f"operations.{template.id}")
            )
    for form, template in templates.items():
        if form not in seen:
            entries.append(form_fields(form.value, template, True, f"operations.{template.id}"))
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


def operations(spec):
    """include/dingo/conversion/operations.gen.h from the wrapper semantics and the
    operations."""
    from spec.schema import Access, ValueCategory

    content = render(
        "operations.gen.h.j2",
        properties=[
            {"name": name, "bit": bit} for name, bit in PROPERTY_BITS.items()
        ],
        value_categories=[c.value for c in ValueCategory],
        accesses=[a.value for a in Access],
        requirements=[{"name": name, "bit": bit} for name, bit in REQUIREMENT_BITS.items()],
        operations=operation_entries(spec),
    )
    return [
        GeneratedFile(
            path="include/dingo/conversion/operations.gen.h",
            content=content,
            spec_ids=("wrapper_semantics", "operations", "routes"),
        )
    ]


def storage_forms(spec):
    """include/dingo/conversion/storage_forms.gen.h from the storage forms."""
    forms = sorted(spec.storage_forms, key=lambda f: f.id)
    return [
        GeneratedFile(
            path="include/dingo/conversion/storage_forms.gen.h",
            content=render(
                "storage_forms.gen.h.j2",
                request_categories=[
                    {"name": name, "bit": bit}
                    for name, bit in REQUEST_CATEGORY_BITS.items()
                ],
                storage_forms=[storage_form_fields(form) for form in forms],
            ),
            spec_ids=("storage_forms",),
        )
    ]


def selection_rules(spec):
    """include/dingo/conversion/selection_rules.gen.h from the selection
    rules."""
    from spec.schema import SelectionMode, SelectionSource, StepCondition

    return [
        GeneratedFile(
            path="include/dingo/conversion/selection_rules.gen.h",
            content=render(
                "selection_rules.gen.h.j2",
                selection_sources=[source.value for source in SelectionSource],
                selection_modes=[mode.value for mode in SelectionMode],
                step_conditions=[
                    "always",
                    *(condition.value for condition in StepCondition),
                ],
                selection_rules=[selection_fields(rule) for rule in spec.selection_rules],
                max_selection_steps=max(
                    (len(r.steps) for r in spec.selection_rules), default=1
                ),
                max_step_sources=max(
                    (len(step.sources) for r in spec.selection_rules for step in r.steps),
                    default=1,
                ),
            ),
            spec_ids=("selection_rules",),
        )
    ]


def key_kinds(spec):
    """include/dingo/conversion/key_kinds.gen.h from the key kinds."""
    return [
        GeneratedFile(
            path="include/dingo/conversion/key_kinds.gen.h",
            content=render(
                "key_kinds.gen.h.j2",
                key_kinds=[
                    {"id": kind.id, "has_value": kind.value_time is not None}
                    for kind in spec.key_kinds
                ],
            ),
            spec_ids=("key_kinds",),
        )
    ]


def wrapper_semantics(spec):
    """include/dingo/conversion/wrapper_semantics.gen.h from the wrapper semantics
    and cpp.semantics_tags: the tag of each wrapper semantics of the library and
    its wrapper_semantics, the declaration the semantics of a user make too."""
    spellings = {p.semantics: p.spelling for p in spec.cpp.semantics_tags}
    namespaces = {spelling.rpartition("::")[0] for spelling in spellings.values()}
    if len(namespaces) != 1:
        raise GeneratorError("cpp.semantics_tags: tags span several namespaces")
    names = {key: spelling.rpartition("::")[2] for key, spelling in spellings.items()}
    semantics_list = [
        {
            "id": semantics.id,
            "name": names[semantics.id],
            "properties": property_names(semantics),
            "operations": [
                {"operation": operation.form.value, "target": names.get(operation.target)}
                for operation in semantics.forms
            ],
        }
        for semantics in spec.wrapper_semantics
    ]
    return [
        GeneratedFile(
            path="include/dingo/conversion/wrapper_semantics.gen.h",
            content=render(
                "wrapper_semantics.gen.h.j2",
                namespace=namespaces.pop(),
                semantics_list=semantics_list,
            ),
            spec_ids=("cpp.semantics_tags", "wrapper_semantics"),
        )
    ]


def scopes(spec):
    """include/dingo/conversion/scopes.gen.h from the scopes and
    cpp.scope_tags: the descriptor of a scope, and the tag of each
    built-in scope with its scope_traits, which hold its descriptor."""
    from spec.schema import ExposureRule, RecursionGuard, Slot

    forms = sorted(spec.storage_forms, key=lambda form: form.id)
    form_ids = [form.id for form in forms]
    scope_data = {scope.id: scope_fields(scope, form_ids) for scope in spec.scopes}
    tags = []
    namespaces = set()
    for scope_tag in sorted(spec.cpp.scope_tags, key=lambda p: p.scope):
        namespace, _, name = scope_tag.spelling.rpartition("::")
        if not namespace or not name:
            raise GeneratorError(
                f"cpp.scope_tags.{scope_tag.id}: {scope_tag.spelling} "
                "is not a qualified name"
            )
        namespaces.add(namespace)
        tags.append(
            {"scope": scope_tag.scope, "name": name, "fields": scope_data[scope_tag.scope]}
        )
    if len(namespaces) != 1:
        raise GeneratorError("cpp.scope_tags: tags span several namespaces")
    return [
        GeneratedFile(
            path="include/dingo/conversion/scopes.gen.h",
            content=render(
                "scopes.gen.h.j2",
                namespace=namespaces.pop(),
                tags=tags,
                slots=[s.value for s in Slot],
                guards=[g.value for g in RecursionGuard],
                exposure_rules=[r.value for r in ExposureRule],
            ),
            spec_ids=("scopes", "cpp.scope_tags"),
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
    """include/dingo/diagnostics/diagnostics.gen.h and
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
            path="include/dingo/diagnostics/compile_diagnostics.gen.h",
            content=render(
                "compile_diagnostics.gen.h.j2", diagnostics=fields, carriers=carriers
            ),
            spec_ids=("diagnostics",),
        ),
        GeneratedFile(
            path="include/dingo/diagnostics/diagnostics.gen.h",
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
    operations,
    storage_forms,
    scopes,
    selection_rules,
    key_kinds,
    wrapper_semantics,
    diagnostics,
    diagnostic_tests,
)
