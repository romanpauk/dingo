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
    container_requires: frozenset[str] = frozenset()


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
    provides: frozenset[str] = frozenset()


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
        name="construct_invoke",
        function="run_construct_invoke",
        include="matrix/features/construct_invoke.h",
        requires=frozenset({"concrete_binding", "shared_storage"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_concrete",
        function="run_resolve_concrete",
        include="matrix/features/resolve_concrete.h",
        requires=frozenset({"concrete_binding", "shared_storage"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
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
    Feature(
        name="resolve_keyed",
        function="run_resolve_keyed",
        include="matrix/features/resolve_keyed.h",
        requires=frozenset({"keyed_binding", "shared_storage"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_unique_wrapper",
        function="run_resolve_unique_wrapper",
        include="matrix/features/resolve_unique_wrapper.h",
        requires=frozenset({"interface_binding", "unique_storage"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_indexed",
        function="run_resolve_indexed",
        include="matrix/features/resolve_indexed.h",
        requires=frozenset({"indexed_binding", "shared_storage"}),
        modes=frozenset({"runtime"}),
        container_requires=frozenset({"indexed_container"}),
    ),
)


BINDING_RECIPES = (
    BindingRecipe(
        name="shared_concrete",
        provides=frozenset({"concrete_binding", "shared_storage"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        static_bindings=(
            "dingo::bindings<"
            "dingo::bind<dingo::scope<dingo::shared>, "
            "dingo::storage<config>>>"
        ),
        runtime_setup=(
            "container.template register_type<dingo::scope<dingo::shared>, "
            "dingo::storage<config>>();",
        ),
        mixed_static_bindings=(
            "dingo::bindings<"
            "dingo::bind<dingo::scope<dingo::shared>, "
            "dingo::storage<config>>>"
        ),
        mixed_runtime_setup=(),
    ),
    BindingRecipe(
        name="shared_interface",
        provides=frozenset(
            {"concrete_binding", "interface_binding", "shared_storage"}
        ),
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
        name="unique_interface",
        provides=frozenset({"interface_binding", "unique_storage"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        static_bindings=(
            "dingo::bindings<"
            "dingo::bind<dingo::scope<dingo::unique>, "
            "dingo::storage<std::unique_ptr<unique_service>>, "
            "dingo::interfaces<unique_interface>>>"
        ),
        runtime_setup=(
            "container.template register_type<dingo::scope<dingo::unique>, "
            "dingo::storage<std::unique_ptr<unique_service>>, "
            "dingo::interfaces<unique_interface>>();",
        ),
        mixed_static_bindings=(
            "dingo::bindings<"
            "dingo::bind<dingo::scope<dingo::unique>, "
            "dingo::storage<std::unique_ptr<unique_service>>, "
            "dingo::interfaces<unique_interface>>>"
        ),
        mixed_runtime_setup=(),
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
    BindingRecipe(
        name="keyed_shared_interface",
        provides=frozenset({"keyed_binding", "shared_storage"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        static_bindings=(
            "dingo::bindings<"
            "dingo::bind<dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<processor<0>>>, "
            "dingo::interfaces<processor_interface>, "
            "dingo::key<first_key>>, "
            "dingo::bind<dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<processor<1>>>, "
            "dingo::interfaces<processor_interface>, "
            "dingo::key<second_key>>>"
        ),
        runtime_setup=(
            "container.template register_type<dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<processor<0>>>, "
            "dingo::interfaces<processor_interface>, "
            "dingo::key<first_key>>();",
            "container.template register_type<dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<processor<1>>>, "
            "dingo::interfaces<processor_interface>, "
            "dingo::key<second_key>>();",
        ),
        mixed_static_bindings=(
            "dingo::bindings<"
            "dingo::bind<dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<processor<0>>>, "
            "dingo::interfaces<processor_interface>, "
            "dingo::key<first_key>>>"
        ),
        mixed_runtime_setup=(
            "container.template register_type<dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<processor<1>>>, "
            "dingo::interfaces<processor_interface>, "
            "dingo::key<second_key>>();",
        ),
    ),
    BindingRecipe(
        name="indexed_shared_interface",
        provides=frozenset({"indexed_binding", "shared_storage"}),
        modes=frozenset({"runtime"}),
        static_bindings=None,
        runtime_setup=(
            "container.template register_indexed_type<"
            "dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<processor<0>>>, "
            "dingo::interfaces<processor_interface>>(0);",
            "container.template register_indexed_type<"
            "dingo::scope<dingo::shared>, "
            "dingo::storage<std::shared_ptr<processor<1>>>, "
            "dingo::interfaces<processor_interface>>(1);",
        ),
        mixed_static_bindings=None,
        mixed_runtime_setup=(),
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
    ContainerSpec(
        name="runtime_container_indexed",
        modes=frozenset({"runtime"}),
        container_type="dingo::runtime_container<indexed_container_traits>",
        provides=frozenset({"indexed_container"}),
    ),
    ContainerSpec(
        name="container_indexed",
        modes=frozenset({"runtime"}),
        container_type="dingo::container<indexed_container_traits>",
        provides=frozenset({"indexed_container"}),
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


def can_generate(
    feature: Feature, recipe: BindingRecipe, container: ContainerSpec
) -> bool:
    return (
        feature.requires <= recipe.provides
        and feature.container_requires <= container.provides
    )


def generate_feature(feature: Feature, out_dir: Path, env: Environment) -> Path:
    cases: list[GeneratedCase] = []
    tests: list[GeneratedTest] = []

    for recipe in BINDING_RECIPES:
        for container in CONTAINERS:
            if not can_generate(feature, recipe, container):
                continue
            modes = valid_modes(feature, recipe, container)
            if not modes:
                continue
            for mode in sorted(modes):
                if mode == "static" and recipe.static_bindings is None:
                    continue
                if mode == "mixed" and recipe.mixed_static_bindings is None:
                    continue
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

    if not tests:
        raise RuntimeError(f"feature {feature.name} did not generate any tests")

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
