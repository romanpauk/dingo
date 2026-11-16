#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import importlib.util
import pathlib
import tempfile
import unittest


MODULE_PATH = pathlib.Path(__file__).with_name("names.py")
SPEC = importlib.util.spec_from_file_location("names", MODULE_PATH)
names = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(names)


class NamesTest(unittest.TestCase):
    def test_words_split_at_underscores_and_capitals(self) -> None:
        self.assertEqual(names.words("closed_answer_table"),
                         ["closed", "answer", "table"])
        self.assertEqual(names.words("SourceOf"), ["source", "of"])
        self.assertEqual(names.words("_shape_wrappers"), ["shape", "wrappers"])

    def test_a_connecting_word_fails_a_name(self) -> None:
        self.assertTrue(names.connecting("element_of_t"))
        self.assertTrue(names.connecting("with_quals_t"))
        self.assertTrue(names.connecting("of"))
        self.assertFalse(names.connecting("held_element_t"))
        # A word that only contains one is no connecting word.
        self.assertFalse(names.connecting("offset"))
        self.assertFalse(names.connecting("origin"))

    def test_kept_names_pass(self) -> None:
        self.assertFalse(names.connecting("for_each"))

    def test_headers_fail_on_their_names_not_on_std_or_comments(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "a.h").write_text(
                "// a comment of names_of_things\n"
                "template <typename T> using index_of_t = std::enable_if_t<T>;\n"
                "inline int count(int of) { return of; }\n")
            failed = {name for _, _, name in names.cpp_names(root)}
        self.assertEqual(failed, {"index_of_t", "of"})

    def test_python_fails_on_bound_names_not_on_tests(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "a.py").write_text(
                "def kind_of(name, at):\n"
                "    from_stored = name.split()\n"
                "    return from_stored, at\n"
                "def test_a_request_resolves_from_a_source():\n"
                "    pass\n")
            failed = {name for _, _, name in names.python_names(root)}
        self.assertEqual(failed, {"kind_of", "at", "from_stored"})


if __name__ == "__main__":
    unittest.main()
