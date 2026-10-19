"""Validation of the spec.

`validate` returns a list of error strings, empty when the spec is valid. Each
error starts with the section and the entity id it concerns. The checks are:

- ids are snake_case and unique within their section
- references between sections resolve
- shape patterns use known layer kinds with the right number of arguments,
  and requirements name pattern variables
- scope inheritance has no cycle
- diagnostic message placeholders match the declared placeholders
- contract bounds carry the fields their kind needs and match the dimension
- capability detection is well-formed: exactly one of a feature macro with a
  minimum or a builtin, and a header only with a feature macro
"""

import dataclasses
import re
import string
from collections.abc import Iterable, Iterator

from spec.schema import (
    Bound,
    BoundKind,
    Capability,
    Cardinality,
    ContractDimension,
    CppProjection,
    SelectionMode,
    ShapePattern,
    PatternForm,
    Spec,
    VariantKind,
)

SNAKE_CASE = re.compile(r"^[a-z][a-z0-9]*(_[a-z0-9]+)*$")
PATTERN_VARIABLE = re.compile(r"^[A-Z][A-Za-z0-9]*$")
MACRO_NAME = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
HEADER_NAME = re.compile(r"^[A-Za-z0-9_][A-Za-z0-9_./-]*$")

INLINING_BOUNDS = {BoundKind.ALWAYS, BoundKind.NEVER, BoundKind.COLD}


def sections(spec: Spec) -> Iterator[tuple[str, tuple]]:
    """Yields (section name, entities) for every section of the spec."""
    for field in dataclasses.fields(Spec):
        if field.name != "cpp":
            yield field.name, getattr(spec, field.name)
    for field in dataclasses.fields(CppProjection):
        yield f"cpp.{field.name}", getattr(spec.cpp, field.name)


def ids(entities: Iterable) -> set[str]:
    return {entity.id for entity in entities}


def validate(spec: Spec) -> list[str]:
    """Returns the errors of the spec, in a deterministic order."""
    errors: list[str] = []
    check_ids(spec, errors)
    check_scopes(spec, errors)
    check_layer_kinds(spec, errors)
    check_routes(spec, errors)
    check_lookups(spec, errors)
    check_selection_rules(spec, errors)
    check_graph_rules(spec, errors)
    check_diagnostics(spec, errors)
    check_binding_time_constraints(spec, errors)
    check_variant_points(spec, errors)
    check_contracts(spec, errors)
    check_behavior_changes(spec, errors)
    check_cpp(spec, errors)
    return errors


def check_ids(spec: Spec, errors: list[str]) -> None:
    for section, entities in sections(spec):
        seen: set[str] = set()
        for entity in entities:
            if not isinstance(entity.id, str) or not SNAKE_CASE.match(entity.id):
                errors.append(f"{section}: id {entity.id!r} is not snake_case")
            if entity.id in seen:
                errors.append(f"{section}: duplicate id {entity.id!r}")
            seen.add(entity.id)


def require_ref(
    errors: list[str], where: str, what: str, value: str, known: set[str]
) -> None:
    if value not in known:
        errors.append(f"{where}: {what} {value!r} does not exist")


def check_unique(errors: list[str], where: str, what: str, values: Iterable) -> None:
    values = list(values)
    if len(values) != len(set(values)):
        errors.append(f"{where}: {what} has duplicates")


def check_scopes(spec: Spec, errors: list[str]) -> None:
    scope_ids = ids(spec.scopes)
    parents = {scope.id: scope.extends for scope in spec.scopes}
    for scope in spec.scopes:
        where = f"scopes.{scope.id}"
        if scope.extends is not None:
            require_ref(errors, where, "extends scope", scope.extends, scope_ids)
        seen = {scope.id}
        parent = scope.extends
        while parent is not None and parent in parents:
            if parent in seen:
                errors.append(f"{where}: extends cycle through {parent!r}")
                break
            seen.add(parent)
            parent = parents[parent]
        for what, values in (("phases", scope.phases), ("needs", scope.needs)):
            check_unique(errors, where, what, values)
            for value in values:
                if not SNAKE_CASE.match(value):
                    errors.append(f"{where}: {what} entry {value!r} is not snake_case")


