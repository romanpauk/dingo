#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Classify type paths as supported, unavailable, unsupported, or unmodeled.

The grammar (`grammar/expr.py`) enumerates what can be written. This module answers
what the current conversion model can serve. A path that is outside the model's
known forms, shapes, or rules is reported as `unmodeled` -- never silently
dropped.
"""

from __future__ import annotations

from dataclasses import dataclass
from functools import lru_cache

from spec.grammar.catalog import CONSUMING_SCOPES
from spec.domain.cases import Case
from spec.semantics.routes.ledger import build_rows
from spec.semantics.routes.derivation import leaf_copy_required
from spec.semantics.routes.rules import derive_route
from spec.grammar.traits import DEFAULT_TRAITS, LeafTraits
from spec.grammar.expr import (
    Expr,
    INDIRECTION_OPS,
    constructors as constructor_specs,
    leaf,
)
from spec.grammar.shapes import BASE, WRAPPER_BY_NAME, Shape

from spec.semantics.capabilities import binding_supported, storage_form
from spec.grammar.storage import const_leaf, has_const, storage_path

_SHAPE_WRAPPERS = {
    "const": "const",
    "lvalue_reference": "lvalue_reference",
    "rvalue_reference": "rvalue_reference",
    "pointer": "pointer",
    "optional": "optional",
    "unique_ptr": "unique_ptr",
    "shared_ptr": "shared_ptr",
    "unique_array": "unique_array",
    "shared_array": "shared_array",
    "array_reference": "array_lvalue_reference",
    "array_pointer": "array_pointer",
}
for _spec in (
    *constructor_specs("qualifier"),
    *constructor_specs("wrapper"),
):
    if _spec.name not in _SHAPE_WRAPPERS:
        raise ValueError(f"constructor {_spec.name!r} has no shape mapping")

INTERFACE_INNER_OPS = frozenset(
    {"leaf", "lvalue_reference", "pointer", "shared_ptr", "unique_ptr"}
)


def _direct_interface_target(expr: Expr) -> bool:
    """Whether the request addresses the interface itself, not a nested wrap."""
    if expr.op == "const":
        return _direct_interface_target(expr.inner)
    if expr.op in {"lvalue_reference", "rvalue_reference", "pointer"}:
        return _direct_interface_target(expr.inner)
    if expr.op in {"shared_ptr", "unique_ptr"}:
        inner = expr.inner
        if inner.op == "const":
            inner = inner.inner
        return inner.op == "leaf"
    return expr.op == "leaf"


def _interface_handle(expr: Expr) -> Expr | None:
    while expr.op in {"const", "lvalue_reference", "rvalue_reference", "pointer"}:
        expr = expr.inner
    if expr.op in {"shared_ptr", "unique_ptr"}:
        return expr
    return None


def _annotated_target(expr: Expr) -> bool:
    """A request form annotation can wrap, with one native wrapper layer."""
    if expr.op in {"const", "lvalue_reference", "rvalue_reference", "pointer"}:
        return _annotated_target(expr.inner)
    if expr.op in {
        "optional",
        "unique_ptr",
        "shared_ptr",
        "unique_array",
        "shared_array",
        "variant",
    }:
        inner = expr.inner if expr.op != "variant" else expr.alternatives[0]
        if inner.op == "const":
            inner = inner.inner
        return inner.op == "leaf"
    return expr.op == "leaf"


def _annotated_wrapper_matches(form, inner: Expr) -> bool:
    if inner.op == "optional":
        return form.kind == "optional"
    if inner.op == "shared_ptr":
        return form.name == "shared_handle"
    if inner.op == "unique_ptr":
        return form.name == "unique_handle"
    if inner.op == "unique_array":
        return form.name == "unique_array_handle"
    if inner.op == "shared_array":
        return form.name == "shared_array_handle"
    if inner.op == "variant":
        return form.kind == "variant"
    if inner.op in {"array", "array_reference", "array_pointer"}:
        return form.kind == "array"
    return True


def _annotated_wrapper_const_matches(storage: Expr, request: Expr) -> bool:
    base = request
    accessed = False
    while base.op in {"lvalue_reference", "rvalue_reference", "pointer"}:
        accessed = True
        base = base.inner
    if base.op not in {
        "optional",
        "unique_ptr",
        "shared_ptr",
        "unique_array",
        "shared_array",
        "variant",
    }:
        return True
    stored_const = const_leaf(storage) or (
        storage.op == "variant" and has_const(storage)
    )
    inner = base.inner if base.op != "variant" else base.alternatives[0]
    request_const = inner.op == "const"
    if accessed and request_const and not stored_const:
        return False
    if stored_const and not request_const:
        return False
    return True


_LEDGER = {
    (row.scope, row.form, row.category): row for row in build_rows()
}


@dataclass(frozen=True, slots=True)
class Verdict:
    outcome: str  # supported, unavailable, unsupported, unmodeled
    rule: str = ""
    detail: str = ""

    @property
    def reason(self) -> str:
        if self.rule and self.detail:
            return f"{self.rule}: {self.detail}"
        return self.detail


def _apply(shape: Shape, wrapper_name: str) -> Shape | None:
    try:
        return shape.apply(WRAPPER_BY_NAME[wrapper_name])
    except ValueError:
        return None


@lru_cache(maxsize=None)
def to_shape(expr: Expr) -> Shape | None:
    """Map a path into the model's request-shape vocabulary, when it fits."""
    if expr.op == "leaf":
        return BASE if expr.name == "T" else None
    if expr.op == "annotated":
        return None
    if expr.op == "array":
        return None
    if expr.op in {"array_reference", "array_pointer"}:
        if expr.extent != 2:
            return None
        inner = to_shape(expr.inner)
        if inner is None:
            return None
        return _apply(inner, _SHAPE_WRAPPERS[expr.op])
    if expr.op == "variant":
        if len(expr.alternatives) != 2:
            return None
        first, second = expr.alternatives
        if second != leaf("long"):
            return None
        inner = to_shape(first)
        if inner is None:
            return None
        return _apply(inner, "variant")
    if expr.inner is None:
        return None
    if expr.op == "const" and expr.inner.op in {
        "pointer",
        "array",
        "array_reference",
        "array_pointer",
    }:
        # `const` over a pointer/array is not the model's request spelling.
        return None
    inner = to_shape(expr.inner)
    if inner is None:
        return None
    wrapper = _SHAPE_WRAPPERS.get(expr.op)
    if wrapper is None:
        return None
    return _apply(inner, wrapper)


