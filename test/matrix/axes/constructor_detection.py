#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from axes.dependency_compositions import (
    DEPENDENCY_COMPOSITIONS,
    render_dependency_composition,
)
from axes.dependency_forms import (
    DEPENDENCY_FORMS,
    DEPENDENCY_SHAPES,
    WRAPPER_SIGNATURE_RECOVERY_LIMITATION,
    render_dependency_request,
)
from schema import (
    ConstructorArgumentCategory,
    ConstructorArgumentStorage,
    ConstructorDetectionLimitation,
    ConstructorShape,
    ConstructorDetectionBackend,
    ConstructorDetectionMode,
    DependencyForm,
)


CONSTRUCTOR_ARGUMENT_STORAGES = (
    ConstructorArgumentStorage(
        name="unique",
        type_name="constructor_unique_argument",
    ),
    ConstructorArgumentStorage(
        name="shared",
        type_name="constructor_shared_argument",
    ),
)

CONSTRUCTOR_ARGUMENT_CATEGORIES = (
    ConstructorArgumentCategory(
        name="value",
        type_name="constructor_argument_value",
        dependency_form="value",
        supported_storages=frozenset({"unique", "shared"}),
        guard="!defined(_MSC_VER) || DINGO_CXX_STANDARD > 17",
    ),
    ConstructorArgumentCategory(
        name="lvalue_reference",
        type_name="constructor_argument_lvalue_reference",
        dependency_form="lvalue_reference",
        supported_storages=frozenset({"unique", "shared"}),
    ),
    ConstructorArgumentCategory(
        name="const_lvalue_reference",
        type_name="constructor_argument_const_lvalue_reference",
        dependency_form="const_lvalue_reference",
        supported_storages=frozenset({"unique", "shared"}),
    ),
    ConstructorArgumentCategory(
        name="rvalue_reference",
        type_name="constructor_argument_rvalue_reference",
        dependency_form="rvalue_reference",
        supported_storages=frozenset({"unique", "shared"}),
    ),
    ConstructorArgumentCategory(
        name="pointer",
        type_name="constructor_argument_pointer",
        dependency_form="pointer",
        supported_storages=frozenset({"unique"}),
    ),
    ConstructorArgumentCategory(
        name="const_pointer",
        type_name="constructor_argument_const_pointer",
        dependency_form="const_pointer",
        supported_storages=frozenset({"unique"}),
    ),
)


CONSTRUCTOR_DETECTION_BACKENDS = (
    ConstructorDetectionBackend(
        name="portable",
        type_name="constructor_detection_portable_backend",
        guard="!defined(_MSC_VER)",
    ),
    ConstructorDetectionBackend(
        name="msvc",
        type_name="constructor_detection_msvc_backend",
    ),
)

CONSTRUCTOR_DETECTION_MODES = (
    ConstructorDetectionMode(
        name="shape",
        type_name="dingo::detail::constructor_shape",
    ),
    ConstructorDetectionMode(
        name="signature",
        type_name="dingo::detail::constructor_signature",
    ),
)

_DEPENDENCY_SHAPES_BY_NAME = {
    shape.name: shape for shape in DEPENDENCY_SHAPES
}

_UNCONSTRAINED_FORWARDING_LIMITATIONS = tuple(
    ConstructorDetectionLimitation(
        backend=None,
        mode=mode,
        reason=(
            "constructor detection cannot distinguish a concrete dependency "
            "with an unconstrained forwarding constructor from a generic "
            "constructor"
        ),
    )
    for mode in ("shape", "signature")
)


def _dependency_shape(form: DependencyForm) -> ConstructorShape:
    request = render_dependency_request(
        form, "constructor_config", "constructor_selector"
    )
    signature_request = (
        "constructor_config"
        if (
            form.shape == "identity"
            and form.decoration == "plain"
            and form.carrier == "rvalue_reference"
        )
        else request
    )
    return ConstructorShape(
        name=f"dependency_{form.name}",
        target_type=f"constructor_dependency<{request}>",
        expected_kind="dingo::detail::constructor_kind::concrete",
        expected_arity=1,
        signature_arguments=f"dingo::type_list<{signature_request}>",
        dependency_forms=frozenset({form.name}),
        constructor_detection_limitations=tuple(
            capability.limitation
            for capability in _DEPENDENCY_SHAPES_BY_NAME[
                form.shape
            ].constructor_detection_limitations
            if form.carrier in capability.carriers
            and form.decoration in capability.decorations
        ),
    )


DEPENDENCY_CONSTRUCTOR_SHAPES = tuple(
    _dependency_shape(form)
    for form in DEPENDENCY_FORMS
    if "constructor_detection" in form.required_families
)


WRAPPED_DEPENDENCY_CONSTRUCTOR_SHAPES = tuple(
    ConstructorShape(
        name=composition.name,
        target_type=f"constructor_dependency<{type_name}>",
        expected_kind="dingo::detail::constructor_kind::concrete",
        expected_arity=1,
        signature_arguments=f"dingo::type_list<{type_name}>",
        detector_only=True,
        constructor_detection_limitations=(
            WRAPPER_SIGNATURE_RECOVERY_LIMITATION,
            *composition.constructor_detection_limitations,
        ),
    )
    for composition in DEPENDENCY_COMPOSITIONS
    for type_name in (render_dependency_composition(composition),)
)