def check_layer_kinds(spec: Spec, errors: list[str]) -> None:
    for kind in spec.layer_kinds:
        where = f"layer_kinds.{kind.id}"
        if kind.parameters < 0:
            errors.append(f"{where}: parameters is negative")
        if kind.variadic and kind.parameters < 1:
            errors.append(f"{where}: variadic needs at least one parameter")
        check_unique(errors, where, "traits", kind.traits)


def pattern_variables(pattern: ShapePattern) -> Iterator[str]:
    if pattern.form in (PatternForm.VARIABLE, PatternForm.WRAPPER) and pattern.name:
        yield pattern.name
    for arg in pattern.args:
        yield from pattern_variables(arg)


def check_pattern(
    pattern: ShapePattern, where: str, layers: dict[str, object], errors: list[str]
) -> None:
    form = pattern.form
    if form is PatternForm.LAYER:
        layer = layers.get(pattern.name)
        if layer is None:
            errors.append(f"{where}: layer kind {pattern.name!r} does not exist")
        elif layer.variadic and len(pattern.args) < layer.parameters:
            errors.append(
                f"{where}: {pattern.name} takes at least {layer.parameters} arguments"
            )
        elif not layer.variadic and len(pattern.args) != layer.parameters:
            errors.append(f"{where}: {pattern.name} takes {layer.parameters} arguments")
    elif form is PatternForm.VARIABLE:
        if not (pattern.name and PATTERN_VARIABLE.match(pattern.name)):
            errors.append(f"{where}: pattern variable {pattern.name!r} is malformed")
        if pattern.args:
            errors.append(f"{where}: pattern variable {pattern.name!r} has arguments")
    elif form is PatternForm.WRAPPER:
        if not (pattern.name and PATTERN_VARIABLE.match(pattern.name)):
            errors.append(f"{where}: wrapper variable {pattern.name!r} is malformed")
        if len(pattern.args) != 1:
            errors.append(f"{where}: wrapper {pattern.name!r} takes one argument")
    elif form is PatternForm.ROUTE:
        if pattern.name is not None:
            errors.append(f"{where}: route pattern has a name")
        if len(pattern.args) != 1:
            errors.append(f"{where}: route pattern takes one argument")
    for arg in pattern.args:
        check_pattern(arg, where, layers, errors)


def check_routes(spec: Spec, errors: list[str]) -> None:
    layers = {kind.id: kind for kind in spec.layer_kinds}
    for route in spec.routes:
        where = f"routes.{route.id}"
        patterns = [route.from_shape, route.to_shape]
        if route.cache is not None:
            patterns.append(route.cache)
        for pattern in patterns:
            check_pattern(pattern, where, layers, errors)
        bound = set(pattern_variables(route.from_shape)) | set(
            pattern_variables(route.to_shape)
        )
        for requirement in (*route.requires, *route.where):
            if requirement.subject not in bound:
                errors.append(
                    f"{where}: requirement subject {requirement.subject!r} is not a pattern variable"
                )


def check_lookups(spec: Spec, errors: list[str]) -> None:
    filter_ids = ids(spec.lookup_filters)
    key_ids = ids(spec.key_kinds)
    for lookup_filter in spec.lookup_filters:
        where = f"lookup_filters.{lookup_filter.id}"
        if not lookup_filter.dimensions:
            errors.append(f"{where}: no dimensions")
        check_unique(errors, where, "dimensions", lookup_filter.dimensions)
    for index in spec.index_structures:
        where = f"index_structures.{index.id}"
        require_ref(errors, where, "filter", index.filter, filter_ids)
        if not index.key_kinds:
            errors.append(f"{where}: no key kinds")
        check_unique(errors, where, "key_kinds", index.key_kinds)
        for key in index.key_kinds:
            require_ref(errors, where, "key kind", key, key_ids)
        check_unique(errors, where, "declarations", index.declarations)
        for declaration in index.declarations:
            if not SNAKE_CASE.match(declaration):
                errors.append(f"{where}: declaration {declaration!r} is not snake_case")