def _category(shape: Shape) -> str:
    if shape.access == "value":
        return "value"
    if shape.access == "rvalue_reference":
        return "rvalue"
    return "object"


def _contains_wrapper(shape: Shape, name: str) -> bool:
    current: Shape | None = shape
    while current is not None:
        if current.outer is not None and current.outer.name == name:
            return True
        current = current.inner
    return False


def _subtree_has_const(shape: Shape | None) -> bool:
    current = shape
    while current is not None:
        if current.outer is not None and current.outer.is_const:
            return True
        current = current.inner
    return False


def _has_mutable_access(shape: Shape) -> bool:
    current: Shape | None = shape
    while current is not None:
        outer = current.outer
        if (
            outer is not None
            and outer.is_access
            and not _subtree_has_const(current.inner)
        ):
            return True
        current = current.inner
    return False


def _has_access(shape: Shape) -> bool:
    current: Shape | None = shape
    while current is not None:
        if current.outer is not None and current.outer.is_access:
            return True
        current = current.inner
    return False


def _is_const_leaf(shape: Shape | None) -> bool:
    if shape is None or shape.outer is None or shape.outer.name != "const":
        return False
    inner = shape.inner
    return inner is not None and inner.outer is None


def _indirection_layers(shape: Shape) -> list[Shape]:
    layers = []
    current: Shape | None = shape
    while current is not None:
        if current.outer is not None and current.outer.name in INDIRECTION_OPS:
            layers.append(current)
        current = current.inner
    return layers


def _target_variant(shape: Shape) -> Shape | None:
    current = shape
    while current.outer is not None and current.outer.is_access:
        current = current.inner
    if current.outer is not None and current.outer.name == "variant":
        return current
    return None


def _variant_keeps_const(variant: Shape) -> bool:
    inner = variant.inner
    return (
        inner is not None
        and inner.outer is not None
        and inner.outer.name == "const"
        and inner.inner is not None
        and inner.inner.outer is None
    )


