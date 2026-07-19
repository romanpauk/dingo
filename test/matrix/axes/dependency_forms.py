#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT

"""Shared dependency semantics and exhaustive provisioning recipes."""

from __future__ import annotations

from schema import (
    ConstructorDetectionLimitation,
    DependencyCarrier,
    DependencyDecoration,
    DependencyForm,
    DependencyProvisioning,
    DependencyShape,
    DependencyShapeConstructorDetectionLimitation,
)


WRAPPER_SIGNATURE_RECOVERY_REASON = (
    "constructor signature recovery cannot distinguish an outer value wrapper "
    "from conversions inspected by that wrapper"
)
WRAPPER_SIGNATURE_RECOVERY_LIMITATION = ConstructorDetectionLimitation(
    backend=None,
    mode="signature",
    reason=WRAPPER_SIGNATURE_RECOVERY_REASON,
)


_ALL_FAMILIES = frozenset(
    {
        "constructor_argument_conversion",
        "constructor_detection",
        "resolution",
        "invoke",
    }
)
_STRUCTURAL_FAMILIES = frozenset(
    {"constructor_detection", "resolution", "invoke"}
)
DEPENDENCY_SHAPES = (
    DependencyShape(
        name="identity",
        type_expression="{base}",
        required_families=_ALL_FAMILIES,
    ),
    DependencyShape(
        name="pointer",
        type_expression="{base} *",
        required_families=_ALL_FAMILIES,
    ),
    DependencyShape(
        name="const_pointer",
        type_expression="const {base} *",
        required_families=_ALL_FAMILIES,
    ),
    DependencyShape(
        name="shared_pointer",
        type_expression="std::shared_ptr<{base}>",
        required_families=_STRUCTURAL_FAMILIES,
    ),
    DependencyShape(
        name="unique_pointer",
        type_expression="std::unique_ptr<{base}>",
        required_families=_STRUCTURAL_FAMILIES,
    ),
    DependencyShape(
        name="optional",
        type_expression="std::optional<{base}>",
        required_families=_STRUCTURAL_FAMILIES,
        constructor_detection_limitations=(
            DependencyShapeConstructorDetectionLimitation(
                carriers=frozenset({"value"}),
                decorations=frozenset({"plain"}),
                limitation=WRAPPER_SIGNATURE_RECOVERY_LIMITATION,
            ),
        ),
    ),
    DependencyShape(
        name="array",
        type_expression="dependency_array<{base}>",
        required_families=_STRUCTURAL_FAMILIES,
    ),
    DependencyShape(
        name="variant",
        type_expression="dependency_variant<{base}>",
        required_families=_STRUCTURAL_FAMILIES,
        constructor_detection_limitations=(
            DependencyShapeConstructorDetectionLimitation(
                carriers=frozenset({"value"}),
                decorations=frozenset({"plain"}),
                limitation=WRAPPER_SIGNATURE_RECOVERY_LIMITATION,
            ),
        ),
    ),
)


DEPENDENCY_CARRIERS = (
    DependencyCarrier(name="value", type_expression="{base}"),
    DependencyCarrier(name="lvalue_reference", type_expression="{base} &"),
    DependencyCarrier(
        name="const_lvalue_reference",
        type_expression="const {base} &",
    ),
    DependencyCarrier(name="rvalue_reference", type_expression="{base} &&"),
)


DEPENDENCY_DECORATIONS = (
    DependencyDecoration(
        name="plain",
        request_expression="{carrier}",
        supported_shape_carriers=frozenset(
            {
                ("identity", "value"),
                ("identity", "lvalue_reference"),
                ("identity", "const_lvalue_reference"),
                ("identity", "rvalue_reference"),
                ("pointer", "value"),
                ("const_pointer", "value"),
                ("shared_pointer", "value"),
                ("shared_pointer", "lvalue_reference"),
                ("unique_pointer", "value"),
                ("optional", "value"),
                ("optional", "lvalue_reference"),
                ("array", "lvalue_reference"),
                ("variant", "value"),
            }
        ),
        required_families=_ALL_FAMILIES,
    ),
    DependencyDecoration(
        name="selected",
        request_expression=(
            "dingo::detail::selected<{carrier}, "
            "dingo::detail::type_selector<{selector}>>"
        ),
        supported_shape_carriers=frozenset(
            {
                ("identity", "value"),
                ("identity", "lvalue_reference"),
            }
        ),
        required_families=_STRUCTURAL_FAMILIES,
    ),
    DependencyDecoration(
        name="annotated",
        request_expression="dingo::annotated<{carrier}, {selector}>",
        supported_shape_carriers=frozenset(
            {
                ("identity", "lvalue_reference"),
                ("pointer", "value"),
                ("shared_pointer", "value"),
                ("shared_pointer", "lvalue_reference"),
            }
        ),
        required_families=_STRUCTURAL_FAMILIES,
    ),
)


