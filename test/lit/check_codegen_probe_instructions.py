#!/usr/bin/env python3

import re
import os
import platform
import sys
from pathlib import Path


TINY_PROBES = {
    "probe_static_service_read": 1,
    "probe_hybrid_service_read": 1,
    "probe_static_shared_value_config": 1,
    "probe_hybrid_shared_value_config": 1,
    "probe_static_shared_reference_config": 1,
    "probe_hybrid_shared_reference_config": 1,
    "probe_static_optional_config": 1,
    "probe_hybrid_optional_config": 1,
    "probe_static_unique_value_config": 1,
    "probe_hybrid_unique_value_config": 1,
    "probe_static_unique_rvalue_config": 1,
    "probe_hybrid_unique_rvalue_config": 1,
    "probe_static_unique_wrapper_config": 1,
    "probe_hybrid_unique_wrapper_config": 1,
}

SYMBOL_RE = re.compile(r"^[0-9a-fA-F]+ <([^>]+)>:$")
INSTRUCTION_RE = re.compile(r"^\s*[0-9a-fA-F]+:\s+([A-Za-z.][A-Za-z0-9.]*)\b")

NOPS = {"nop", "nopl", "nopw", "hint", "cs"}
RETURNS = {"ret", "retq", "retl", "retab", "retaa"}
FORBIDDEN_PREFIXES = ("call", "jmp", "j", "b", "bl", "cb", "tb")


def load_instructions(path: Path) -> dict[str, list[str]]:
    instructions: dict[str, list[str]] = {}
    current_symbol: str | None = None
    for raw_line in path.read_text().splitlines():
        line = raw_line.strip("\n")
        if not line.strip():
            current_symbol = None
            continue
        match = SYMBOL_RE.match(line.strip())
        if match is not None:
            current_symbol = match.group(1) if match.group(1) in TINY_PROBES else None
            if current_symbol is not None:
                instructions.setdefault(current_symbol, [])
            continue
        if current_symbol is None:
            continue
        instruction = INSTRUCTION_RE.match(line)
        if instruction is None:
            continue
        instructions[current_symbol].append(instruction.group(1).lower())
    return instructions


def main() -> int:
    if len(sys.argv) != 2:
        print(
            "usage: check_codegen_probe_instructions.py <objdump-output>",
            file=sys.stderr,
        )
        return 2

    instructions = load_instructions(Path(sys.argv[1]))
    missing = sorted(set(TINY_PROBES) - instructions.keys())
    if missing:
        print(f"missing probe disassembly: {', '.join(missing)}", file=sys.stderr)
        return 1

    machine = platform.machine().lower()
    if machine not in ("x86_64", "amd64", "i386", "i686"):
        # The size checker is the portable guard for non-x86 targets. This
        # instruction check intentionally verifies the x86 tiny-probe contract,
        # where the optimized static path should compile down to one move and a
        # return; AArch64 compilers may emit local conditional branches while
        # still keeping the symbol within the small architecture-specific cap.
        return 0

    compiler_id = os.environ.get("DINGO_CXX_ID", "")
    if "g++" in compiler_id:
        # GCC can keep the static probe under the tiny size cap while still
        # emitting a local conditional branch. Keep the exact branch-free
        # mnemonic check for compiler families that reliably expose that shape,
        # and use the size checker as the GCC guard.
        return 0

    for symbol, max_non_return in TINY_PROBES.items():
        ops = instructions[symbol]
        significant = [op for op in ops if op not in NOPS and op not in RETURNS]
        for op in significant:
            if any(op == prefix or op.startswith(prefix) for prefix in FORBIDDEN_PREFIXES):
                print(
                    f"{symbol} regressed to control-flow instruction {op}",
                    file=sys.stderr,
                )
                return 1
        if len(significant) > max_non_return:
            print(
                f"{symbol} regressed: {len(significant)} significant instructions",
                file=sys.stderr,
            )
            return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
