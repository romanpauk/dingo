"""Tests of the public API section: its validation, its content, and the check generated from it."""

import importlib.util
import re
import stat
import sys
from dataclasses import replace
from pathlib import Path

import pytest

from spec import build_spec, validate
from spec.schema import ApiKind, CppApiBinding, PublicApiEntry
from spec.tests.fixtures import sample_spec

PROJECT_ROOT = Path(__file__).resolve().parents[2]
CHECKER = PROJECT_ROOT / "test" / "spec" / "public_api_check.py"


def errors_of(spec):
    return validate(spec)


def assert_error(spec, fragment):
    errors = errors_of(spec)
    assert any(fragment in error for error in errors), errors


def with_bindings(spec, bindings):
    return replace(spec, cpp=replace(spec.cpp, api_bindings=tuple(bindings)))


def with_entries(spec, entries):
    return replace(spec, public_api=tuple(entries))


def binding_of(spec, api):
    return next(b for b in spec.cpp.api_bindings if b.api == api)


def replace_binding(spec, api, **changes):
    bindings = [
        replace(b, **changes) if b.api == api else b for b in spec.cpp.api_bindings
    ]
    return with_bindings(spec, bindings)


def test_sample_public_api_is_valid():
    assert errors_of(sample_spec()) == []


def test_entry_without_binding_is_reported():
    spec = sample_spec()
    extra = PublicApiEntry(id="extra", kind=ApiKind.NAME, description="Extra.")
    spec = with_entries(spec, (*spec.public_api, extra))
    assert_error(spec, "public_api.extra: has no C++ binding")


def test_binding_without_entry_is_reported():
    spec = sample_spec()
    orphan = CppApiBinding(id="orphan", api="missing", spelling="x")
    spec = with_bindings(spec, (*spec.cpp.api_bindings, orphan))
    assert_error(spec, "public API entry 'missing' does not exist")


def test_entry_with_two_bindings_is_reported():
    spec = sample_spec()
    twin = replace(binding_of(spec, "shared_ptr_shape"), id="twin")
    spec = with_bindings(spec, (*spec.cpp.api_bindings, twin))
    assert_error(spec, "public API entries has duplicates")


def test_entry_description_is_required():
    spec = sample_spec()
    entry = replace(spec.public_api[0], description=" ")
    spec = with_entries(spec, (entry, *spec.public_api[1:]))
    assert_error(spec, "public_api.container_header: description is empty")


@pytest.mark.parametrize(
    "changes, fragment",
    [
        ({"usage": None}, "needs usage"),
        ({"header": None}, "needs a header"),
        ({"header": "dingo/unknown.h"}, "is not a public include path"),
        ({"usage": "int x;"}, "must use {name}"),
        ({"usage": "  "}, "usage is empty"),
        ({"declarations": "  "}, "declarations is empty"),
    ],
)
def test_name_binding_rules(changes, fragment):
    spec = replace_binding(sample_spec(), "shared_ptr_shape", **changes)
    assert_error(spec, fragment)


def test_declarations_may_use_the_name():
    spec = replace_binding(
        sample_spec(),
        "shared_ptr_shape",
        usage="int x;",
        declarations="using alias = {name}<int>;",
    )
    assert errors_of(spec) == []


@pytest.mark.parametrize("field", ["header", "declarations", "usage"])
def test_include_path_carries_no_code(field):
    spec = replace_binding(sample_spec(), "container_header", **{field: "dingo/container.h"})
    assert_error(spec, f"only a name or extension point has {field}")


def test_include_path_must_be_well_formed():
    spec = replace_binding(sample_spec(), "container_header", spelling="<dingo/container.h>")
    assert_error(spec, "include path '<dingo/container.h>' is malformed")


def test_configuration_macro_must_be_well_formed():
    spec = sample_spec()
    entry = PublicApiEntry(id="macro", kind=ApiKind.CONFIGURATION_MACRO, description="A macro.")
    binding = CppApiBinding(id="macro_cpp", api="macro", spelling="NOT A MACRO")
    spec = with_entries(spec, (*spec.public_api, entry))
    spec = with_bindings(spec, (*spec.cpp.api_bindings, binding))
    assert_error(spec, "macro 'NOT A MACRO' is malformed")


def test_extension_point_follows_the_name_rules():
    spec = sample_spec()
    entry = PublicApiEntry(id="traits", kind=ApiKind.EXTENSION_POINT, description="Traits.")
    binding = CppApiBinding(id="traits_cpp", api="traits", spelling="dingo::type_traits")
    spec = with_entries(spec, (*spec.public_api, entry))
    spec = with_bindings(spec, (*spec.cpp.api_bindings, binding))
    errors = errors_of(spec)
    assert any("needs usage" in e for e in errors)
    assert any("needs a header" in e for e in errors)


# Shipped content


def shipped():
    return build_spec()


def test_shipped_public_api_is_valid():
    assert errors_of(shipped()) == []


def test_shipped_public_api_has_every_kind():
    kinds = {entry.kind for entry in shipped().public_api}
    assert kinds == set(ApiKind)


def test_shipped_bindings_match_entries_one_to_one():
    spec = shipped()
    assert sorted(b.api for b in spec.cpp.api_bindings) == sorted(e.id for e in spec.public_api)


def test_shipped_descriptions_contain_no_cpp_text():
    for entry in shipped().public_api:
        assert not re.search(r"::|<|>|`|\(\)", entry.description), entry.id
        assert entry.description.endswith("."), entry.id


