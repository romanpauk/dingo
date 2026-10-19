#!/usr/bin/env python3
"""Exports the validated spec as canonical JSON.

Usage, from the repository root:

    python3 -m spec.export [--output FILE]

Dataclasses become objects, enums their values, and tuples lists. Keys are
sorted, the indent is two spaces, and the output ends with a newline, so the
output is deterministic. The default output is stdout.
"""

import argparse
import dataclasses
import json
import sys
from enum import Enum
from pathlib import Path

from spec import load_spec


def to_jsonable(value):
    """Converts a spec value to plain JSON data."""
    if isinstance(value, Enum):
        return value.value
    if dataclasses.is_dataclass(value) and not isinstance(value, type):
        return {
            field.name: to_jsonable(getattr(value, field.name))
            for field in dataclasses.fields(value)
        }
    if isinstance(value, (tuple, list)):
        return [to_jsonable(item) for item in value]
    return value


def to_json(spec) -> str:
    return json.dumps(to_jsonable(spec), indent=2, sort_keys=True) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--output", type=Path, help="write to FILE instead of stdout")
    args = parser.parse_args(argv)
    text = to_json(load_spec())
    if args.output is None:
        sys.stdout.write(text)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(text.encode("utf-8"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
