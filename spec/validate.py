"""Validation of the spec.

`validate` returns a list of error strings, empty when the spec is valid. Each
error starts with the section and the entity id it concerns. The checks are:

- ids are snake_case and unique within their section
- references between sections resolve
- shape patterns use known layer kinds with the right number of arguments,
  and requirements name pattern variables
- scope inheritance has no cycle
- diagnostic message placeholders match the declared placeholders
- contract bounds carry the fields their kind needs and match the dimension,
  and every contract has a description
- contract probes name a contract and either metric patterns or the reason the
  contract is not observable
- every public API entry has exactly one C++ binding, bound with the fields
  its kind needs, and a name's header is a public include path
- capability detection is well-formed: exactly one of a feature macro with a
  minimum or a builtin, and a header only with a feature macro
"""

import dataclasses
import re
import string
from collections.abc import Iterable, Iterator

from spec.schema import (
    ApiKind,
    BindingTime,
    Bound,
    BoundKind,
    Capability,
    Cardinality,
    Combine,
    ContractDimension,
    CppProjection,
    SelectionMode,
    SelectionSource,
    ShapePattern,
    PatternForm,
    Spec,
    StepCondition,
    VariantKind,
)

SNAKE_CASE = re.compile(r"^[a-z][a-z0-9]*(_[a-z0-9]+)*$")
IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
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
    check_layer_kinds(spec, errors)
    check_storage_forms(spec, errors)
    check_scopes(spec, errors)
    check_routes(spec, errors)
    check_lookups(spec, errors)
    check_selection_rules(spec, errors)
    check_selection_sources(spec, errors)
    check_transaction_rules(spec, errors)
    check_graph_rules(spec, errors)
    check_diagnostics(spec, errors)
    check_diagnostic_examples(spec, errors)
    check_binding_time_constraints(spec, errors)
    check_variant_points(spec, errors)
    check_contracts(spec, errors)
    check_contract_probes(spec, errors)
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


def check_storage_forms(spec: Spec, errors: list[str]) -> None:
    layers = {kind.id: kind for kind in spec.layer_kinds}
    for storage_form in spec.storage_forms:
        where = f"storage_forms.{storage_form.id}"
        check_pattern(storage_form.pattern, where, layers, errors)
    check_unique(
        errors,
        "storage_forms",
        "patterns",
        (storage_form.pattern for storage_form in spec.storage_forms),
    )


def check_scopes(spec: Spec, errors: list[str]) -> None:
    scope_ids = ids(spec.scopes)
    form_ids = ids(spec.storage_forms)
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
        if not scope.exposures:
            errors.append(f"{where}: exposes no storage form")
        check_unique(
            errors, where, "exposed forms", (e.form for e in scope.exposures)
        )
        for exposure in scope.exposures:
            require_ref(errors, where, "storage form", exposure.form, form_ids)
            if not exposure.categories:
                errors.append(f"{where}: form {exposure.form!r} exposes no category")
            check_unique(
                errors, where, f"categories of {exposure.form}", exposure.categories
            )
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
    declaration_ids = ids(spec.lookup_declarations)
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
            require_ref(errors, where, "declaration", declaration, declaration_ids)
    defaults = [d.id for d in spec.lookup_declarations if d.default]
    if spec.lookup_declarations and len(defaults) != 1:
        errors.append(f"lookup_declarations: exactly one is the default, not {defaults}")
    for declaration in spec.lookup_declarations:
        where = f"lookup_declarations.{declaration.id}"
        if not declaration.key_kinds:
            errors.append(f"{where}: no key kinds")
        check_unique(errors, where, "key_kinds", declaration.key_kinds)
        for key in declaration.key_kinds:
            require_ref(errors, where, "key kind", key, key_ids)
        check_unique(errors, where, "cardinalities", declaration.cardinalities)
        if Cardinality.EMPTY in declaration.cardinalities or not declaration.cardinalities:
            errors.append(f"{where}: cardinalities are one or many")
        if declaration.default_cardinality not in declaration.cardinalities:
            errors.append(f"{where}: the default cardinality is not allowed")


