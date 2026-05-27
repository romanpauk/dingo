#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from jinja2 import Environment, FileSystemLoader, StrictUndefined


@dataclass(frozen=True)
class Feature:
    name: str
    function: str
    include: str
    requires: frozenset[str]
    modes: frozenset[str]


@dataclass(frozen=True)
class BindingRecipe:
    name: str
    provides: frozenset[str]
    modes: frozenset[str]
    static_bindings: str | None
    runtime_setup: tuple[str, ...]
    mixed_static_bindings: str | None
    mixed_runtime_setup: tuple[str, ...]


@dataclass(frozen=True)
class ContainerSpec:
    name: str
    modes: frozenset[str]
    container_type: str


@dataclass(frozen=True)
class GeneratedCase:
    name: str
    container_type: str
    static_bindings: str | None
    setup_lines: tuple[str, ...]


@dataclass(frozen=True)
class GeneratedTest:
    suite: str
    name: str
    function: str
    case_name: str


FEATURES = (
    Feature(
        name="resolve_interface",
        function="run_resolve_interface",
        include="matrix/features/resolve_interface.h",
        requires=frozenset({"interface_binding", "shared_storage"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_collection",
        function="run_resolve_collection",
        include="matrix/features/resolve_collection.h",
        requires=frozenset({"collection_binding", "shared_storage"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
)


BINDING_RECIPES = (
    BindingRecipe(
        name="shared_interface",
        provides=frozenset({"interface_binding", "shared_storage"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        static_bindings=(
            "dingo::bindings<"
            "dingo::bind<dingo::scope<dingo::shared>, "
            "dingo::storage<config>>, "
            "dingo::bind<dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<service>>, "
            "dingo::interfaces<service_interface>>>"
        ),
        runtime_setup=(
            "container.template register_type<dingo::scope<dingo::shared>, "
            "dingo::storage<config>>();",
            "container.template register_type<dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<service>>, "
            "dingo::interfaces<service_interface>>();",
        ),
        mixed_static_bindings=(
            "dingo::bindings<"
            "dingo::bind<dingo::scope<dingo::shared>, "
            "dingo::storage<config>>>"
        ),
        mixed_runtime_setup=(
            "container.template register_type<dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<service>>, "
            "dingo::interfaces<service_interface>>();",
        ),
    ),
    BindingRecipe(
        name="shared_processor_collection",
        provides=frozenset({"collection_binding", "shared_storage"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        static_bindings=(
            "dingo::bindings<"
            "dingo::bind<dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<processor<0>>>, "
            "dingo::interfaces<processor_interface>>, "
            "dingo::bind<dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<processor<1>>>, "
            "dingo::interfaces<processor_interface>>>"
        ),
        runtime_setup=(
            "container.template register_type_collection<"
            "dingo::scope<dingo::unique>, "
            "dingo::storage<std::vector<std::shared_ptr<"
            "processor_interface>>>>();",
            "container.template register_type<dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<processor<0>>>, "
            "dingo::interfaces<processor_interface>>();",
            "container.template register_type<dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<processor<1>>>, "
            "dingo::interfaces<processor_interface>>();",
        ),
        mixed_static_bindings=(
            "dingo::bindings<"
            "dingo::bind<dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<processor<0>>>, "
            "dingo::interfaces<processor_interface>>>"
        ),
        mixed_runtime_setup=(
            "container.template register_type_collection<"
            "dingo::scope<dingo::unique>, "
            "dingo::storage<std::vector<std::shared_ptr<"
            "processor_interface>>>>();",
            "container.template register_type<dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<processor<1>>>, "
            "dingo::interfaces<processor_interface>>();",
        ),
    ),
)


CONTAINERS = (
    ContainerSpec(
        name="runtime_container",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<>",
    ),
    ContainerSpec(
        name="container_runtime",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<>",
    ),
    ContainerSpec(
        name="static_container",
        modes=frozenset({"static"}),
        container_type="dingo::static_container<static_bindings>",
    ),
    ContainerSpec(
        name="container_static",
        modes=frozenset({"static"}),
        container_type="dingo::container<static_bindings>",
    ),
    ContainerSpec(
        name="container_mixed",
        modes=frozenset({"mixed"}),
        container_type="dingo::container<static_bindings>",
    ),
)


def valid_modes(feature: Feature, recipe: BindingRecipe, container: ContainerSpec) -> set[str]:
    return set(feature.modes & recipe.modes & container.modes)


def make_case(mode: str, recipe: BindingRecipe, container: ContainerSpec) -> GeneratedCase:
    static_bindings = None
    setup_lines = ()
    if mode == "static":
        static_bindings = recipe.static_bindings
    elif mode == "runtime":
        setup_lines = recipe.runtime_setup
    elif mode == "mixed":
        static_bindings = recipe.mixed_static_bindings
        setup_lines = recipe.mixed_runtime_setup
    return GeneratedCase(
        name=f"{recipe.name}_{container.name}",
        container_type=container.container_type,
        static_bindings=static_bindings,
        setup_lines=setup_lines,
    )


def generate_feature(feature: Feature, out_dir: Path, env: Environment) -> Path:
    cases: list[GeneratedCase] = []
    tests: list[GeneratedTest] = []

    for recipe in BINDING_RECIPES:
        if not feature.requires <= recipe.provides:
            continue
        for container in CONTAINERS:
            modes = valid_modes(feature, recipe, container)
            if not modes:
                continue
            for mode in sorted(modes):
                case = make_case(mode, recipe, container)
                cases.append(case)
                tests.append(
                    GeneratedTest(
                        suite=feature.name,
                        name=case.name,
                        function=feature.function,
                        case_name=case.name,
                    )
                )

    source = out_dir / f"{feature.name}.cpp"
    rendered = env.get_template("source.cpp.j2").render(
        includes=[feature.include],
        cases=cases,
        tests=tests,
    )
    source.write_text(rendered, encoding="utf-8")
    return source


def generate(out_dir: Path, cmake_file: Path) -> None:
    script_dir = Path(__file__).resolve().parent
    env = Environment(
        loader=FileSystemLoader(script_dir / "templates"),
        undefined=StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
    )

    out_dir.mkdir(parents=True, exist_ok=True)
    sources = [generate_feature(feature, out_dir, env) for feature in FEATURES]
    cmake_file.parent.mkdir(parents=True, exist_ok=True)
    cmake_file.write_text(
        env.get_template("cmake.cmake.j2").render(
            sources=[source.as_posix() for source in sources]
        ),
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--cmake", required=True, type=Path)
    args = parser.parse_args()

    generate(args.out, args.cmake)


if __name__ == "__main__":
    main()
