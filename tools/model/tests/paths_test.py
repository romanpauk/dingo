#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import pytest

from tools.model.grammar.bindings import ANNOTATED, CONCRETE, INTERFACE, KEYED
from tools.model.domain.cases import counts, iter_cases, plain_forms, request_forms
from tools.model.report.paths import build_report, render
from tools.model.semantics.support import classify, iter_verdicts, to_shape
from tools.model.grammar.expr import (
    annotated,
    array,
    array_pointer,
    array_reference,
    const,
    interface,
    keyed,
    leaf,
    lvalue_reference,
    optional,
    pointer,
    rvalue_reference,
    shared_array,
    shared_ptr,
    unique_array,
    unique_ptr,
    valid,
    variant,
)


def test_grammar_renders_canonical_types() -> None:
    base = leaf()
    assert base.render() == "T"
    assert const(base).render() == "const T"
    assert lvalue_reference(base).render() == "T &"
    assert rvalue_reference(base).render() == "T &&"
    assert pointer(base).render() == "T *"
    assert pointer(const(base)).render() == "const T *"
    assert const(pointer(base)).render() == "T * const"
    assert array(base, 2).render() == "T[2]"
    assert array(array(base, 2), 3).render() == "T[2][3]"
    assert array_reference(base, 2).render() == "T (&)[2]"
    assert array_pointer(base, 2).render() == "T (*)[2]"
    assert optional(base).render() == "std::optional<T>"
    assert unique_ptr(base).render() == "std::unique_ptr<T>"
    assert shared_ptr(base).render() == "std::shared_ptr<T>"
    assert unique_array(base).render() == "std::unique_ptr<T[]>"
    assert shared_array(base).render() == "std::shared_ptr<T[]>"
    assert variant(base, leaf("long")).render() == "std::variant<T, long>"
    assert annotated(base).render() == "dingo::annotated<T, tag_a>"


def test_grammar_rejects_invalid_compositions() -> None:
    base = leaf()
    reference = lvalue_reference(base)
    assert const(reference) is None
    assert lvalue_reference(reference) is None
    assert rvalue_reference(reference) is None
    assert pointer(reference) is None
    assert optional(reference) is None
    assert unique_ptr(reference) is None
    assert shared_ptr(reference) is None
    assert unique_array(array(base, 2)) is None
    assert shared_array(array(base, 2)) is None
    assert array_reference(array(base, 2), 3) is None
    assert variant(reference, leaf("long")) is None


def test_domain_is_bounded_and_deterministic() -> None:
    first = counts()
    second = counts()
    assert first == second
    assert first["requests"] == len(request_forms())
    assert 0 < first["plain"] < 100
    assert 0 < first["wrapped"] < 500
    assert 0 < first["variants"] < 500
    assert 0 < first["requests"] < 20000
    assert first["cases"] == len(iter_cases(request_forms()))
    assert first["cases"] == len(iter_cases(request_forms()))


def test_every_enumerated_form_is_valid_and_unique() -> None:
    forms = request_forms()
    assert len(forms) == len(set(forms))
    assert all(valid(expr) for expr in forms)
    assert all(valid(expr) for expr in plain_forms())


def test_composed_objects_get_access_forms() -> None:
    base = leaf()
    forms = request_forms()
    assert lvalue_reference(optional(base)) in forms
    assert pointer(optional(base)) in forms
    assert lvalue_reference(shared_ptr(base)) in forms
    assert lvalue_reference(variant(const(base), leaf("long"))) in forms
    assert pointer(variant(const(base), leaf("long"))) in forms


def test_known_paths_map_to_model_shapes() -> None:
    base = leaf()
    assert to_shape(base).cpp_type == "int"
    assert to_shape(lvalue_reference(base)).cpp_type == "int &"
    assert to_shape(pointer(const(base))).cpp_type == "const int *"
    assert to_shape(optional(base)).cpp_type == "std::optional<int>"
    assert to_shape(unique_ptr(base)).cpp_type == "std::unique_ptr<int>"
    assert to_shape(shared_ptr(base)).cpp_type == "std::shared_ptr<int>"
    assert to_shape(array_reference(base, 2)).cpp_type == "int (&)[2]"
    assert to_shape(array_pointer(base, 2)).cpp_type == "int (*)[2]"
    assert to_shape(array_reference(base, 3)) is None
    assert to_shape(variant(base, leaf("long"))).cpp_type == (
        "std::variant<int, long>"
    )
    assert to_shape(annotated(base)) is None


def test_qualified_paths_are_classified_not_skipped() -> None:
    from tools.model.semantics.support import classify_case

    qualified = [
        case
        for case in iter_cases(request_forms())
        if case.binding.kind != "concrete"
    ]
    assert qualified
    assert any(
        case.binding.kind == "annotated" and case.scope == "unique"
        for case in qualified
    )
    for case in qualified:
        verdict = classify_case(case)
        assert verdict.outcome in {
            "supported",
            "unavailable",
            "unsupported",
            "unmodeled",
        }


def test_every_case_has_a_classified_verdict() -> None:
    for case, verdict in iter_verdicts(iter_cases(request_forms())):
        assert verdict.outcome in {
            "supported",
            "unavailable",
            "unsupported",
            "unmodeled",
        }
        if verdict.outcome != "supported":
            assert verdict.reason, case