def check_selection_rules(spec: Spec, errors: list[str]) -> None:
    combine_of_mode = {
        SelectionMode.SINGULAR: "first_non_empty",
        SelectionMode.COLLECTION: "concat",
    }
    for rule in spec.selection_rules:
        where = f"selection_rules.{rule.id}"
        if not rule.steps:
            errors.append(f"{where}: no steps")
        for step in rule.steps:
            if not step.sources:
                errors.append(f"{where}: a step has no sources")
        if rule.combine.value != combine_of_mode[rule.mode]:
            errors.append(
                f"{where}: {rule.mode.value} requests combine with {combine_of_mode[rule.mode]}"
            )
        counts = [outcome.count for outcome in rule.outcomes]
        check_unique(errors, where, "outcome counts", counts)
        if set(counts) != set(Cardinality):
            errors.append(f"{where}: outcomes must cover empty, one, and many")


def check_graph_rules(spec: Spec, errors: list[str]) -> None:
    scope_ids = ids(spec.scopes)
    diagnostic_ids = ids(spec.diagnostics)
    for rule in spec.graph_rules:
        where = f"graph_rules.{rule.id}"
        if not rule.legal_scopes:
            errors.append(f"{where}: no legal scopes")
        for scope in rule.legal_scopes:
            require_ref(errors, where, "scope", scope, scope_ids)
        if not rule.checked_at:
            errors.append(f"{where}: no check times")
        check_unique(errors, where, "checked_at", rule.checked_at)
        require_ref(errors, where, "diagnostic", rule.diagnostic, diagnostic_ids)


def message_fields(message: str) -> list[str] | str:
    """Returns the placeholder names of a template, or an error text."""
    names = []
    try:
        for _, name, format_spec, conversion in string.Formatter().parse(message):
            if name is None:
                continue
            if format_spec or conversion:
                return f"placeholder {name!r} has a conversion or format"
            names.append(name)
    except ValueError as error:
        return f"message is malformed: {error}"
    return names


def check_diagnostics(spec: Spec, errors: list[str]) -> None:
    for diagnostic in spec.diagnostics:
        where = f"diagnostics.{diagnostic.id}"
        for what in ("condition", "exception", "message", "fallback_carrier"):
            if not getattr(diagnostic, what).strip():
                errors.append(f"{where}: {what} is empty")
        check_unique(errors, where, "placeholders", diagnostic.placeholders)
        for name in diagnostic.placeholders:
            if not SNAKE_CASE.match(name):
                errors.append(f"{where}: placeholder {name!r} is not snake_case")
        used = message_fields(diagnostic.message)
        if isinstance(used, str):
            errors.append(f"{where}: {used}")
            continue
        for name in sorted(set(used) - set(diagnostic.placeholders)):
            errors.append(f"{where}: message uses undeclared placeholder {name!r}")
        for name in sorted(set(diagnostic.placeholders) - set(used)):
            errors.append(f"{where}: placeholder {name!r} is not used by the message")


def check_binding_time_constraints(spec: Spec, errors: list[str]) -> None:
    for constraint in spec.binding_time_constraints:
        where = f"binding_time_constraints.{constraint.id}"
        for what in ("feature", "justification"):
            if not getattr(constraint, what).strip():
                errors.append(f"{where}: {what} is empty")


def check_variant_points(spec: Spec, errors: list[str]) -> None:
    capability_ids = ids(spec.cpp.capabilities)
    index_ids = ids(spec.index_structures)
    for point in spec.variant_points:
        where = f"variant_points.{point.id}"
        if not point.justification.strip():
            errors.append(f"{where}: justification is empty")
        wants_capability = point.kind is VariantKind.LANGUAGE_TIER
        wants_index = point.kind is VariantKind.INDEX_STRUCTURE
        if wants_capability and point.capability is None:
            errors.append(f"{where}: a language tier names a capability")
        if not wants_capability and point.capability is not None:
            errors.append(f"{where}: only a language tier names a capability")
        if wants_index and point.index_structure is None:
            errors.append(f"{where}: an index structure variant names an index structure")
        if not wants_index and point.index_structure is not None:
            errors.append(f"{where}: only an index structure variant names an index structure")
        if point.capability is not None:
            require_ref(errors, where, "capability", point.capability, capability_ids)
        if point.index_structure is not None:
            require_ref(errors, where, "index structure", point.index_structure, index_ids)