def dependency_form_name(shape: str, carrier: str, decoration: str) -> str:
    if shape == "identity":
        structural_name = carrier
    elif carrier == "value":
        structural_name = shape
    else:
        structural_name = f"{shape}_{carrier}"
    return (
        structural_name
        if decoration == "plain"
        else f"{decoration}_{structural_name}"
    )


def build_dependency_forms(
    shapes: tuple[DependencyShape, ...] = DEPENDENCY_SHAPES,
    carriers: tuple[DependencyCarrier, ...] = DEPENDENCY_CARRIERS,
    decorations: tuple[DependencyDecoration, ...] = DEPENDENCY_DECORATIONS,
) -> tuple[DependencyForm, ...]:
    return tuple(
        DependencyForm(
            name=dependency_form_name(
                shape.name, carrier.name, decoration.name
            ),
            shape=shape.name,
            carrier=carrier.name,
            decoration=decoration.name,
            required_families=(
                shape.required_families & decoration.required_families
            ),
        )
        for decoration in decorations
        for shape in shapes
        for carrier in carriers
        if (shape.name, carrier.name)
        in decoration.supported_shape_carriers
    )


DEPENDENCY_FORMS = build_dependency_forms()


_SHAPES_BY_NAME = {shape.name: shape for shape in DEPENDENCY_SHAPES}
_CARRIERS_BY_NAME = {carrier.name: carrier for carrier in DEPENDENCY_CARRIERS}
_DECORATIONS_BY_NAME = {
    decoration.name: decoration for decoration in DEPENDENCY_DECORATIONS
}


def render_dependency_request(
    form: DependencyForm, base: str, selector: str
) -> str:
    carrier_type = render_dependency_type(form, base)
    decoration = _DECORATIONS_BY_NAME[form.decoration]
    return decoration.request_expression.format(
        carrier=carrier_type,
        selector=selector,
    )


def render_dependency_type(form: DependencyForm, base: str) -> str:
    shaped_type = _SHAPES_BY_NAME[form.shape].type_expression.format(base=base)
    return render_dependency_carrier(form.carrier, shaped_type)


def render_dependency_carrier(carrier: str, base: str) -> str:
    return _CARRIERS_BY_NAME[carrier].type_expression.format(base=base)


_ALL_MODES = frozenset({"runtime", "static", "mixed"})
_RUNTIME_MODE = frozenset({"runtime"})

_STABLE_CONCRETE_CASES = (
    ("lvalue_reference", "value_ref_ptr", "resolve_concrete"),
    ("pointer", "value_ref_ptr", "resolve_concrete"),
    ("value", "value", "resolve_concrete"),
    ("const_lvalue_reference", "const_value_ref", "resolve_concrete"),
    ("const_pointer", "const_value_ptr", "resolve_concrete"),
)
_UNIQUE_CONCRETE_CASES = (
    ("rvalue_reference", "value_rvalue", "lifetime_counts"),
    ("value", "value", "resolve_concrete"),
)
_SHARED_WRAPPER_CASES = (
    ("shared_pointer", "value_shared_ptr", "resolve_wrapper"),
    (
        "shared_pointer_lvalue_reference",
        "value_shared_ptr_ref",
        "resolve_wrapper",
    ),
)
_ANNOTATED_CONCRETE_CASES = (
    ("annotated_lvalue_reference", "annotated_value_ref", "annotated"),
)
_ANNOTATED_INTERFACE_CASES = (
    ("annotated_lvalue_reference", "annotated_interface_ref", "annotated"),
    ("annotated_pointer", "annotated_interface_ptr", "annotated"),
    (
        "annotated_shared_pointer",
        "annotated_interface_shared_ptr",
        "annotated",
    ),
    (
        "annotated_shared_pointer_lvalue_reference",
        "annotated_interface_shared_ptr_ref",
        "annotated",
    ),
)


def _provisioning(
    name: str,
    scope: str,
    stored_type: str,
    exposed_type: str,
    resolution_cases: tuple[tuple[str, str, str], ...],
    supported_modes: frozenset[str],
) -> DependencyProvisioning:
    return DependencyProvisioning(
        name=name,
        scope=scope,
        stored_type=stored_type,
        exposed_type=exposed_type,
        resolution_cases=resolution_cases,
        supported_modes=supported_modes,
    )