def check_selection_rules(spec: Spec, errors: list[str]) -> None:
    for rule in spec.selection_rules:
        where = f"selection_rules.{rule.id}"
        if not rule.steps:
            errors.append(f"{where}: no steps")
        for step in rule.steps:
            if not step.sources:
                errors.append(f"{where}: a step has no sources")
        if (
            rule.mode in (SelectionMode.SINGULAR, SelectionMode.CONSTRUCT)
            and rule.combine is not Combine.FIRST_NON_EMPTY
        ):
            errors.append(f"{where}: {rule.mode.value} requests combine with first_non_empty")
        for step in rule.steps:
            if SelectionSource.ELEMENTS in step.sources and (
                rule.mode is not SelectionMode.SINGULAR
                or step.when is not StepCondition.COLLECTION
            ):
                errors.append(
                    f"{where}: the elements are a source of singular requests of "
                    "a collection type"
                )
            if SelectionSource.FACTORY in step.sources and (
                rule.mode is not SelectionMode.CONSTRUCT
                or step.when is not StepCondition.HAS_FACTORY
            ):
                errors.append(
                    f"{where}: a factory is a source of construct requests, "
                    "taken when the request has a factory"
                )
        modes = [r.mode for r in spec.selection_rules]
        if modes.count(rule.mode) > 1:
            errors.append(f"{where}: one rule per mode")
        counts = [outcome.count for outcome in rule.outcomes]
        check_unique(errors, where, "outcome counts", counts)
        if set(counts) != set(Cardinality):
            errors.append(f"{where}: outcomes must cover empty, one, and many")


def check_selection_sources(spec: Spec, errors: list[str]) -> None:
    sources = [entry.source for entry in spec.selection_sources]
    check_unique(errors, "selection_sources", "sources", sources)
    for source in SelectionSource:
        if source not in sources:
            errors.append(f"selection_sources: no time of {source.value}")


def check_transaction_rules(spec: Spec, errors: list[str]) -> None:
    for rule in spec.transaction_rules:
        where = f"transaction_rules.{rule.id}"
        if not rule.operations:
            errors.append(f"{where}: no operations")
        if not rule.undoes:
            errors.append(f"{where}: undoes no effect")
        check_unique(errors, where, "operations", rule.operations)
        check_unique(errors, where, "undoes", rule.undoes)


def check_graph_rules(spec: Spec, errors: list[str]) -> None:
    diagnostic_ids = ids(spec.diagnostics)
    for rule in spec.graph_rules:
        where = f"graph_rules.{rule.id}"
        if not rule.legal_slots:
            errors.append(f"{where}: no legal slots")
        check_unique(errors, where, "legal_slots", rule.legal_slots)
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
        for what in ("condition", "message", "fallback_carrier"):
            if not getattr(diagnostic, what).strip():
                errors.append(f"{where}: {what} is empty")
        if not diagnostic.reported:
            errors.append(f"{where}: reported at no time")
        check_unique(errors, where, "reported", diagnostic.reported)
        runtime = BindingTime.RUNTIME in diagnostic.reported
        if runtime != bool(diagnostic.exception and diagnostic.exception.strip()):
            errors.append(
                f"{where}: a diagnostic has an exception if and only if it is "
                "reported at runtime"
            )
        if not IDENTIFIER.match(diagnostic.fallback_carrier):
            errors.append(f"{where}: fallback_carrier is not an identifier")
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
        if BindingTime.COMPILE in diagnostic.reported and used != list(
            diagnostic.placeholders
        ):
            errors.append(
                f"{where}: a diagnostic reported when compiled uses each "
                "placeholder once, in the declared order"
            )


