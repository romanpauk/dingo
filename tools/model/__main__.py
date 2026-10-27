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
    "paths": "tools.model.report.paths",
    "baseline": "tools.model.report.baseline",
    "storage": "tools.model.emit.families.storage",
    "selection": "tools.model.emit.selection",
    "route": "tools.model.emit.families.route",
    "behavior": "tools.model.emit.families.behavior",
    "interface": "tools.model.emit.families.interface",
    "interface-runtime": "tools.model.emit.families.interface_runtime",
    "matrix": "tools.model.emit.matrix",
    "binding-matrix": "tools.model.emit.binding_matrix",
    "type-behavior": "tools.model.emit.behavior",
    "property": "tools.model.emit.families.property",
    "annotated": "tools.model.emit.families.annotated",
    "const": "tools.model.emit.families.const",
    "keyed": "tools.model.emit.families.keyed",
}


def main(argv: list[str] | None = None) -> int:
    arguments = list(sys.argv[1:] if argv is None else argv)
    if not arguments or arguments[0] in {"-h", "--help"}:
        names = ", ".join(sorted(COMMANDS))
        print(f"usage: python -m tools.model <command> [options]\ncommands: {names}")
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
