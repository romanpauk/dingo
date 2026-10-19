#!/usr/bin/env python3
"""Generates the committed generated files from the spec.

generate  writes the files whose content changed, deletes the files that are
          no longer generated, and writes spec/generated.manifest
check     reports every difference between the tree and the generated output

Run it through 'cmake --build <build> -t generate' or '-t generate-check' so
that the pinned clang-format and Jinja versions are used.
"""

import argparse
from pathlib import Path
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))

import emitters  # noqa: E402
import framework  # noqa: E402

REPOSITORY = Path(__file__).resolve().parents[2]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=REPOSITORY)
    parser.add_argument("--work-dir", type=Path, help="check: write the output set here")
    parser.add_argument("command", choices=("generate", "check"))
    arguments = parser.parse_args(argv)
    root = arguments.root.resolve()
    try:
        outputs = framework.collect(
            framework.load_spec(root), emitters.EMITTERS, root
        )
        if arguments.command == "generate":
            problems, written, deleted = framework.generate(root, outputs)
            for path in written:
                print(f"wrote {path}")
            for path in deleted:
                print(f"deleted {path}")
            if not problems:
                print(
                    f"generate: {len(outputs)} files, {len(written)} written, "
                    f"{len(deleted)} deleted"
                )
        else:
            problems = framework.check(root, outputs, arguments.work_dir)
            if not problems:
                print(f"generate-check: {len(outputs)} files are up to date")
    except framework.GeneratorError as error:
        print(f"error: {error}")
        return 1
    for problem in problems:
        print(problem)
    if problems:
        if arguments.command == "check":
            print(
                f"{len(problems)} problem(s); regenerate with "
                f"'{framework.FIX_COMMAND}'"
            )
        else:
            print(f"{len(problems)} problem(s); nothing was changed")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