def _const_conversion(form, shape: Shape) -> Verdict | None:
    """The conversions a const-qualified storage can offer.

    A const leaf can be copied, borrowed, or held behind one indirection. It
    cannot be turned into a mutable access, it cannot be wrapped in a second
    handle/pointer layer, and inline storage (an optional or a variant) cannot
    lend an indirection at all. A variant request must also keep the stored
    alternative's constness; the whole variant may be copied or borrowed, but
    its const alternative cannot be replaced by a mutable one.
    """
    if form.kind != "variant" and _contains_wrapper(shape, "variant"):
        return Verdict(
            "unsupported",
            "const",
            "const storage cannot convert to a variant alternative",
        )
    if form.kind == "array_handle":
        return Verdict(
            "unsupported",
            "const",
            "a const dynamic array needs an explicit factory",
        )
    if form.kind == "array":
        return Verdict(
            "unsupported",
            "const",
            "a const array storage has no conversion route",
        )
    if _has_mutable_access(shape):
        return Verdict(
            "unsupported",
            "const",
            "const storage cannot yield a mutable reference or pointer",
        )

    if form.kind == "variant":
        target = _target_variant(shape)
        if target is None or not _variant_keeps_const(target):
            return Verdict(
                "unsupported",
                "const",
                "a const variant alternative has no conversion route",
            )
        return None

    layers = _indirection_layers(shape)
    if form.kind in {"optional", "value"}:
        if layers or _has_access(shape):
            return Verdict(
                "unsupported",
                "const",
                "const inline storage cannot lend an indirection",
            )
        return None

    if form.kind in {"pointer", "handle"}:
        if len(layers) > 1:
            return Verdict(
                "unsupported",
                "const",
                "const storage supports one indirection layer",
            )
        for layer in layers:
            if not _is_const_leaf(layer.inner):
                return Verdict(
                    "unsupported",
                    "const",
                    "const storage cannot produce a mutable pointee",
                )
        return None

    return Verdict(
        "unsupported",
        "const",
        "const storage has no value copy route",
    )


def _keyed_leaf_request(expr: Expr) -> bool:
    while expr.op in {
        "lvalue_reference",
        "rvalue_reference",
        "pointer",
        "const",
    }:
        expr = expr.inner
    return expr.op == "leaf"


def _keyed_capability(
    scope: str, form, path, request: Expr, shape: Shape
) -> Verdict | None:
    """The keyed lookup route only serves direct leaf access.

    Keying a registration does not add the conversions needed to wrap or
    unwrap a handle, optional, or variant. Those requests remain outside the
    keyed capability until each conversion route is verified.
    """
    if not _keyed_leaf_request(request):
        return Verdict(
            "unsupported",
            "keyed",
            "a keyed lookup only serves direct leaf access",
        )
    if form.kind == "array":
        if path.expr.op == "array" and path.expr.inner.op == "array":
            return Verdict(
                "unsupported",
                "keyed",
                "a nested array keyed lookup has no conversion route",
            )
        if scope in CONSUMING_SCOPES and shape.access != "pointer":
            return Verdict(
                "unsupported",
                "keyed",
                "a consuming keyed array lookup only serves pointer access",
            )
    if scope in CONSUMING_SCOPES:
        if form.kind in {"handle", "array_handle", "optional"}:
            return Verdict(
                "unsupported",
                "keyed",
                "a consuming keyed lookup does not serve handles or optionals",
            )
        if form.kind == "pointer" and shape.access != "pointer":
            return Verdict(
                "unsupported",
                "keyed",
                "a consuming keyed pointer lookup only serves pointer access",
            )
    return None


def _const_rvalue(request: Expr) -> bool:
    while request.op in {"annotated", "keyed", "interface"}:
        request = request.inner
    return (
        request.op == "rvalue_reference"
        and request.inner is not None
        and request.inner.op == "const"
    )


