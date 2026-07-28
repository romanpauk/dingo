#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Bounded recursive dependency-wrapper compositions."""

from __future__ import annotations

from itertools import combinations

from axes.dependency_forms import NON_GNU_WRAPPER_SHAPE_LIMITATION
from schema import (
    ConstructorDetectionLimitation,
    DependencyComposition,
    DependencyCompositionOperator,
    DependencyCompositionRequestStrategy,
    DependencyCompositionResolutionLimitation,
    LimitationDisposition,
)


MAX_DEPENDENCY_COMPOSITION_DEPTH = 2


DEPENDENCY_COMPOSITION_REQUEST_STRATEGIES = (
    DependencyCompositionRequestStrategy(
        name="stable",
        type_expression="{0} &",
        uses_operator_expression=True,
    ),
    DependencyCompositionRequestStrategy(
        name="value",
        type_expression="{0}",
    ),
    DependencyCompositionRequestStrategy(
        name="rvalue",
        type_expression="{0} &&",
    ),
)


_STABLE_REQUEST = frozenset({"stable"})
_ALL_REQUEST_STRATEGIES = frozenset(
    strategy.name for strategy in DEPENDENCY_COMPOSITION_REQUEST_STRATEGIES
)


_NESTED_SMART_POINTER_REQUEST_LIMITATION = (
    DependencyCompositionResolutionLimitation(
        position="request_composed_operand",
        disposition=LimitationDisposition.KNOWN_GAP,
        reason=(
            "nested smart-pointer storage exposes inner conversion "
            "capabilities that cannot materialize the exact composition"
        ),
    )
)
_OWNING_ARRAY_OPTIONAL_REQUEST_LIMITATION = (
    DependencyCompositionResolutionLimitation(
        position="request_composed_operand",
        disposition=LimitationDisposition.KNOWN_GAP,
        reason=(
            "owning array requests use constructor-shape materialization and "
            "cannot construct an optional element"
        ),
        request_strategies=frozenset({"value", "rvalue"}),
        operand_operators=frozenset({"optional"}),
    )
)
DEPENDENCY_COMPOSITION_OPERATORS = (
    DependencyCompositionOperator(
        name="pointer",
        arity=1,
        type_expression="{0} *",
        copyability="always",
        movability="always",
        request_expression="{0}",
        supported_request_strategies=_STABLE_REQUEST,
        unsupported_request_disposition=(
            LimitationDisposition.INTENTIONAL_CONSTRAINT
        ),
    ),
    DependencyCompositionOperator(
        name="const_pointer",
        arity=1,
        type_expression="dependency_const_pointer<{0}>",
        copyability="always",
        movability="always",
        request_expression="{0}",
        supported_request_strategies=_STABLE_REQUEST,
        unsupported_request_disposition=(
            LimitationDisposition.INTENTIONAL_CONSTRAINT
        ),
    ),
    DependencyCompositionOperator(
        name="shared_pointer",
        arity=1,
        type_expression="std::shared_ptr<{0}>",
        copyability="always",
        movability="always",
        request_expression="{0} &",
        supported_request_strategies=_ALL_REQUEST_STRATEGIES,
        resolution_limitations=(_NESTED_SMART_POINTER_REQUEST_LIMITATION,),
    ),
    DependencyCompositionOperator(
        name="unique_pointer",
        arity=1,
        type_expression="std::unique_ptr<{0}>",
        copyability="never",
        movability="always",
        request_expression="{0} &",
        supported_request_strategies=_ALL_REQUEST_STRATEGIES,
        resolution_limitations=(_NESTED_SMART_POINTER_REQUEST_LIMITATION,),
    ),
    DependencyCompositionOperator(
        name="optional",
        arity=1,
        type_expression="std::optional<{0}>",
        copyability="all_operands",
        movability="all_operands",
        request_expression="{0} &",
        supported_request_strategies=_ALL_REQUEST_STRATEGIES,
    ),
    DependencyCompositionOperator(
        name="array",
        arity=1,
        type_expression="std::array<{0}, 2>",
        copyability="all_operands",
        movability="all_operands",
        request_expression="{0} &",
        supported_request_strategies=_ALL_REQUEST_STRATEGIES,
        resolution_limitations=(_OWNING_ARRAY_OPTIONAL_REQUEST_LIMITATION,),
    ),
    DependencyCompositionOperator(
        name="variant",
        arity=2,
        type_expression="std::variant<{0}, {1}>",
        copyability="all_operands",
        movability="all_operands",
        request_expression="{0} &",
        supported_request_strategies=_ALL_REQUEST_STRATEGIES,
    ),
)


