#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import importlib.util
import pathlib
import unittest


MODULE_PATH = pathlib.Path(__file__).with_name("complexity.py")
SPEC = importlib.util.spec_from_file_location("complexity", MODULE_PATH)
complexity = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(complexity)


def event(name, detail, ts, dur, tid=0):
    return {
        "ph": "X",
        "name": name,
        "ts": ts,
        "dur": dur,
        "tid": tid,
        "args": {"detail": detail},
    }


class ComplexityTests(unittest.TestCase):
    def test_template_name_drops_arguments_and_scope(self) -> None:
        self.assertEqual(
            complexity.template_name(
                "dingo::detail::binding_activation<cell<a<b>>, c &>::apply<x>"
            ),
            "binding_activation::apply",
        )
        self.assertEqual(
            complexity.template_name("dingo::resolve<int>(int &)"), "resolve"
        )

    def test_instantiation_time_excludes_nested_instantiations(self) -> None:
        times = complexity.instantiation_times(
            [
                event("InstantiateClass", "dingo::outer<int>", 0, 100),
                event("InstantiateClass", "dingo::inner<int>", 10, 30),
                event("InstantiateFunction", "dingo::inner<long>", 50, 20),
                event("InstantiateClass", "std::vector<int>", 80, 10),
                event("Source", "dingo/container.h", 0, 500),
            ]
        )
        self.assertEqual(times["outer"], (40, 1))
        self.assertEqual(times["inner"], (50, 2))
        self.assertNotIn("std::vector", times)

    def test_instantiations_nest_per_thread(self) -> None:
        times = complexity.instantiation_times(
            [
                event("InstantiateClass", "dingo::first<int>", 0, 100, tid=1),
                event("InstantiateClass", "dingo::second<int>", 10, 30, tid=2),
            ]
        )
        self.assertEqual(times["first"], (100, 1))
        self.assertEqual(times["second"], (30, 1))

    def test_header_metrics_count_template_machinery(self) -> None:
        source = complexity.strip_comments(
            """
            // struct commented_out {};
            template <typename T> struct used_twice {};
            template <typename T> struct used_once {};
            template <typename T> struct used_twice<T *> {};
            template <typename T>
            using alias_t = typename used_twice<T>::type;
            template <typename T>
            inline constexpr bool flag_v = std::is_void_v<T>;
            void f() { if constexpr (flag_v<int>) {} }
            """
        )
        other = "used_twice<int> a; used_twice<long> b; alias_t<int> c;"
        rows = complexity.header_metrics({"a.h": source, "b.h": other})
        row = next(row for row in rows if row["header"] == "a.h")
        self.assertEqual(row["templates"], 5)
        self.assertEqual(row["specializations"], 1)
        self.assertEqual(row["if_constexpr"], 1)
        self.assertEqual(row["types"], 4)
        self.assertEqual(row["single_use"], 3)

    def test_profile_command_retargets_compile_command(self) -> None:
        command = complexity.profile_command(
            {
                "directory": "/build",
                "file": "/src/a.cpp",
                "command": "/usr/bin/c++ -I/src/include -std=c++17 -Wall "
                "-Werror -o a.o -c /src/a.cpp",
            },
            "clang++",
            "/tmp/unit.o",
        )
        self.assertEqual(
            command,
            [
                "clang++",
                "-I/src/include",
                "-std=c++17",
                "-c",
                "/src/a.cpp",
                "-o",
                "/tmp/unit.o",
                "-ftime-trace",
                "-ftime-trace-granularity=50",
            ],
        )


if __name__ == "__main__":
    unittest.main()