CONSTRUCTOR_SHAPES = (
    *DEPENDENCY_CONSTRUCTOR_SHAPES,
    ConstructorShape(
        name="two_values",
        target_type="constructor_two_values",
        expected_kind="dingo::detail::constructor_kind::concrete",
        expected_arity=2,
        signature_arguments="dingo::type_list<int, float>",
        dependency_forms=frozenset({"value"}),
    ),
    ConstructorShape(
        name="mixed_wrappers",
        target_type="constructor_mixed_wrappers",
        expected_kind="dingo::detail::constructor_kind::concrete",
        expected_arity=4,
        signature_arguments=(
            "dingo::type_list<int, "
            "constructor_test_wrapper<constructor_config>, "
            "std::optional<constructor_config>, "
            "std::variant<constructor_config, "
            "constructor_move_only_config>>"
        ),
        dependency_forms=frozenset({"value", "optional", "variant"}),
        constructor_detection_limitations=(
            WRAPPER_SIGNATURE_RECOVERY_LIMITATION,
        ),
    ),
    ConstructorShape(
        name="nested_forwarding_wrapper",
        target_type="constructor_nested_forwarding_wrapper",
        expected_kind="dingo::detail::constructor_kind::concrete",
        expected_arity=1,
        signature_arguments=(
            "dingo::type_list<constructor_nested_forwarding_dependency>"
        ),
        detector_only=True,
        constructor_detection_limitations=(
            WRAPPER_SIGNATURE_RECOVERY_LIMITATION,
        ),
    ),
    *WRAPPED_DEPENDENCY_CONSTRUCTOR_SHAPES,
    ConstructorShape(
        name="unconstrained_forwarding_wrapper",
        target_type="constructor_unconstrained_forwarding_wrapper_consumer",
        expected_kind="dingo::detail::constructor_kind::concrete",
        expected_arity=1,
        signature_arguments=(
            "dingo::type_list<"
            "constructor_unconstrained_forwarding_dependency>"
        ),
        detector_only=True,
        constructor_detection_limitations=(
            _UNCONSTRAINED_FORWARDING_LIMITATIONS
        ),
    ),
    ConstructorShape(
        name="copy_only_value",
        target_type="constructor_copy_only_value",
        expected_kind="dingo::detail::constructor_kind::concrete",
        expected_arity=1,
        signature_arguments="dingo::type_list<constructor_copy_only_config>",
        dependency_forms=frozenset({"value"}),
    ),
    ConstructorShape(
        name="move_only_value",
        target_type="constructor_move_only_value",
        expected_kind="dingo::detail::constructor_kind::concrete",
        expected_arity=1,
        signature_arguments="dingo::type_list<constructor_move_only_config>",
        dependency_forms=frozenset({"value"}),
    ),
    ConstructorShape(
        name="incomplete_reference",
        target_type="constructor_incomplete_reference",
        expected_kind="dingo::detail::constructor_kind::concrete",
        expected_arity=1,
        signature_arguments="dingo::type_list<constructor_incomplete_config &>",
        dependency_forms=frozenset({"lvalue_reference"}),
    ),
    ConstructorShape(
        name="selected_incomplete_reference",
        target_type="constructor_selected_incomplete_reference",
        expected_kind="dingo::detail::constructor_kind::concrete",
        expected_arity=1,
        signature_arguments=(
            "dingo::type_list<dingo::detail::selected<"
            "constructor_incomplete_config &, "
            "dingo::detail::type_selector<constructor_selector>>>"
        ),
        dependency_forms=frozenset({"selected_lvalue_reference"}),
    ),
    ConstructorShape(
        name="aggregate",
        target_type="constructor_aggregate",
        expected_kind="dingo::detail::constructor_kind::concrete",
        expected_arity=2,
        signature_arguments="dingo::type_list<int, constructor_config &>",
        dependency_forms=frozenset({"value", "lvalue_reference"}),
    ),
    ConstructorShape(
        name="defaulted",
        target_type="constructor_defaulted",
        expected_kind="dingo::detail::constructor_kind::concrete",
        expected_arity=0,
        signature_arguments="dingo::type_list<>",
        detector_only=True,
    ),
    ConstructorShape(
        name="generic",
        target_type="constructor_generic",
        expected_kind="dingo::detail::constructor_kind::generic",
        expected_arity="DINGO_CONSTRUCTOR_DETECTION_ARGS",
        signature_arguments="void",
        detector_only=True,
    ),
    ConstructorShape(
        name="ambiguous",
        target_type="constructor_ambiguous",
        expected_kind="dingo::detail::constructor_kind::invalid",
        expected_arity=0,
        signature_arguments="void",
        detector_only=True,
    ),
    ConstructorShape(
        name="explicit",
        target_type="constructor_explicit",
        expected_kind="dingo::detail::constructor_kind::concrete",
        expected_arity=1,
        signature_arguments="dingo::type_list<int>",
        dependency_forms=frozenset({"value"}),
    ),
    ConstructorShape(
        name="initializer_list",
        target_type="constructor_initializer_list",
        expected_kind="dingo::detail::constructor_kind::concrete",
        expected_arity="DINGO_CONSTRUCTOR_DETECTION_ARGS",
        signature_arguments="constructor_repeated_int_arguments",
        detector_only=True,
    ),
    ConstructorShape(
        name="same_arity_overload",
        target_type="constructor_same_arity_overload",
        expected_kind="dingo::detail::constructor_kind::invalid",
        expected_arity=0,
        signature_arguments="void",
        detector_only=True,
    ),
)

__all__ = (
    "CONSTRUCTOR_ARGUMENT_CATEGORIES",
    "CONSTRUCTOR_ARGUMENT_STORAGES",
    "CONSTRUCTOR_DETECTION_BACKENDS",
    "CONSTRUCTOR_DETECTION_MODES",
    "CONSTRUCTOR_SHAPES",
    "DEPENDENCY_CONSTRUCTOR_SHAPES",
)