DEPENDENCY_COMPOSITION_LEAVES = (
    DependencyComposition(
        name="regular",
        type_name="dependency_regular",
        copyable=True,
        movable=True,
    ),
    DependencyComposition(
        name="move_only",
        type_name="dependency_move_only",
        movable=True,
    ),
    DependencyComposition(
        name="copy_only",
        type_name="dependency_copy_only",
        copyable=True,
    ),
)


_OPERATORS_BY_NAME = {
    operator.name: operator for operator in DEPENDENCY_COMPOSITION_OPERATORS
}


def _composition_property(
    rule: str,
    operand_properties: tuple[bool, ...],
) -> bool:
    if rule == "always":
        return True
    if rule == "never":
        return False
    if rule == "all_operands":
        return all(operand_properties)
    raise ValueError(f"unknown dependency composition property rule: {rule}")


def compose_dependency(
    name: str,
    operator: str,
    *operands: DependencyComposition,
) -> DependencyComposition:
    operator_spec = _OPERATORS_BY_NAME[operator]
    return DependencyComposition(
        name=name,
        operator=operator_spec,
        operands=tuple(operands),
        copyable=_composition_property(
            operator_spec.copyability,
            tuple(operand.copyable for operand in operands),
        ),
        movable=_composition_property(
            operator_spec.movability,
            tuple(operand.movable for operand in operands),
        ),
    )


_COMPOSITION_NAME_OVERRIDES = {
    "optional_shared_pointer_regular": "nested_optional_shared_pointer",
    "optional_unique_pointer_regular": "optional_unique_pointer",
    "variant_optional_regular_move_only": "nested_variant_optional",
    (
        "variant_shared_pointer_regular_unique_pointer_regular"
    ): "variant_shared_unique_pointers",
    "variant_move_only_copy_only": "variant_move_copy_only",
}

_OPTIONAL_COPY_ONLY_COMPOSITE_SHAPE_LIMITATION = (
    ConstructorDetectionLimitation(
        backend=None,
        mode="shape",
        reason=(
            "constructor shape detection cannot identify an optional "
            "containing a copy-only composite through the opaque conversion "
            "probe"
        ),
    ),
)

_OPTIONAL_POINTER_SHAPE_LIMITATION = (
    ConstructorDetectionLimitation(
        backend=None,
        mode="shape",
        reason=(
            "constructor shape detection cannot identify an optional "
            "containing a pointer through the opaque conversion probe"
        ),
    ),
)

_COMPOSITION_LIMITATIONS = {
    name: _OPTIONAL_COPY_ONLY_COMPOSITE_SHAPE_LIMITATION
    for name in (
        "optional_array_copy_only",
        "optional_variant_regular_copy_only",
    )
}


def _composition_limitations(
    name: str,
    operator: DependencyCompositionOperator,
    operands: tuple[DependencyComposition, ...],
) -> tuple[ConstructorDetectionLimitation, ...]:
    limitations = _COMPOSITION_LIMITATIONS.get(name, ())
    if (
        operator.name == "optional"
        and operands[0].operator is not None
        and operands[0].operator.name in {"pointer", "const_pointer"}
    ):
        limitations += _OPTIONAL_POINTER_SHAPE_LIMITATION
    non_gnu_ambiguous_wrapper = operator.name == "optional" or (
        operator.name == "variant"
        and any(
            operand.operator is not None
            and operand.operator.name == "optional"
            for operand in operands
        )
    )
    has_unconditional_shape_limitation = any(
        limitation.mode == "shape"
        and limitation.guard is None
        and limitation.backend is None
        for limitation in limitations
    )
    if non_gnu_ambiguous_wrapper and not has_unconditional_shape_limitation:
        limitations += (NON_GNU_WRAPPER_SHAPE_LIMITATION,)
    return limitations


def _composition_name(
    operator: DependencyCompositionOperator,
    operands: tuple[DependencyComposition, ...],
) -> str:
    generated = "_".join(
        (operator.name, *(operand.name for operand in operands))
    )
    return _COMPOSITION_NAME_OVERRIDES.get(generated, generated)


