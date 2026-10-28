#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Container configuration classes.

A configuration swaps a container trait under the same rows. Its only degrees
of freedom are the trait and the row it is proved on, so each configuration is
one file with one case:

    resolution/runtime/<configuration>/global/resolve.cpp

The indexed lookup family uses the same level for its lookup definition.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from spec.emit.common.runner import emit, render_template
from spec.emit.hierarchy import CasePath


@dataclass(frozen=True)
class ConfigurationShard:
    configuration: str
    path: str
    case_path: str
    suite: str
    namespace: str


def _shard(configuration: str) -> ConfigurationShard:
    case = CasePath(
        phase="resolution",
        container="runtime",
        configuration=configuration,
        scope="global",
        category="resolve",
    )
    return ConfigurationShard(
        configuration=configuration,
        path=case.file,
        case_path=case.path,
        suite=case.suite,
        namespace=case.suite,
    )


SHARDS = (_shard("allocator"), _shard("rtti"))


def render(shard: ConfigurationShard) -> str:
    return render_template("configuration_cases.cpp.j2", shard=shard)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    for shard in SHARDS:
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)


if __name__ == "__main__":
    main()