_EXTERNAL_STORED_TYPES = (
    ("external_value", "external_value_type"),
    ("external_reference", "external_value_ref"),
    ("external_pointer", "external_value_ptr"),
    ("external_stored_pointer", "value_ptr"),
    ("external_stored_reference", "value_ref"),
    ("external_shared_pointer", "external_value_shared_ptr"),
)


DEPENDENCY_PROVISIONINGS = (
    _provisioning(
        "shared_value",
        "shared",
        "value_type",
        "concrete",
        _STABLE_CONCRETE_CASES,
        _ALL_MODES,
    ),
    _provisioning(
        "unique_value",
        "unique",
        "unique_value_type",
        "concrete",
        _UNIQUE_CONCRETE_CASES,
        _ALL_MODES,
    ),
    _provisioning(
        "shared_pointer_value",
        "shared",
        "value_shared_ptr",
        "concrete",
        _SHARED_WRAPPER_CASES,
        _ALL_MODES,
    ),
    _provisioning(
        "unique_pointer_value",
        "unique",
        "value_unique_ptr",
        "concrete",
        (("unique_pointer", "value_unique_ptr", "resolve_wrapper"),),
        _ALL_MODES,
    ),
    _provisioning(
        "unique_optional_value",
        "unique",
        "value_optional",
        "concrete",
        (("optional", "value_optional", "resolve_wrapper"),),
        _ALL_MODES,
    ),
    _provisioning(
        "shared_optional_value",
        "shared",
        "value_optional",
        "concrete",
        (
            (
                "optional_lvalue_reference",
                "value_optional_ref",
                "resolve_wrapper",
            ),
        ),
        _ALL_MODES,
    ),
    _provisioning(
        "shared_array_value",
        "shared",
        "value_array_2",
        "array_concrete",
        (("array_lvalue_reference", "raw_array_ref", "array"),),
        _ALL_MODES,
    ),
    _provisioning(
        "unique_variant_value",
        "unique",
        "value_variant",
        "variant_concrete",
        (("variant", "variant_value", "variant"),),
        _ALL_MODES,
    ),
    *(
        _provisioning(
            name,
            "external",
            stored_type,
            "concrete",
            _STABLE_CONCRETE_CASES,
            _RUNTIME_MODE,
        )
        for name, stored_type in _EXTERNAL_STORED_TYPES
    ),
    _provisioning(
        "shared_annotated_value",
        "shared",
        "value_type",
        "annotated_concrete",
        _ANNOTATED_CONCRETE_CASES,
        _ALL_MODES,
    ),
    _provisioning(
        "shared_annotated_shared_pointer",
        "shared",
        "value_shared_ptr",
        "annotated_concrete",
        _ANNOTATED_CONCRETE_CASES,
        _ALL_MODES,
    ),
    _provisioning(
        "shared_annotated_optional",
        "shared",
        "value_optional",
        "annotated_concrete",
        _ANNOTATED_CONCRETE_CASES,
        _ALL_MODES,
    ),
    _provisioning(
        "shared_annotated_interface",
        "shared",
        "implementation_shared_ptr",
        "annotated_interface",
        _ANNOTATED_INTERFACE_CASES,
        _ALL_MODES,
    ),
    *(
        _provisioning(
            f"{name}_annotated",
            "external",
            stored_type,
            "annotated_concrete",
            _ANNOTATED_CONCRETE_CASES,
            _RUNTIME_MODE,
        )
        for name, stored_type in _EXTERNAL_STORED_TYPES
    ),
    _provisioning(
        "keyed_value",
        "shared",
        "value_type",
        "keyed_concrete",
        (
            (
                "selected_lvalue_reference",
                "keyed_value_dependency",
                "construct",
            ),
        ),
        _ALL_MODES,
    ),
    _provisioning(
        "keyed_collection",
        "shared",
        "element_shared_ptr",
        "element_keyed_collection",
        (("selected_value", "keyed_collection_dependency", "construct"),),
        frozenset({"static", "mixed"}),
    ),
    *(
        _provisioning(
            f"{name}_keyed",
            "external",
            stored_type,
            "keyed_concrete",
            (
                (
                    "selected_lvalue_reference",
                    "keyed_value_dependency",
                    "construct",
                ),
            ),
            _RUNTIME_MODE,
        )
        for name, stored_type in _EXTERNAL_STORED_TYPES
    ),
)


__all__ = (
    "DEPENDENCY_CARRIERS",
    "DEPENDENCY_DECORATIONS",
    "DEPENDENCY_FORMS",
    "DEPENDENCY_PROVISIONINGS",
    "DEPENDENCY_SHAPES",
    "build_dependency_forms",
    "dependency_form_name",
    "render_dependency_carrier",
    "render_dependency_request",
    "render_dependency_type",
)
