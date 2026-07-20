#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT

"""Independent callable and provisioning axes for container invocation."""

from __future__ import annotations

from axes.containers import CONTAINERS
from axes.dependency_forms import (
    DEPENDENCY_FORMS,
    DEPENDENCY_PROVISIONINGS,
    render_dependency_request,
    render_dependency_type,
)
from axes.modes import REGISTRATION_MODES
from schema import DependencyForm, InvokeCallable


def _provisionings_supporting(
    dependency_forms: frozenset[str],
) -> frozenset[str]:
    return frozenset(
        provisioning.name
        for provisioning in DEPENDENCY_PROVISIONINGS
        if dependency_forms <= provisioning.supported_forms
    )


UNKEYED_PROVISIONINGS = _provisionings_supporting(
    frozenset({"lvalue_reference"})
)


def _scenario(
    name: str,
    *,
    dependency_forms: frozenset[str] = frozenset({"lvalue_reference"}),
    system_headers: tuple[str, ...] = (),
) -> InvokeCallable:
    return InvokeCallable(
        name=name,
        policy=f"resolution::scenario<invoke_{name}>",
        dependency_forms=dependency_forms,
        supported_provisionings=_provisionings_supporting(dependency_forms),
        system_headers=system_headers,
        headers=("matrix/scenarios/invoke.h",),
    )


def _dependency(
    name: str,
    request: str,
    value: str,
    dependency_form: str,
    *,
    explicit_signature: bool = False,
) -> InvokeCallable:
    forms = frozenset({dependency_form})
    policy = (
        "dependency_invoke_explicit"
        if explicit_signature
        else "dependency_invoke"
    )
    return InvokeCallable(
        name=name,
        policy=f"resolution::{policy}<{request}, {value}>",
        dependency_forms=forms,
        supported_provisionings=_provisionings_supporting(forms),
    )


def _plain_dependency(form: DependencyForm) -> InvokeCallable:
    base = "variant_a" if form.shape == "variant" else "value_type"
    return _dependency(
        f"{form.name}_dependency",
        render_dependency_request(form, base, "key_a"),
        render_dependency_type(form, base),
        form.name,
        explicit_signature=form.shape == "array",
    )


PLAIN_DEPENDENCY_CALLABLES = tuple(
    _plain_dependency(form)
    for form in DEPENDENCY_FORMS
    if form.decoration == "plain"
)


def _selected_dependency(form: DependencyForm) -> InvokeCallable:
    forms = frozenset({form.name})
    if form.carrier == "lvalue_reference":
        policy = (
            "resolution::keyed_invoke<value_type, "
            "dingo::key_type<key_a>, 3>"
        )
    elif form.carrier == "value":
        policy = (
            "resolution::keyed_collection_invoke<"
            "std::vector<std::shared_ptr<element_interface>>, "
            "dingo::key_type<key_a>, 2>"
        )
    else:
        raise ValueError(f"unsupported selected carrier: {form.carrier}")
    return InvokeCallable(
        name=f"{form.name}_dependency",
        policy=policy,
        dependency_forms=forms,
        supported_provisionings=_provisionings_supporting(forms),
    )


SELECTED_DEPENDENCY_CALLABLES = tuple(
    _selected_dependency(form)
    for form in DEPENDENCY_FORMS
    if form.decoration == "selected"
)


def _annotated_dependency(form: DependencyForm) -> InvokeCallable:
    if form.shape == "identity":
        base = "value_type"
        supported_provisionings = frozenset({"shared_annotated_value"})
    else:
        base = "interface_type"
        supported_provisionings = frozenset({"shared_annotated_interface"})
    request = render_dependency_request(form, base, "tag_a")
    value = render_dependency_type(form, base)
    # Hybrid containers cannot yet materialize an annotated wrapper from a
    # compile-time binding; keep that compatibility limit explicit in the axis.
    return InvokeCallable(
        name=f"{form.name}_dependency",
        policy=f"resolution::dependency_invoke<{request}, {value}>",
        dependency_forms=frozenset({form.name}),
        supported_provisionings=supported_provisionings,
        supported_containers=frozenset(
            container.name
            for container in CONTAINERS
            if container.name not in {"container_static", "container_mixed"}
        ),
    )


ANNOTATED_DEPENDENCY_CALLABLES = tuple(
    _annotated_dependency(form)
    for form in DEPENDENCY_FORMS
    if form.decoration == "annotated"
)


INVOKE_CALLABLES = (
    _scenario("inferred_lambda"),
    _scenario("explicit_signature_overload"),
    _scenario("std_function", system_headers=("functional",)),
    _scenario("free_function_pointer"),
    _scenario("noexcept_function_pointer"),
    _scenario("static_member_function_pointer"),
    _scenario("member_function_pointer"),
    _scenario("mutable_lambda"),
    _scenario("generic_lambda_explicit_signature"),
    _scenario("lvalue_ref_qualified_functor"),
    _scenario("rvalue_ref_qualified_functor"),
    _scenario("noexcept_functor"),
    _scenario("move_only_functor"),
    _scenario(
        "multi_argument_callable",
        dependency_forms=frozenset(
            {"lvalue_reference", "const_lvalue_reference"}
        ),
    ),
    _scenario("move_only_function", system_headers=("functional",)),
    *PLAIN_DEPENDENCY_CALLABLES,
    *SELECTED_DEPENDENCY_CALLABLES,
    *ANNOTATED_DEPENDENCY_CALLABLES,
)


_INVOKE_PROVISIONING_NAMES = frozenset().union(
    *(callable_spec.supported_provisionings for callable_spec in INVOKE_CALLABLES)
)
INVOKE_PROVISIONINGS = tuple(
    provisioning
    for provisioning in DEPENDENCY_PROVISIONINGS
    if provisioning.name in _INVOKE_PROVISIONING_NAMES
)
INVOKE_MODES = REGISTRATION_MODES
INVOKE_CONTAINERS = CONTAINERS

__all__ = (
    "ANNOTATED_DEPENDENCY_CALLABLES",
    "INVOKE_CALLABLES",
    "INVOKE_CONTAINERS",
    "INVOKE_PROVISIONINGS",
    "INVOKE_MODES",
    "PLAIN_DEPENDENCY_CALLABLES",
    "SELECTED_DEPENDENCY_CALLABLES",
    "UNKEYED_PROVISIONINGS",
)