def _build_dependency_compositions(
    operators: tuple[DependencyCompositionOperator, ...],
    leaves: tuple[DependencyComposition, ...],
    max_depth: int,
) -> tuple[DependencyComposition, ...]:
    levels = [leaves]
    all_nodes = list(leaves)
    result: list[DependencyComposition] = []
    for _ in range(max_depth):
        previous_level = levels[-1]
        level: list[DependencyComposition] = []
        for operator in operators:
            operand_sets = (
                ((operand,) for operand in previous_level)
                if operator.arity == 1
                else (
                    operands
                    for operands in combinations(all_nodes, operator.arity)
                    if any(operand in previous_level for operand in operands)
                )
            )
            for operands in operand_sets:
                operands = tuple(
                    sorted(
                        operands,
                        key=lambda operand: operand not in previous_level,
                    )
                )
                name = _composition_name(operator, operands)
                level.append(
                    DependencyComposition(
                        name=name,
                        operator=operator,
                        operands=operands,
                        copyable=_composition_property(
                            operator.copyability,
                            tuple(operand.copyable for operand in operands),
                        ),
                        movable=_composition_property(
                            operator.movability,
                            tuple(operand.movable for operand in operands),
                        ),
                        constructor_detection_limitations=_composition_limitations(
                            name,
                            operator,
                            operands,
                        ),
                    )
                )
        levels.append(tuple(level))
        all_nodes.extend(level)
        result.extend(level)
    return tuple(result)


DEPENDENCY_COMPOSITIONS = _build_dependency_compositions(
    DEPENDENCY_COMPOSITION_OPERATORS,
    DEPENDENCY_COMPOSITION_LEAVES,
    MAX_DEPENDENCY_COMPOSITION_DEPTH,
)


def dependency_composition_depth(composition: DependencyComposition) -> int:
    if composition.operator is None:
        return 0
    return 1 + max(
        dependency_composition_depth(operand)
        for operand in composition.operands
    )


def render_dependency_composition(composition: DependencyComposition) -> str:
    if composition.operator is None:
        if composition.type_name is None:
            raise ValueError(
                f"dependency composition leaf {composition.name} has no type"
            )
        return composition.type_name
    return composition.operator.type_expression.format(
        *(
            render_dependency_composition(operand)
            for operand in composition.operands
        )
    )


def render_dependency_composition_request(
    composition: DependencyComposition,
    strategy: DependencyCompositionRequestStrategy | None = None,
) -> str:
    if composition.operator is None:
        raise ValueError(
            f"dependency composition {composition.name} has no outer operator"
        )
    expression = (
        composition.operator.request_expression
        if strategy is None or strategy.uses_operator_expression
        else strategy.type_expression
    )
    return expression.format(
        render_dependency_composition(composition)
    )


