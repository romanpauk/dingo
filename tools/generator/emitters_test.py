"""Tests of the emitters with a fake spec."""

from types import SimpleNamespace

import pytest

import emitters
import framework


def capability(**fields):
    values = dict(
        id="pack_indexing",
        description="Pack indexing.",
        feature_macro="__cpp_pack_indexing",
        minimum=202311,
        builtin=None,
        header=None,
    )
    values.update(fields)
    return SimpleNamespace(**values)


def spec_of(*capabilities):
    return SimpleNamespace(cpp=SimpleNamespace(capabilities=tuple(capabilities)))


CAPABILITIES = (
    capability(),
    capability(
        id="move_only_function",
        description="std::move_only_function.",
        feature_macro="__cpp_lib_move_only_function",
        minimum=202110,
        header="version",
    ),
    capability(
        id="type_pack_element",
        description="The __type_pack_element builtin.",
        feature_macro=None,
        minimum=None,
        builtin="__type_pack_element",
    ),
)


def body():
    (generated,) = emitters.capabilities(spec_of(*CAPABILITIES))
    return generated


def test_capabilities_path_and_ids():
    generated = body()
    assert generated.path == "include/dingo/core/capabilities.gen.h"
    assert generated.spec_ids == ("cpp.capabilities",)


def test_capabilities_layout():
    content = body().content
    assert content.startswith("#pragma once\n")
    assert content.count("#include <version>") == 1
    assert "#if __has_include(<version>)" in content
    assert (
        "// spec: cpp.capabilities.pack_indexing\n// Pack indexing.\n"
        "#if defined(__cpp_pack_indexing) && __cpp_pack_indexing >= 202311L\n"
        "#define DINGO_HAS_PACK_INDEXING 1\n#else\n"
        "#define DINGO_HAS_PACK_INDEXING 0\n#endif\n"
    ) in content
    assert (
        "#if defined(__has_builtin)\n#if __has_builtin(__type_pack_element)\n"
        "#define DINGO_HAS_TYPE_PACK_ELEMENT 1\n"
    ) in content


def test_capabilities_output_is_independent_of_input_order():
    forward = emitters.capabilities(spec_of(*CAPABILITIES))
    backward = emitters.capabilities(spec_of(*reversed(CAPABILITIES)))
    assert forward == backward


@pytest.mark.parametrize(
    "fields",
    [
        dict(id="CamelCase"),
        dict(builtin="__x"),
        dict(feature_macro=None),
        dict(minimum=None),
        dict(feature_macro=None, minimum=None, builtin="__x", header="version"),
    ],
)
def test_capabilities_rejects_inconsistent_entries(fields):
    with pytest.raises(framework.GeneratorError):
        emitters.capabilities(spec_of(capability(**fields)))
