#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class ProbeLimit:
    default: int | None
    clang: int | None = None
    clang_arm64: int | None = None
    gcc: int | None = None
    gcc13: int | None = None
    gcc14: int | None = None
    gcc15: int | None = None
    gcc_arm64: int | None = None

    def as_dict(self) -> dict[str, int | None]:
        return {
            key: value
            for key, value in (
                ("default", self.default),
                ("clang", self.clang),
                ("clang_arm64", self.clang_arm64),
                ("gcc", self.gcc),
                ("gcc13", self.gcc13),
                ("gcc14", self.gcc14),
                ("gcc15", self.gcc15),
                ("gcc_arm64", self.gcc_arm64),
            )
            if value is not None or key == "default"
        }


@dataclass(frozen=True, slots=True)
class ProbeShape:
    name: str
    feature: str
    scope: str
    stored_type: str
    exposed_type: str
    resolved_type: str
    expression: str
    tiny_static: bool = False
    limits: dict[str, ProbeLimit] | None = None


MODE_PREFIXES = {
    "static": "static_resolution",
    "runtime": "runtime_resolution",
    "mixed": "static_resolution_mixed_container",
}


COMMON_TINY_LIMITS = {
    "static": ProbeLimit(default=0x10, gcc=0x50, gcc_arm64=0x60),
    "mixed": ProbeLimit(default=0x10, gcc=0x50, gcc_arm64=0x60),
    "runtime": ProbeLimit(default=None),
}


PROBE_SHAPES = (
    ProbeShape(
        name="value_ref",
        feature="resolve_concrete",
        scope="shared",
        stored_type="value_type",
        exposed_type="concrete",
        resolved_type="value_ref_ptr",
        expression="container.template resolve<value_type&>().marker()",
        tiny_static=True,
        limits=COMMON_TINY_LIMITS,
    ),
    ProbeShape(
        name="shared_value",
        feature="resolve_concrete",
        scope="shared",
        stored_type="value_type",
        exposed_type="concrete",
        resolved_type="value",
        expression="container.template construct<value_type>().marker()",
        tiny_static=True,
        limits=COMMON_TINY_LIMITS,
    ),
    ProbeShape(
        name="shared_reference",
        feature="resolve_concrete",
        scope="shared",
        stored_type="value_type",
        exposed_type="concrete",
        resolved_type="value_ref_ptr",
        expression="container.template resolve<value_type&>().marker()",
        tiny_static=True,
        limits=COMMON_TINY_LIMITS,
    ),
    ProbeShape(
        name="optional_value",
        feature="resolve_wrapper",
        scope="shared",
        stored_type="value_optional",
        exposed_type="concrete",
        resolved_type="value_optional_ref",
        expression="container.template resolve<std::optional<value_type>&>()->marker()",
        tiny_static=True,
        limits={
            "static": ProbeLimit(default=0x60),
            "mixed": ProbeLimit(default=0x10),
            "runtime": ProbeLimit(default=None),
        },
    ),
    ProbeShape(
        name="unique_value",
        feature="resolve_concrete",
        scope="unique",
        stored_type="unique_value_type",
        exposed_type="concrete",
        resolved_type="value",
        expression="container.template construct<value_type>().marker()",
        tiny_static=True,
        limits=COMMON_TINY_LIMITS,
    ),
    ProbeShape(
        name="unique_rvalue",
        feature="lifetime_counts",
        scope="unique",
        stored_type="unique_value_type",
        exposed_type="concrete",
        resolved_type="value_rvalue",
        expression="container.template resolve<value_type&&>().marker()",
        tiny_static=True,
        limits={
            "static": ProbeLimit(default=0x10, gcc=0x50, gcc_arm64=0x60),
            "mixed": ProbeLimit(default=0x10, gcc=0x50, gcc13=0x90, gcc_arm64=0x60),
            "runtime": ProbeLimit(default=None),
        },
    ),
    ProbeShape(
        name="unique_wrapper",
        feature="resolve_concrete",
        scope="unique",
        stored_type="unique_value_type",
        exposed_type="concrete",
        resolved_type="value",
        expression="container.template construct<std::unique_ptr<value_type>>()->marker()",
        tiny_static=True,
        limits={
            "static": ProbeLimit(default=0x10, gcc=0x50, gcc_arm64=0x60),
            "mixed": ProbeLimit(default=0x10),
            "runtime": ProbeLimit(default=None),
        },
    ),
    ProbeShape(
        name="interface_handle",
        feature="resolve_interface",
        scope="shared",
        stored_type="implementation_shared_ptr",
        exposed_type="interface_type",
        resolved_type="interface_ref_ptr_shared_ptr",
        expression="container.template resolve<std::shared_ptr<interface_type>&>()->marker()",
        limits={
            "static": ProbeLimit(default=0x180, clang=0x1A0, clang_arm64=0x1D0),
            "mixed": ProbeLimit(default=None),
            "runtime": ProbeLimit(default=None),
        },
    ),
    ProbeShape(
        name="collection_sum",
        feature="resolve_collection",
        scope="shared",
        stored_type="element_shared_ptr",
        exposed_type="element_collection",
        resolved_type="element_vector",
        expression=(
            "[]<typename Container>(Container& container) { "
            "auto values = container.template resolve<std::vector<std::shared_ptr<element_interface>>>(); "
            "return values[0]->id() + values[1]->id(); }(container)"
        ),
        limits={
            "static": ProbeLimit(default=0x500),
            "mixed": ProbeLimit(default=None),
            "runtime": ProbeLimit(default=None),
        },
    ),
    ProbeShape(
        name="external_value",
        feature="resolve_concrete",
        scope="external",
        stored_type="external_value_type",
        exposed_type="concrete",
        resolved_type="value_ref_ptr",
        expression="container.template resolve<value_type&>().marker()",
        limits={
            "runtime": ProbeLimit(
                default=0x300,
                clang=0x2D0,
                gcc=0x310,
                gcc_arm64=0x390,
            ),
        },
    ),
    ProbeShape(
        name="external_reference",
        feature="resolve_concrete",
        scope="external",
        stored_type="external_value_ref",
        exposed_type="concrete",
        resolved_type="value_ref_ptr",
        expression="container.template resolve<value_type&>().marker()",
        limits={
            "runtime": ProbeLimit(
                default=0x300,
                clang=0x2E0,
                gcc=0x330,
                gcc_arm64=0x330,
            ),
        },
    ),
    ProbeShape(
        name="external_wrapper",
        feature="resolve_concrete",
        scope="external",
        stored_type="external_value_shared_ptr",
        exposed_type="concrete",
        resolved_type="value_ref_ptr",
        expression="container.template resolve<value_type&>().marker()",
        limits={
            "runtime": ProbeLimit(
                default=0x420,
                clang=0x940,
                clang_arm64=0x740,
                gcc15=0x440,
                gcc_arm64=0x500,
            ),
        },
    ),
)


FASTER_THAN_RUNTIME = {
    "value_ref",
    "shared_value",
    "shared_reference",
    "unique_value",
    "unique_rvalue",
    "unique_wrapper",
    "interface_handle",
    "collection_sum",
}