def check_diagnostic_examples(spec: Spec, errors: list[str]) -> None:
    diagnostics = {diagnostic.id: diagnostic for diagnostic in spec.diagnostics}
    examples = {}
    for example in spec.cpp.diagnostic_examples:
        where = f"cpp.diagnostic_examples.{example.id}"
        diagnostic = diagnostics.get(example.diagnostic)
        if diagnostic is None:
            errors.append(f"{where}: unknown diagnostic {example.diagnostic!r}")
            continue
        examples.setdefault(example.diagnostic, []).append(example)
        if BindingTime.COMPILE not in diagnostic.reported:
            errors.append(f"{where}: {example.diagnostic} is not reported when compiled")
        if len(example.subjects) != len(diagnostic.placeholders):
            errors.append(f"{where}: one subject per placeholder of {example.diagnostic}")
        if not example.program.strip():
            errors.append(f"{where}: program is empty")
    for diagnostic in spec.diagnostics:
        if BindingTime.COMPILE in diagnostic.reported and diagnostic.id not in examples:
            errors.append(
                f"diagnostics.{diagnostic.id}: reported when compiled, with no "
                "cpp.diagnostic_examples entry"
            )


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
    takes_value = needs_value or kind is BoundKind.LINEAR
    takes_unit = takes_value or kind is BoundKind.RECORDED
    if needs_value and (bound.value is None or bound.value < 0):
        errors.append(f"{where}: {kind.value} needs a non-negative value")
    if kind is BoundKind.LINEAR and bound.value is not None and bound.value < 0:
        errors.append(f"{where}: linear takes a non-negative value")
    if not takes_value and bound.value is not None:
        errors.append(f"{where}: {kind.value} takes no value")
    if kind is BoundKind.LINEAR and not bound.variable:
        errors.append(f"{where}: linear names its variable")
    if kind is BoundKind.LINEAR and bound.variable and not SNAKE_CASE.match(bound.variable):
        errors.append(f"{where}: variable {bound.variable!r} is not snake_case")
    if kind is not BoundKind.LINEAR and bound.variable is not None:
        errors.append(f"{where}: {kind.value} takes no variable")
    if bound.unit is not None and not takes_unit:
        errors.append(f"{where}: {kind.value} takes no unit")
    if bound.unit is not None and not SNAKE_CASE.match(bound.unit):
        errors.append(f"{where}: unit {bound.unit!r} is not snake_case")


def check_contracts(spec: Spec, errors: list[str]) -> None:
    for contract in spec.contracts:
        where = f"contracts.{contract.id}"
        for what in ("description", "operation"):
            if not getattr(contract, what).strip():
                errors.append(f"{where}: {what} is empty")
        check_bound(contract.bound, where, errors)
        is_inlining = contract.dimension is ContractDimension.INLINING
        if is_inlining != (contract.bound.kind in INLINING_BOUNDS):
            errors.append(
                f"{where}: {contract.bound.kind.value} does not fit {contract.dimension.value}"
            )
        if contract.construct is not None:
            if contract.dimension is not ContractDimension.COMPILE_COST:
                errors.append(f"{where}: only a compile cost contract names a construct")
            if not SNAKE_CASE.match(contract.construct):
                errors.append(f"{where}: construct {contract.construct!r} is not snake_case")