def check_bound(bound: Bound, where: str, errors: list[str]) -> None:
    kind = bound.kind
    needs_value = kind in (BoundKind.AT_MOST, BoundKind.EXACTLY)
    if needs_value and (bound.value is None or bound.value < 0):
        errors.append(f"{where}: {kind.value} needs a non-negative value")
    if not needs_value and bound.value is not None:
        errors.append(f"{where}: {kind.value} takes no value")
    if kind is BoundKind.LINEAR and not bound.variable:
        errors.append(f"{where}: linear names its variable")
    if kind is not BoundKind.LINEAR and bound.variable is not None:
        errors.append(f"{where}: {kind.value} takes no variable")
    if bound.unit is not None and not needs_value:
        errors.append(f"{where}: {kind.value} takes no unit")


def check_contracts(spec: Spec, errors: list[str]) -> None:
    for contract in spec.contracts:
        where = f"contracts.{contract.id}"
        if not contract.operation.strip():
            errors.append(f"{where}: operation is empty")
        check_bound(contract.bound, where, errors)
        is_inlining = contract.dimension is ContractDimension.INLINING
        if is_inlining != (contract.bound.kind in INLINING_BOUNDS):
            errors.append(
                f"{where}: {contract.bound.kind.value} does not fit {contract.dimension.value}"
            )


def check_behavior_changes(spec: Spec, errors: list[str]) -> None:
    for change in spec.behavior_changes:
        where = f"behavior_changes.{change.id}"
        if not change.cases or not all(case.strip() for case in change.cases):
            errors.append(f"{where}: cases is not a non-empty list of patterns")
        for what in ("description", "baseline", "now", "decision"):
            if not getattr(change, what).strip():
                errors.append(f"{where}: {what} is empty")


def check_capability(capability: Capability, errors: list[str]) -> None:
    where = f"cpp.capabilities.{capability.id}"
    if not capability.description.strip():
        errors.append(f"{where}: description is empty")
    has_macro = capability.feature_macro is not None
    has_minimum = capability.minimum is not None
    has_builtin = capability.builtin is not None
    if has_macro != has_minimum:
        errors.append(f"{where}: feature_macro and minimum are set together")
    if has_macro and has_builtin:
        errors.append(f"{where}: detected by a feature macro or a builtin, not both")
    if not has_macro and not has_builtin:
        errors.append(f"{where}: no detection, set feature_macro with minimum or builtin")
    if has_macro and not MACRO_NAME.match(capability.feature_macro):
        errors.append(f"{where}: feature_macro {capability.feature_macro!r} is malformed")
    if has_minimum and (
        isinstance(capability.minimum, bool)
        or not isinstance(capability.minimum, int)
        or capability.minimum <= 0
    ):
        errors.append(f"{where}: minimum must be a positive integer")
    if has_builtin and not MACRO_NAME.match(capability.builtin):
        errors.append(f"{where}: builtin {capability.builtin!r} is malformed")
    if capability.header is not None:
        if not has_macro:
            errors.append(f"{where}: header needs a feature_macro")
        if not HEADER_NAME.match(capability.header):
            errors.append(f"{where}: header {capability.header!r} is malformed")


def check_cpp(spec: Spec, errors: list[str]) -> None:
    for capability in spec.cpp.capabilities:
        check_capability(capability, errors)
    layer_ids = ids(spec.layer_kinds)
    api_ids = ids(spec.public_api)
    for projection in spec.cpp.shape_projections:
        where = f"cpp.shape_projections.{projection.id}"
        require_ref(errors, where, "layer kind", projection.layer, layer_ids)
        if not projection.spelling.strip():
            errors.append(f"{where}: spelling is empty")
        check_unique(errors, where, "headers", projection.headers)
    check_unique(
        errors,
        "cpp.shape_projections",
        "layer kinds",
        (projection.layer for projection in spec.cpp.shape_projections),
    )
    for binding in spec.cpp.api_bindings:
        where = f"cpp.api_bindings.{binding.id}"
        require_ref(errors, where, "public API entry", binding.api, api_ids)
        if not binding.spelling.strip():
            errors.append(f"{where}: spelling is empty")
    check_unique(
        errors,
        "cpp.api_bindings",
        "public API entries",
        (binding.api for binding in spec.cpp.api_bindings),
    )
