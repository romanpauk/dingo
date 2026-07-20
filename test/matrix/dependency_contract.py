#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT

"""Validate dependency coverage shared by the matrix test families."""

from __future__ import annotations

from axes.constructor_detection import (
    CONSTRUCTOR_ARGUMENT_CATEGORIES,
    CONSTRUCTOR_DETECTION_BACKENDS,
    CONSTRUCTOR_DETECTION_MODES,
    CONSTRUCTOR_SHAPES,
)
from axes.containers import CONTAINERS
from axes.dependency_compositions import (
    DEPENDENCY_COMPOSITION_LEAVES,
    DEPENDENCY_COMPOSITION_OPERATORS,
    DEPENDENCY_COMPOSITIONS,
    validate_dependency_compositions,
)
from axes.dependency_forms import (
    DEPENDENCY_CARRIERS,
    DEPENDENCY_DECORATIONS,
    DEPENDENCY_FORMS,
    DEPENDENCY_PROVISIONINGS,
    DEPENDENCY_SHAPES,
    dependency_form_name,
)
from axes.exposed_types import EXPOSED_TYPES
from axes.features import FEATURES
from axes.invoke import INVOKE_CALLABLES
from axes.modes import REGISTRATION_MODES
from axes.resolved_types import RESOLVED_TYPES
from axes.scopes import SCOPES
from axes.stored_types import STORED_TYPES
from schema import (
    ConstructorArgumentCategory,
    ConstructorShape,
    DependencyCarrier,
    DependencyComposition,
    DependencyCompositionOperator,
    DependencyDecoration,
    DependencyForm,
    DependencyProvisioning,
    DependencyShape,
    ExposedType,
    Feature,
    InvokeCallable,
    RegistrationMode,
    ResolvedType,
    ScopeSpec,
    StoredType,
)


DEPENDENCY_FAMILIES = frozenset(
    {
        "constructor_argument_conversion",
        "constructor_detection",
        "resolution",
        "invoke",
    }
)


def _by_name(axis: tuple, identity: str = "name") -> dict[str, object]:
    result: dict[str, object] = {}
    for member in axis:
        name = getattr(member, identity)
        if name in result:
            raise ValueError(f"dependency {identity} axis contains duplicate: {name}")
        result[name] = member
    return result


def _validate_form_references(
    owner: str,
    members: tuple,
    known_forms: frozenset[str],
) -> None:
    for member in members:
        missing = sorted(member.dependency_forms - known_forms)
        if missing:
            raise ValueError(
                f"{owner} {member.name} references unknown dependency forms: "
                f"{missing}"
            )