def check_contract_probes(spec: Spec, errors: list[str]) -> None:
    contract_ids = ids(spec.contracts)
    for probe in spec.cpp.contract_probes:
        where = f"cpp.contract_probes.{probe.id}"
        require_ref(errors, where, "contract", probe.contract, contract_ids)
        observable = bool(probe.metrics)
        if observable == (probe.not_observable is not None):
            errors.append(f"{where}: set metrics or not_observable, exactly one")
        if probe.not_observable is not None and not probe.not_observable.strip():
            errors.append(f"{where}: not_observable gives no reason")
        if probe.excluding and not observable:
            errors.append(f"{where}: excluding needs metrics")
        check_unique(errors, where, "metrics", probe.metrics)
        for pattern in (*probe.metrics, *probe.excluding):
            if not pattern.strip() or pattern != pattern.strip() or "/" not in pattern:
                errors.append(f"{where}: metric pattern {pattern!r} is not a metric path")
    check_unique(
        errors,
        "cpp.contract_probes",
        "contracts",
        (probe.contract for probe in spec.cpp.contract_probes),
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


def check_api_bindings(spec: Spec, errors: list[str]) -> None:
    """Every public API entry has exactly one C++ binding, and vice versa.

    An include path binds a header name and a configuration macro a macro
    name. A name or extension point binds its spelling, the public include
    path that makes it available, and the usage the public API check compiles.
    """
    kinds = {entry.id: entry.kind for entry in spec.public_api}
    for entry in spec.public_api:
        if not entry.description.strip():
            errors.append(f"public_api.{entry.id}: description is empty")
    bindings = {binding.api: binding for binding in spec.cpp.api_bindings}
    for entry in spec.public_api:
        if entry.id not in bindings:
            errors.append(f"public_api.{entry.id}: has no C++ binding")
    check_unique(
        errors,
        "cpp.api_bindings",
        "public API entries",
        (binding.api for binding in spec.cpp.api_bindings),
    )
    include_paths = {
        binding.spelling
        for binding in spec.cpp.api_bindings
        if kinds.get(binding.api) is ApiKind.INCLUDE_PATH
    }
    for binding in spec.cpp.api_bindings:
        where = f"cpp.api_bindings.{binding.id}"
        require_ref(errors, where, "public API entry", binding.api, set(kinds))
        if not binding.spelling.strip():
            errors.append(f"{where}: spelling is empty")
        for what in ("header", "declarations", "usage"):
            value = getattr(binding, what)
            if value is not None and not value.strip():
                errors.append(f"{where}: {what} is empty")
        kind = kinds.get(binding.api)
        carries_code = kind in (ApiKind.NAME, ApiKind.EXTENSION_POINT)
        if kind is ApiKind.INCLUDE_PATH and not HEADER_NAME.match(binding.spelling):
            errors.append(f"{where}: include path {binding.spelling!r} is malformed")
        if kind is ApiKind.CONFIGURATION_MACRO and not MACRO_NAME.match(
            binding.spelling
        ):
            errors.append(f"{where}: macro {binding.spelling!r} is malformed")
        if kind is not None and not carries_code:
            for what in ("header", "declarations", "usage"):
                if getattr(binding, what) is not None:
                    errors.append(f"{where}: only a name or extension point has {what}")
        if carries_code:
            if binding.usage is None:
                errors.append(f"{where}: a name or extension point needs usage")
            elif "{name}" not in f"{binding.declarations or ''}{binding.usage}":
                errors.append(f"{where}: declarations or usage must use {{name}}")
            if binding.header is None:
                errors.append(f"{where}: a name or extension point needs a header")
            elif binding.header not in include_paths:
                errors.append(
                    f"{where}: header {binding.header!r} is not a public include path"
                )


def check_cpp(spec: Spec, errors: list[str]) -> None:
    for capability in spec.cpp.capabilities:
        check_capability(capability, errors)
    scope_ids = ids(spec.scopes)
    for projection in spec.cpp.scope_projections:
        where = f"cpp.scope_projections.{projection.id}"
        require_ref(errors, where, "scope", projection.scope, scope_ids)
        if not projection.spelling.strip():
            errors.append(f"{where}: spelling is empty")
        if not HEADER_NAME.match(projection.header):
            errors.append(f"{where}: header {projection.header!r} is malformed")
    projected = [projection.scope for projection in spec.cpp.scope_projections]
    check_unique(errors, "cpp.scope_projections", "scopes", projected)
    for scope in spec.scopes:
        if scope.id not in projected:
            errors.append(f"scopes.{scope.id}: has no C++ scope projection")
    check_api_bindings(spec, errors)
