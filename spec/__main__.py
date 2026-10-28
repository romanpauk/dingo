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
    "storage": "spec.emit.families.storage",
    "selection": "spec.emit.selection",
    "route": "spec.emit.families.route",
    "behavior": "spec.emit.families.behavior",
    "interface": "spec.emit.families.interface",
    "interface-runtime": "spec.emit.families.interface_runtime",
    "matrix": "spec.emit.matrix",
    "trait-matrix": "spec.emit.traits",
    "resolve": "spec.emit.resolve",
    "detection-constructor": "spec.emit.detection_constructor",
    "binding-matrix": "spec.emit.binding_matrix",
    "type-behavior": "spec.emit.behavior",
    "property": "spec.emit.families.property",
    "annotated": "spec.emit.families.annotated",
    "const": "spec.emit.families.const",
    "keyed": "spec.emit.families.keyed",
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