def validate_dependency_contract(
    *,
    shapes: tuple[DependencyShape, ...] = DEPENDENCY_SHAPES,
    carriers: tuple[DependencyCarrier, ...] = DEPENDENCY_CARRIERS,
    decorations: tuple[DependencyDecoration, ...] = DEPENDENCY_DECORATIONS,
    forms: tuple[DependencyForm, ...] = DEPENDENCY_FORMS,
    compositions: tuple[
        DependencyComposition, ...
    ] = DEPENDENCY_COMPOSITIONS,
    composition_operators: tuple[
        DependencyCompositionOperator, ...
    ] = DEPENDENCY_COMPOSITION_OPERATORS,
    composition_leaves: tuple[
        DependencyComposition, ...
    ] = DEPENDENCY_COMPOSITION_LEAVES,
    provisionings: tuple[DependencyProvisioning, ...] = DEPENDENCY_PROVISIONINGS,
    constructor_shapes: tuple[ConstructorShape, ...] = CONSTRUCTOR_SHAPES,
    argument_categories: tuple[ConstructorArgumentCategory, ...] = (
        CONSTRUCTOR_ARGUMENT_CATEGORIES
    ),
    resolved_types: tuple[ResolvedType, ...] = RESOLVED_TYPES,
    invoke_callables: tuple[InvokeCallable, ...] = INVOKE_CALLABLES,
    modes: tuple[RegistrationMode, ...] = REGISTRATION_MODES,
    scopes: tuple[ScopeSpec, ...] = SCOPES,
    stored_types: tuple[StoredType, ...] = STORED_TYPES,
    exposed_types: tuple[ExposedType, ...] = EXPOSED_TYPES,
    features: tuple[Feature, ...] = FEATURES,
) -> None:
    validate_dependency_compositions(
        compositions,
        composition_operators,
        composition_leaves,
    )
    shapes_by_name = _by_name(shapes)
    carriers_by_name = _by_name(carriers)
    decorations_by_name = _by_name(decorations)
    known_shapes = frozenset(shapes_by_name)
    known_carriers = frozenset(carriers_by_name)
    known_backends = frozenset(
        backend.name for backend in CONSTRUCTOR_DETECTION_BACKENDS
    )
    known_detection_modes = frozenset(
        mode.name for mode in CONSTRUCTOR_DETECTION_MODES
    )
    for shape in shapes:
        unknown_families = sorted(
            shape.required_families - DEPENDENCY_FAMILIES
        )
        if unknown_families:
            raise ValueError(
                f"dependency shape {shape.name} references unknown families: "
                f"{unknown_families}"
            )
        if not shape.required_families:
            raise ValueError(
                f"dependency shape {shape.name} does not require any test family"
            )
        for capability in shape.constructor_detection_limitations:
            unknown_carriers = sorted(capability.carriers - known_carriers)
            unknown_decorations = sorted(
                capability.decorations - decorations_by_name.keys()
            )
            if unknown_carriers:
                raise ValueError(
                    f"dependency shape {shape.name} limitation references "
                    f"unknown carriers: {unknown_carriers}"
                )
            if unknown_decorations:
                raise ValueError(
                    f"dependency shape {shape.name} limitation references "
                    f"unknown decorations: {unknown_decorations}"
                )
            if not capability.carriers or not capability.decorations:
                raise ValueError(
                    f"dependency shape {shape.name} limitation must select "
                    "carriers and decorations"
                )
            limitation = capability.limitation
            if (
                limitation.backend is not None
                and limitation.backend not in known_backends
            ):
                raise ValueError(
                    f"dependency shape {shape.name} limitation references "
                    f"unknown backend: {limitation.backend}"
                )
            if limitation.mode not in known_detection_modes:
                raise ValueError(
                    f"dependency shape {shape.name} limitation references "
                    f"unknown mode: {limitation.mode}"
                )
            if not limitation.reason:
                raise ValueError(
                    f"dependency shape {shape.name} limitation has no reason"
                )
            compatible = any(
                (shape.name, carrier) in decorations_by_name[
                    decoration
                ].supported_shape_carriers
                for carrier in capability.carriers
                for decoration in capability.decorations
            )
            if not compatible:
                raise ValueError(
                    f"dependency shape {shape.name} limitation does not match "
                    "a compatible shape/carrier/decoration cell"
                )
    for decoration in decorations:
        missing_shapes = sorted(
            shape
            for shape, _ in decoration.supported_shape_carriers
            if shape not in known_shapes
        )
        missing_carriers = sorted(
            carrier
            for _, carrier in decoration.supported_shape_carriers
            if carrier not in known_carriers
        )
        if missing_shapes:
            raise ValueError(
                f"dependency decoration {decoration.name} references unknown "
                f"shapes: {missing_shapes}"
            )
        if missing_carriers:
            raise ValueError(
                f"dependency decoration {decoration.name} references unknown "
                f"carriers: {missing_carriers}"
            )
        if not decoration.supported_shape_carriers:
            raise ValueError(
                f"dependency decoration {decoration.name} does not support any "
                "shape/carrier pairs"
            )
        unknown_families = sorted(
            decoration.required_families - DEPENDENCY_FAMILIES
        )
        if unknown_families:
            raise ValueError(
                f"dependency decoration {decoration.name} references unknown "
                f"families: {unknown_families}"
            )
        if not decoration.required_families:
            raise ValueError(
                f"dependency decoration {decoration.name} does not require any "
                "test family"
            )

    forms_by_name = _by_name(forms)
    known_forms = frozenset(forms_by_name)
    form_shapes: dict[tuple[str, str, str], str] = {}
    for form in forms:
        product_cell = (form.shape, form.carrier, form.decoration)
        if product_cell in form_shapes:
            raise ValueError(
                "dependency form axis contains duplicate shape/carrier/decoration: "
                f"{product_cell[0]} / {product_cell[1]} / {product_cell[2]} "
                f"({form_shapes[product_cell]}, {form.name})"
            )
        form_shapes[product_cell] = form.name
        if form.shape not in known_shapes:
            raise ValueError(
                f"dependency form {form.name} references unknown shape: "
                f"{form.shape}"
            )
        if form.carrier not in known_carriers:
            raise ValueError(
                f"dependency form {form.name} references unknown carrier: "
                f"{form.carrier}"
            )
        if form.decoration not in decorations_by_name:
            raise ValueError(
                f"dependency form {form.name} references unknown decoration: "
                f"{form.decoration}"
            )
        unknown_families = sorted(form.required_families - DEPENDENCY_FAMILIES)
        if unknown_families:
            raise ValueError(
                f"dependency form {form.name} references unknown families: "
                f"{unknown_families}"
            )
        if not form.required_families:
            raise ValueError(
                f"dependency form {form.name} does not require any test family"
            )

    expected_forms = {
        dependency_form_name(shape.name, carrier.name, decoration.name): (
            shape.name,
            carrier.name,
            decoration.name,
            shape.required_families & decoration.required_families,
        )
        for decoration in decorations
        for shape in shapes
        for carrier in carriers
        if (shape.name, carrier.name)
        in decoration.supported_shape_carriers
    }
    actual_forms = {
        form.name: (
            form.shape,
            form.carrier,
            form.decoration,
            form.required_families,
        )
        for form in forms
    }
    missing_product = sorted(expected_forms.keys() - actual_forms.keys())
    unexpected_product = sorted(actual_forms.keys() - expected_forms.keys())
    mismatched_product = sorted(
        name
        for name in expected_forms.keys() & actual_forms.keys()
        if expected_forms[name] != actual_forms[name]
    )
    if missing_product or unexpected_product or mismatched_product:
        raise ValueError(
            "dependency forms do not match the compatible shape x carrier x "
            "decoration product: "
            f"missing={missing_product}, unexpected={unexpected_product}, "
            f"mismatched={mismatched_product}"
        )

    _validate_form_references(
        "constructor shape", constructor_shapes, known_forms
    )
    _validate_form_references("resolved type", resolved_types, known_forms)
    _validate_form_references("invoke callable", invoke_callables, known_forms)
    for category in argument_categories:
        if category.dependency_form not in known_forms:
            raise ValueError(
                f"constructor argument category {category.name} references "
                f"unknown dependency form: {category.dependency_form}"
            )

    for shape in constructor_shapes:
        if shape.detector_only and shape.dependency_forms:
            raise ValueError(
                f"detector-only constructor shape {shape.name} declares "
                "dependency forms"
            )
        if not shape.detector_only and not shape.dependency_forms:
            raise ValueError(
                f"constructor shape {shape.name} must declare a dependency form "
                "or be detector-only"
            )

    covered_by_family = {
        "constructor_argument_conversion": frozenset(
            category.dependency_form for category in argument_categories
        ),
        "constructor_detection": frozenset().union(
            *(shape.dependency_forms for shape in constructor_shapes)
        ),
        "resolution": frozenset().union(
            *(resolved_type.dependency_forms for resolved_type in resolved_types)
        ),
        "invoke": frozenset().union(
            *(callable_spec.dependency_forms for callable_spec in invoke_callables)
        ),
    }
    missing_coverage = sorted(
        (form.name, family)
        for form in forms
        for family in form.required_families
        if form.name not in covered_by_family[family]
    )
    if missing_coverage:
        raise RuntimeError(
            "dependency forms lack required family coverage: "
            + ", ".join(
                f"{form} / {family}" for form, family in missing_coverage
            )
        )

    provisionings_by_name = _by_name(provisionings)
    known_provisionings = frozenset(provisionings_by_name)
    for provisioning in provisionings:
        declared_forms = tuple(
            form for form, _, _ in provisioning.resolution_cases
        )
        if len(declared_forms) != len(set(declared_forms)):
            raise ValueError(
                f"dependency provisioning {provisioning.name} maps a form to "
                "multiple resolution cases"
            )
        missing_forms = sorted(provisioning.supported_forms - known_forms)
        if missing_forms:
            raise ValueError(
                f"dependency provisioning {provisioning.name} references unknown "
                f"forms: {missing_forms}"
            )
        if not provisioning.supported_forms:
            raise ValueError(
                f"dependency provisioning {provisioning.name} does not support "
                "any forms"
            )
    for callable_spec in invoke_callables:
        if callable_spec.supported_containers is not None:
            unknown_containers = sorted(
                callable_spec.supported_containers
                - {container.name for container in CONTAINERS}
            )
            if unknown_containers:
                raise ValueError(
                    f"invoke callable {callable_spec.name} references unknown "
                    f"containers: {unknown_containers}"
                )
        missing = sorted(
            callable_spec.supported_provisionings - known_provisionings
        )
        if missing:
            raise ValueError(
                f"invoke callable {callable_spec.name} references unknown "
                f"dependency provisionings: {missing}"
            )
        for provisioning_name in callable_spec.supported_provisionings:
            provisioning = provisionings_by_name[provisioning_name]
            unsupported_forms = sorted(
                callable_spec.dependency_forms - provisioning.supported_forms
            )
            if unsupported_forms:
                raise ValueError(
                    f"invoke callable {callable_spec.name} uses unsupported forms "
                    f"with provisioning {provisioning_name}: {unsupported_forms}"
                )
    modes_by_name = _by_name(modes)
    scopes_by_name = _by_name(scopes)
    stored_by_id = _by_name(stored_types, "id")
    exposed_by_name = _by_name(exposed_types)
    resolved_by_name = _by_name(resolved_types)
    features_by_name = _by_name(features)
    provisioned_forms: set[str] = set()
    for provisioning in provisionings:
        resolution_cases = []
        for form, resolved_name, feature_name in provisioning.resolution_cases:
            try:
                case_resolved_type = resolved_by_name[resolved_name]
            except KeyError as error:
                raise ValueError(
                    f"dependency provisioning {provisioning.name} maps form "
                    f"{form} to unknown resolved type: {error.args[0]}"
                ) from error
            try:
                feature = features_by_name[feature_name]
            except KeyError as error:
                raise ValueError(
                    f"dependency provisioning {provisioning.name} maps form "
                    f"{form} to unknown behavior feature: {error.args[0]}"
                ) from error
            resolution_cases.append((form, case_resolved_type, feature))
        missing_modes = sorted(provisioning.supported_modes - modes_by_name.keys())
        if missing_modes:
            raise ValueError(
                f"dependency provisioning {provisioning.name} references unknown "
                f"modes: {missing_modes}"
            )
        try:
            scope = scopes_by_name[provisioning.scope]
            stored_type = stored_by_id[provisioning.stored_type]
            exposed_type = exposed_by_name[provisioning.exposed_type]
            resolved_type = resolved_by_name[provisioning.resolved_type]
        except KeyError as error:
            raise ValueError(
                f"dependency provisioning {provisioning.name} references unknown "
                f"registration metadata: {error.args[0]}"
            ) from error
        provisioned_forms.update(provisioning.supported_forms)
        if scope.name not in stored_type.supported_scopes:
            raise ValueError(
                f"dependency provisioning {provisioning.name} uses unsupported scope"
            )
        if stored_type.kind not in exposed_type.supported_stored_kinds:
            raise ValueError(
                f"dependency provisioning {provisioning.name} uses unsupported "
                "exposure"
            )
        if exposed_type.name not in resolved_type.supported_exposed_types:
            raise ValueError(
                f"dependency provisioning {provisioning.name} uses unsupported "
                "resolution"
            )
        compatible_modes = (
            stored_type.supported_modes & resolved_type.supported_modes
        )
        unsupported_modes = sorted(
            provisioning.supported_modes - compatible_modes
        )
        if unsupported_modes:
            raise ValueError(
                f"dependency provisioning {provisioning.name} uses unsupported "
                f"modes: {unsupported_modes}"
            )
        for form, case_resolved_type, feature in resolution_cases:
            if form not in case_resolved_type.dependency_forms:
                raise ValueError(
                    f"dependency provisioning {provisioning.name} maps form {form} "
                    f"to resolved type {case_resolved_type.name}, which does not "
                    "declare that form"
                )
            if exposed_type.name not in case_resolved_type.supported_exposed_types:
                raise ValueError(
                    f"dependency provisioning {provisioning.name} maps form {form} "
                    f"to resolved type {case_resolved_type.name} with unsupported "
                    "exposure"
                )
            unsupported_resolved_modes = sorted(
                provisioning.supported_modes
                - case_resolved_type.supported_modes
            )
            if unsupported_resolved_modes:
                raise ValueError(
                    f"dependency provisioning {provisioning.name} maps form {form} "
                    f"to resolved type {case_resolved_type.name} in unsupported "
                    f"modes: {unsupported_resolved_modes}"
                )
            unsupported_form_modes = sorted(
                provisioning.supported_modes - feature.modes
            )
            if unsupported_form_modes:
                raise ValueError(
                    f"dependency provisioning {provisioning.name} maps form {form} "
                    f"to feature {feature.name} in unsupported modes: "
                    f"{unsupported_form_modes}"
                )

    missing_provisioned_forms = sorted(
        form.name
        for form in forms
        if "resolution" in form.required_families
        and form.name not in provisioned_forms
    )
    if missing_provisioned_forms:
        raise RuntimeError(
            "dependency forms lack a provisioning recipe: "
            + ", ".join(missing_provisioned_forms)
        )


__all__ = ("DEPENDENCY_FAMILIES", "validate_dependency_contract")