def _interface_target_failure(inner: Expr, binding) -> Verdict | None:
    if inner.op not in INTERFACE_INNER_OPS:
        return Verdict(
            "unmodeled",
            "interface",
            "this interface request form is not modeled",
        )
    if not _direct_interface_target(inner):
        return Verdict(
            "unmodeled",
            "interface",
            "nested interface conversion is not modeled",
        )
    handle = _interface_handle(inner)
    if handle is not None and handle.inner.op == "const":
        direct_shared = inner is handle and handle.op == "shared_ptr"
        if not direct_shared:
            return Verdict(
                "unsupported",
                "interface",
                "a const interface handle has no conversion route",
            )
    if inner.op == "leaf" and not binding.copyable:
        return Verdict(
            "unsupported",
            "interface",
            "an interface value request requires a copyable interface",
        )
    return None


@lru_cache(maxsize=None)
def classify(
    scope: str,
    storage_name: str,
    binding,
    request_key: Expr,
    traits: LeafTraits = DEFAULT_TRAITS,
) -> Verdict:
    if _const_rvalue(request_key):
        return Verdict(
            "unsupported",
            "move",
            "a const rvalue request has no conversion route",
        )
    if request_key.op == "const":
        return Verdict(
            "unsupported",
            "const",
            "a top-level const value is not a request",
        )
    if request_key.op == "array":
        return Verdict(
            "unsupported",
            "decay",
            "array values are addressable only through a reference or pointer",
        )
    if (
        request_key.op == "annotated"
        and request_key.inner.op == "interface"
    ):
        if not binding.exposes(request_key):
            return Verdict(
                "unavailable",
                "annotation",
                "the binding does not expose this annotated interface",
            )
        form = storage_form(storage_name)
        if form is None:
            return Verdict(
                "unmodeled",
                "storage",
                f"{storage_name} has no form in the storage model",
            )
        if not binding_supported(scope, storage_name, binding):
            return Verdict(
                "unavailable",
                "binding",
                f"{scope}/{form} does not expose the annotated interface",
            )
        failure = _interface_target_failure(request_key.inner.inner, binding)
        if failure is not None:
            return failure
        request_key = request_key.inner.inner
        annotated = False
    else:
        annotated = request_key.op == "annotated"
    if annotated:
        if not binding.exposes(request_key):
            return Verdict(
                "unavailable",
                "annotation",
                "the binding does not expose this annotation tag",
            )
        form = storage_form(storage_name)
        if form is None:
            return Verdict(
                "unmodeled",
                "storage",
                f"{storage_name} has no form in the storage model",
            )
        if not binding_supported(scope, storage_name, binding):
            return Verdict(
                "unavailable",
                "binding",
                f"{scope}/{form} does not expose the annotated binding",
            )
        if not _annotated_target(request_key.inner):
            return Verdict(
                "unmodeled",
                "annotation",
                "this annotated request form is not modeled",
            )
        if not _annotated_wrapper_matches(form, request_key.inner):
            return Verdict(
                "unsupported",
                "annotation",
                "the storage form does not expose this annotated wrapper",
            )
        path = storage_path(scope, storage_name)
        if path is not None and not _annotated_wrapper_const_matches(
            path.expr, request_key.inner
        ):
            return Verdict(
                "unsupported",
                "annotation",
                "the annotated wrapper constness does not match the storage",
            )
        request_key = request_key.inner
    elif request_key.op == "keyed":
        if not binding.exposes(request_key):
            return Verdict(
                "unavailable",
                "key",
                "the binding does not expose this key",
            )
        form = storage_form(storage_name)
        if form is None:
            return Verdict(
                "unmodeled",
                "storage",
                f"{storage_name} has no form in the storage model",
            )
        if not binding_supported(scope, storage_name, binding):
            return Verdict(
                "unavailable",
                "binding",
                f"{scope}/{form} does not expose the keyed binding",
            )
        request_key = request_key.inner
    elif request_key.op == "interface":
        if not binding.exposes(request_key):
            return Verdict(
                "unavailable",
                "interface",
                "the binding does not expose this interface",
            )
        form = storage_form(storage_name)
        if form is None:
            return Verdict(
                "unmodeled",
                "storage",
                f"{storage_name} has no form in the storage model",
            )
        if not binding_supported(scope, storage_name, binding):
            return Verdict(
                "unavailable",
                "binding",
                f"{scope}/{form} does not expose the interface binding",
            )
        failure = _interface_target_failure(request_key.inner, binding)
        if failure is not None:
            return failure
        request_key = request_key.inner
    elif binding.kind == "annotated":
        return Verdict(
            "unavailable",
            "annotation",
            "an annotated binding only exposes annotated requests",
        )
    elif binding.kind == "keyed":
        return Verdict(
            "unavailable",
            "key",
            "a keyed binding only exposes keyed requests",
        )
    elif binding.kind == "interface":
        return Verdict(
            "unavailable",
            "interface",
            "an interface binding only exposes interface requests",
        )

    form = storage_form(storage_name)
    if form is None:
        return Verdict(
            "unmodeled",
            "storage",
            f"{storage_name} has no form in the storage model",
        )
    shape = to_shape(request_key)
    if shape is None:
        return Verdict(
            "unmodeled",
            "request",
            "the request spelling is outside the shape model",
        )
    path = storage_path(scope, storage_name)
    if binding.kind == "keyed" and path is not None:
        failure = _keyed_capability(
            scope, form, path, request_key, shape
        )
        if failure is not None:
            return failure
    if path is not None and path.expr.op == "variant" and path.scope.name == "shared_cyclical":
        return Verdict(
            "unsupported",
            "variant",
            "the cyclical scope cannot hold a variant",
        )
    if path is not None and path.expr.op == "variant" and has_const(path.expr):
        failure = _const_conversion(form, shape)
        if failure is not None:
            return failure
    if path is not None and const_leaf(path.expr):
        if path.scope.name in CONSUMING_SCOPES:
            return Verdict(
                "unsupported",
                "const",
                "a consuming scope cannot give away a const storage",
            )
        if path.scope.name == "shared_cyclical":
            return Verdict(
                "unsupported",
                "const",
                "the cyclical scope materializes through a mutable pointer",
            )
        if path.expr.op == "const" and path.expr.inner.op == "leaf":
            return Verdict(
                "unsupported",
                "const",
                "a const value storage has no conversion route",
            )
        failure = _const_conversion(form, shape)
        if failure is not None:
            return failure
    if (
        not traits.copyable
        and scope == "external"
        and path is not None
        and form.kind in {"optional", "variant"}
        and (const_leaf(path.expr) or has_const(path.expr))
    ):
        # The external registration moves or copies the caller instance into
        # the storage. A const inline wrapper whose element is move-only is
        # neither movable nor copyable.
        return Verdict(
            "unsupported",
            "registration",
            "a const inline wrapper with a move-only element cannot be "
            "registered externally",
        )
    row = _LEDGER.get((scope, form.name, _category(shape)))
    if row is None:
        return Verdict(
            "unmodeled",
            "storage form",
            f"{scope}/{form} is outside the route ledger",
        )
    if row.outcome != "supported":
        return Verdict("unavailable", "exposure", row.reason)
    failure = derive_route(scope, form.name, shape)
    if failure is not None:
        return Verdict("unsupported", failure.rule, failure.detail)
    if not traits.copyable and leaf_copy_required(scope, form.name, shape):
        return Verdict(
            "unsupported",
            "copy",
            "a move-only leaf cannot be copied out of this storage",
        )
    return Verdict("supported")


def classify_case(case: Case) -> Verdict:
    return classify(
        case.scope,
        case.storage_name,
        case.binding,
        case.request,
        case.traits,
    )


def iter_verdicts(cases):
    for case in cases:
        yield case, classify_case(case)


def validate() -> None:
    # A path must never be silently dropped: every classified verdict is one of
    # the four known outcomes, and a non-supported verdict carries a reason.
    from spec.grammar.bindings import CONCRETE

    from spec.grammar.storage import STORAGE_PATHS

    for path in STORAGE_PATHS:
        for request in (leaf("T"),):
            verdict = classify(path.scope.name, path.name, CONCRETE, request)
            if verdict.outcome not in {
                "supported",
                "unavailable",
                "unsupported",
                "unmodeled",
            }:
                raise ValueError(f"unknown outcome {verdict.outcome!r}")
            if verdict.outcome != "supported" and not verdict.reason:
                raise ValueError(
                    f"{path.scope.name}/{path.name}/"
                    f"{request.render()} has no reason"
                )
