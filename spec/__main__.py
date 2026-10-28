#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Unified entrypoint for the model generators and reports.

Each subcommand forwards its arguments to the module that owns the generator.
The CMake build calls the modules directly; this dispatcher is for interactive
use and keeps the file layout out of the command line.
"""

from __future__ import annotations

import importlib
import sys

COMMANDS = {
    "paths": "spec.report.paths",
    "baseline": "spec.report.baseline",
    "resolve-report": "spec.report.resolves",
    "storage": "spec.gen.classification.storage",
    "selection": "spec.gen.classification.selection",
    "route": "spec.gen.resolution.route",
    "behavior": "spec.gen.resolution.behavior",
    "interface": "spec.gen.resolution.interface",
    "interface-runtime": "spec.gen.resolution.interface_runtime",
    "matrix": "spec.gen.resolution.type_matrix",
    "trait-matrix": "spec.gen.resolution.trait_matrix",
    "resolve": "spec.gen.resolution.resolve",
    "detection-constructor": "spec.gen.classification.constructor",
    "binding-matrix": "spec.gen.registration.binding",
    "type-behavior": "spec.gen.resolution.type_behavior",
    "property": "spec.gen.classification.wrapper",
    "annotated": "spec.gen.resolution.annotated",
    "const": "spec.gen.resolution.const",
    "keyed": "spec.gen.resolution.keyed",
}


def main(argv: list[str] | None = None) -> int:
    arguments = list(sys.argv[1:] if argv is None else argv)
    if not arguments or arguments[0] in {"-h", "--help"}:
        names = ", ".join(sorted(COMMANDS))
        print(f"usage: python -m spec <command> [options]\ncommands: {names}")
        return 0
    command = arguments.pop(0)
    module_name = COMMANDS.get(command)
    if module_name is None:
        print(f"unknown command {command!r}; choose from {', '.join(sorted(COMMANDS))}")
        return 2
    module = importlib.import_module(module_name)
    sys.argv = [module_name, *arguments]
    module.main()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