def test_shipped_include_paths_exist():
    spec = shipped()
    kinds = {e.id: e.kind for e in spec.public_api}
    for binding in spec.cpp.api_bindings:
        if kinds[binding.api] is ApiKind.INCLUDE_PATH:
            assert (PROJECT_ROOT / "include" / binding.spelling).is_file(), binding.spelling


def test_shipped_public_api_covers_the_documented_headers():
    spec = shipped()
    kinds = {e.id: e.kind for e in spec.public_api}
    paths = {b.spelling for b in spec.cpp.api_bindings if kinds[b.api] is ApiKind.INCLUDE_PATH}
    required = {
        "dingo/container.h",
        "dingo/runtime_container.h",
        "dingo/static_container.h",
        "dingo/storage/shared.h",
        "dingo/storage/unique.h",
        "dingo/storage/external.h",
        "dingo/storage/shared_cyclical.h",
        "dingo/factory/constructor.h",
        "dingo/factory/function.h",
        "dingo/factory/callable.h",
    }
    assert required <= paths


def test_shipped_macros_are_the_user_definable_ones_of_config_h():
    spec = shipped()
    kinds = {e.id: e.kind for e in spec.public_api}
    macros = {
        b.spelling
        for b in spec.cpp.api_bindings
        if kinds[b.api] is ApiKind.CONFIGURATION_MACRO
    }
    config = (PROJECT_ROOT / "include" / "dingo" / "base" / "config.h").read_text()
    assert macros == set(re.findall(r"#if !defined\((DINGO_\w+)\)", config))


def test_shipped_names_are_exercised_by_their_code():
    spec = shipped()
    kinds = {e.id: e.kind for e in spec.public_api}
    for binding in spec.cpp.api_bindings:
        if kinds[binding.api] in (ApiKind.NAME, ApiKind.EXTENSION_POINT):
            code = f"{binding.declarations or ''}{binding.usage}"
            assert "{name}" in code, binding.api


# The generated check


@pytest.fixture(scope="module")
def checker():
    module_spec = importlib.util.spec_from_file_location("public_api_check", CHECKER)
    module = importlib.util.module_from_spec(module_spec)
    sys.modules[module_spec.name] = module
    module_spec.loader.exec_module(module)
    return module


def test_name_code_substitutes_the_spelling(checker):
    binding = CppApiBinding(
        id="x_cpp",
        api="x",
        spelling="dingo::thing",
        header="dingo/container.h",
        declarations="using alias = {name}<int>;",
        usage="{name}<int> value;",
    )
    code = checker.name_code(binding)
    assert "using alias = dingo::thing<int>;" in code
    assert "inline void api_check_x() {\ndingo::thing<int> value;\n}" in code
    assert "{name}" not in code


def test_units_follow_the_kinds(checker):
    includes, code, macros = checker.collect_units(sample_spec())
    assert [unit.api for unit in includes] == ["container_header"]
    assert includes[0].code == "#include <dingo/container.h>\n"
    assert [unit.api for unit in code] == ["shared_ptr_shape"]
    assert macros == []


def test_macro_unit_names_the_entry(checker):
    code = checker.macro_code("config_x", "DINGO_X")
    assert "#if !defined(DINGO_X)" in code
    assert "public API entry config_x" in code


def test_names_prelude_includes_the_declaring_headers_once(checker):
    assert checker.declaring_headers(sample_spec()) == ["dingo/container.h"]
    prelude = checker.names_prelude(["dingo/container.h"])
    assert prelude.count("#include <dingo/container.h>") == 1
    assert "#include <memory>" in prelude


def write_fake_compiler(path):
    path.write_text(
        f"#!{sys.executable}\n"
        "import sys\n"
        "text = open(sys.argv[-1]).read()\n"
        "if 'BAD' in text:\n"
        "    print(sys.argv[-1] + ':1:1: error: BAD is not declared', file=sys.stderr)\n"
        "    sys.exit(1)\n"
    )
    path.chmod(path.stat().st_mode | stat.S_IEXEC)
    return [str(path)]


def test_check_passes_with_a_compiler_that_accepts(checker, tmp_path):
    command = write_fake_compiler(tmp_path / "cxx")
    checked, failures = checker.check(sample_spec(), command, tmp_path / "work", 2)
    assert (checked, failures) == (2, [])
    assert (tmp_path / "work" / "names.cpp").is_file()
    assert (tmp_path / "work" / "include-container_header.cpp").is_file()


def test_check_reports_the_failing_entry(checker, tmp_path):
    command = write_fake_compiler(tmp_path / "cxx")
    spec = replace_binding(sample_spec(), "shared_ptr_shape", usage="{name}<BAD> pointer;")
    checked, failures = checker.check(spec, command, tmp_path / "work", 2)
    assert checked == 2
    assert [f.api for f in failures] == ["shared_ptr_shape"]
    assert failures[0].spelling == "std::shared_ptr"
    assert "BAD is not declared" in failures[0].output


def test_check_reports_a_failing_include_path(checker, tmp_path):
    command = write_fake_compiler(tmp_path / "cxx")
    spec = replace_binding(sample_spec(), "container_header", spelling="dingo/BAD.h")
    _, failures = checker.check(spec, command, tmp_path / "work", 2)
    assert [f.api for f in failures] == ["container_header"]