def validate_dependency_compositions(
    compositions: tuple[DependencyComposition, ...] = DEPENDENCY_COMPOSITIONS,
    operators: tuple[
        DependencyCompositionOperator, ...
    ] = DEPENDENCY_COMPOSITION_OPERATORS,
    leaves: tuple[DependencyComposition, ...] = DEPENDENCY_COMPOSITION_LEAVES,
    max_depth: int = MAX_DEPENDENCY_COMPOSITION_DEPTH,
) -> None:
    operators_by_name = {operator.name: operator for operator in operators}
    if len(operators_by_name) != len(operators):
        raise ValueError("dependency composition operators contain duplicates")
    leaves_by_name = {leaf.name: leaf for leaf in leaves}
    if len(leaves_by_name) != len(leaves):
        raise ValueError("dependency composition leaves contain duplicates")
    if len({composition.name for composition in compositions}) != len(
        compositions
    ):
        raise ValueError("dependency compositions contain duplicate cases")
    if max_depth < 1:
        raise ValueError("dependency composition maximum depth must be positive")
    property_rules = {"always", "never", "all_operands"}
    resolution_limitation_positions = {"request_composed_operand", "nested"}
    request_strategy_names = {
        strategy.name for strategy in DEPENDENCY_COMPOSITION_REQUEST_STRATEGIES
    }
    request_strategies_by_name = {
        strategy.name: strategy
        for strategy in DEPENDENCY_COMPOSITION_REQUEST_STRATEGIES
    }
    if len(request_strategy_names) != len(
        DEPENDENCY_COMPOSITION_REQUEST_STRATEGIES
    ):
        raise ValueError(
            "dependency composition request strategies contain duplicates"
        )
    for operator in operators:
        if (
            operator.copyability not in property_rules
            or operator.movability not in property_rules
        ):
            raise ValueError(
                f"dependency composition operator {operator.name} has an "
                "unknown copy/move rule"
            )
        if (
            not operator.supported_request_strategies
            or not operator.supported_request_strategies
            <= request_strategy_names
        ):
            raise ValueError(
                f"dependency composition operator {operator.name} has an "
                "invalid request strategy"
            )
        has_unsupported_requests = (
            operator.supported_request_strategies != request_strategy_names
        )
        if has_unsupported_requests != (
            operator.unsupported_request_disposition is not None
        ) or (
            operator.unsupported_request_disposition is not None
            and not isinstance(
                operator.unsupported_request_disposition,
                LimitationDisposition,
            )
        ):
            raise ValueError(
                f"dependency composition operator {operator.name} has an "
                "incomplete unsupported request disposition"
            )
        limitation_keys = [
            (
                limitation.position,
                limitation.request_strategies,
                limitation.operand_operators,
            )
            for limitation in operator.resolution_limitations
        ]
        if any(
            limitation.position not in resolution_limitation_positions
            or not limitation.reason
            or not isinstance(
                limitation.disposition,
                LimitationDisposition,
            )
            or not limitation.request_strategies <= request_strategy_names
            or not limitation.operand_operators <= {
                candidate.name for candidate in operators
            }
            for limitation in operator.resolution_limitations
        ) or len(limitation_keys) != len(set(limitation_keys)):
            raise ValueError(
                f"dependency composition operator {operator.name} has an "
                "invalid resolution limitation"
            )

    used_operators: set[str] = set()
    used_leaves: set[str] = set()
    visited: dict[str, DependencyComposition] = {}

    def validate(composition: DependencyComposition) -> None:
        previous = visited.setdefault(composition.name, composition)
        if previous != composition:
            raise ValueError(
                "dependency composition node has conflicting definitions: "
                f"{composition.name}"
            )
        if composition.operator is None:
            if composition.operands or composition.type_name is None:
                raise ValueError(
                    f"invalid dependency composition leaf: {composition.name}"
                )
            if composition.name not in leaves_by_name:
                raise ValueError(
                    "dependency composition references unknown leaf: "
                    f"{composition.name}"
                )
            if not composition.copyable and not composition.movable:
                raise ValueError(
                    f"dependency composition leaf {composition.name} must be "
                    "copyable or movable"
                )
            used_leaves.add(composition.name)
            return
        if composition.type_name is not None:
            raise ValueError(
                f"dependency composition {composition.name} has a direct type"
            )
        operator = operators_by_name.get(composition.operator.name)
        if operator != composition.operator:
            raise ValueError(
                "dependency composition references unknown operator: "
                f"{composition.operator.name}"
            )
        if len(composition.operands) != operator.arity:
            raise ValueError(
                f"dependency composition {composition.name} requires "
                f"{operator.arity} operands"
            )
        used_operators.add(operator.name)
        for operand in composition.operands:
            validate(operand)

    for composition in compositions:
        validate(composition)
        depth = dependency_composition_depth(composition)
        if depth == 0 or depth > max_depth:
            raise ValueError(
                f"dependency composition {composition.name} has depth {depth}; "
                f"expected 1..{max_depth}"
            )
        render_dependency_composition(composition)
        render_dependency_composition_request(composition)
        for strategy_name in composition.operator.supported_request_strategies:
            render_dependency_composition_request(
                composition,
                request_strategies_by_name[strategy_name],
            )

    unused_operators = sorted(operators_by_name.keys() - used_operators)
    unused_leaves = sorted(leaves_by_name.keys() - used_leaves)
    if unused_operators:
        raise ValueError(
            "dependency composition operators are unused: "
            f"{unused_operators}"
        )
    if unused_leaves:
        raise ValueError(
            f"dependency composition leaves are unused: {unused_leaves}"
        )

    expected = _build_dependency_compositions(operators, leaves, max_depth)
    actual_cells = {
        composition.name: (
            render_dependency_composition(composition),
            composition.copyable,
            composition.movable,
            composition.constructor_detection_limitations,
        )
        for composition in compositions
    }
    expected_cells = {
        composition.name: (
            render_dependency_composition(composition),
            composition.copyable,
            composition.movable,
            composition.constructor_detection_limitations,
        )
        for composition in expected
    }
    if actual_cells != expected_cells:
        missing = sorted(expected_cells.keys() - actual_cells.keys())
        extra = sorted(actual_cells.keys() - expected_cells.keys())
        changed = sorted(
            name
            for name in actual_cells.keys() & expected_cells.keys()
            if actual_cells[name] != expected_cells[name]
        )
        raise ValueError(
            "dependency compositions do not match the bounded product: "
            f"missing={missing}, extra={extra}, changed={changed}"
        )