def test_const_storage_uses_conversion_capabilities() -> None:
    base = leaf()
    reference = lvalue_reference(const(base))
    assert (
        classify(
            "shared",
            "shared_handle_shared_ptr_std_shared_ptr_const_t",
            CONCRETE,
            reference,
        ).outcome
        == "supported"
    )
    assert (
        classify(
            "external",
            "pointer_pointer_const_t",
            CONCRETE,
            reference,
        ).outcome
        == "supported"
    )
    # A consuming scope cannot give a const leaf away.
    assert (
        classify(
            "unique",
            "shared_handle_shared_ptr_std_shared_ptr_const_t",
            CONCRETE,
            reference,
        ).outcome
        == "unsupported"
    )
    # An optional holds its leaf inline: it can copy, not lend.
    assert (
        classify(
            "shared",
            "optional_optional_std_optional_const_t",
            CONCRETE,
            reference,
        ).outcome
        == "unsupported"
    )
    assert (
        classify(
            "shared",
            "optional_optional_std_optional_const_t",
            CONCRETE,
            base,
        ).outcome
        == "supported"
    )
    # Const value storage has no conversion route.
    assert (
        classify(
            "shared",
            "plain_const_const_t",
            CONCRETE,
            base,
        ).outcome
        == "unsupported"
    )
    # A variant is usable as a whole, but a const alternative has no route.
    whole = lvalue_reference(variant(const(base), leaf("long")))
    assert (
        classify(
            "shared",
            "alternative_variant_std_variant_const_t_long",
            CONCRETE,
            whole,
        ).outcome
        == "supported"
    )
    assert (
        classify(
            "shared",
            "alternative_variant_std_variant_const_t_long",
            CONCRETE,
            reference,
        ).outcome
        == "unsupported"
    )
    # The cyclical scope cannot hold a variant at all.
    assert (
        classify(
            "shared_cyclical",
            "alternative_variant_std_variant_const_t_long",
            CONCRETE,
            whole,
        ).outcome
        == "unsupported"
    )


def test_top_level_const_and_arrays_are_explicitly_unsupported() -> None:
    base = leaf()
    scope = "shared"
    assert classify(scope, "plain", CONCRETE, const(base)).outcome == "unsupported"
    assert classify(scope, "plain", CONCRETE, array(base, 2)).outcome == "unsupported"
    assert (
        classify(scope, "bounded_array_3", CONCRETE, lvalue_reference(base)).outcome
        in {"supported", "unavailable", "unsupported"}
    )


def test_supported_shapes_are_identified() -> None:
    from tools.model.semantics.extended_shapes import (
        extended_shapes,
        supported_shapes,
    )

    shapes = supported_shapes()
    assert shapes
    assert len({shape.name for shape in shapes}) == len(shapes)
    missing = extended_shapes()
    assert len({shape.name for shape in missing}) == len(missing)
    names = {shape.name for shape in shapes}
    assert all(shape.name in names for shape in missing)


def test_extended_shapes_are_consumed_by_the_type_matrix() -> None:
    from tools.model.semantics.extended_shapes import extended_shapes
    from tools.model.emit.matrix import SHAPES

    names = {shape.name for shape in SHAPES}
    assert {shape.name for shape in extended_shapes()} <= names


def test_annotated_requests_use_the_annotated_binding() -> None:
    base = leaf()
    request = annotated(lvalue_reference(base))
    assert (
        classify("shared", "plain", ANNOTATED, request).outcome == "supported"
    )
    assert (
        classify("shared", "plain", CONCRETE, request).outcome == "unavailable"
    )
    assert (
        classify("unique", "plain", ANNOTATED, request).outcome == "unavailable"
    )
    assert (
        classify("shared", "bounded_array_2", ANNOTATED, request).outcome
        == "unavailable"
    )


def test_keyed_requests_use_the_keyed_binding() -> None:
    base = leaf()
    request = keyed(lvalue_reference(base))
    assert classify("shared", "plain", KEYED, request).outcome == "supported"
    assert classify("shared", "plain", CONCRETE, request).outcome == "unavailable"
    assert classify("unique", "plain", KEYED, request).outcome == "supported"
    assert (
        classify("shared", "shared_handle", KEYED, request).outcome
        == "supported"
    )


def test_interface_requests_use_the_interface_binding() -> None:
    base = leaf()
    request = interface(lvalue_reference(base))
    assert classify("shared", "plain", INTERFACE, request).outcome == "supported"
    assert classify("shared", "plain", CONCRETE, request).outcome == "unavailable"
    assert (
        classify("shared", "bounded_array_2", INTERFACE, request).outcome
        == "unavailable"
    )
    assert (
        classify(
            "shared", "plain", INTERFACE, interface(optional(base))
        ).outcome
        == "unmodeled"
    )


def test_coverage_report_separates_tested_and_gap() -> None:
    from tools.model.report.coverage import coverage_report

    report = coverage_report()
    assert report["supported"] > 0
    assert report["compile_tested"] > 0
    assert report["runtime_tested"] > 0
    assert report["supported"] == (
        report["compile_tested"] + report["compile_gap"]
    )
    assert report["supported"] == (
        report["runtime_tested"]
        + report["runtime_gap"]
        + report["declared_gap"]
    )
    assert set(report["gap_by_binding"]) >= {
        "annotated",
        "interface",
        "keyed",
    }


def test_coverage_is_split_by_leaf_trait() -> None:
    from tools.model.report.coverage import coverage_report

    report = coverage_report()
    assert set(report["supported_by_trait"]) == {"copyable", "move_only"}
    assert sum(report["supported_by_trait"].values()) == report["supported"]
    assert report["compile_gap_by_trait"] == {
        "copyable": 0,
        "move_only": 0,
    }
    assert report["runtime_gap_by_trait"] == {}
    assert set(report["declared_gap_by_trait"]) == {"copyable", "move_only"}


def test_report_is_deterministic() -> None:
    first = build_report()
    second = build_report()
    assert first == second
    content = render(first)
    assert "# Type Path Coverage" in content
    assert "## By request family" in content
    assert "## By storage" in content
    assert "## Unmodeled examples" in content
    assert first["totals"]["supported"] > 0


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
