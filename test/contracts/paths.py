"""The cheapest path through a function of a disassembled object file.

The calls a function makes on its cheapest path from its entry to a return
are counted from the output of `objdump -dr --no-show-raw-insn`, demangled.
The path follows the jumps of the function and the direct calls into the
functions the object file defines, whose own cheapest paths count; a call of
a function that never returns ends the path. The cost of a path is its number
of calls, then of its indirect calls. An indirect jump is a tail call through
a pointer, so the functions are compiled with -fno-jump-tables.

The counts are those of the path a resolution takes when everything is
already done, such as a memoized resolution, when that path is the cheapest.
"""

import heapq
import re
from dataclasses import dataclass, field

SECTION_RE = re.compile(r"^Disassembly of section (\S+):$")
SYMBOL_RE = re.compile(r"^([0-9a-fA-F]+) <(.+)>:$")
INSTRUCTION_RE = re.compile(r"^\s*([0-9a-fA-F]+):\s+(\S+)\s*(.*)$")
RELOCATION_RE = re.compile(
    r"^\s*([0-9a-fA-F]+):\s+(R_[A-Z0-9_]+)\s+(.+?)(?:([-+])0x([0-9a-f]+))?$"
)
ADDRESS_RE = re.compile(r"^([0-9a-fA-F]+)\s+<(.+?)(?:\+0x([0-9a-f]+))?>")

PREFIXES = {"notrack", "bnd", "rep", "repz", "repnz", "lock", "data16", "cs", "ds"}
RETURNS = {"ret", "retq", "retl"}
CALLS = {"call", "callq", "bl"}
INDIRECT_CALLS = {"blr"}
JUMPS = {"jmp", "jmpq", "b"}
INDIRECT_JUMPS = {"br"}
CONDITIONAL_ARM = {"cbz", "cbnz", "tbz", "tbnz"}
TRAPS = {"ud2", "hlt", "brk", "int3"}

# Functions outside the object file that never return.
NORETURN = (
    "__cxa_throw",
    "__cxa_rethrow",
    "__cxa_bad_",
    "_Unwind_Resume",
    "__clang_call_terminate",
    "std::terminate",
    "abort",
    "exit",
    "__stack_chk_fail",
    "std::__throw_",
    "__assert_fail",
)

# A Dingo function that throws a diagnostic.
DIAGNOSTIC_RE = re.compile(r"dingo::(?:\w+::)*(?:raise|throw)_\w*")


@dataclass
class Instruction:
    address: int
    mnemonic: str
    operands: str
    relocation: tuple | None = None


@dataclass
class Function:
    name: str
    section: str
    start: int
    instructions: list = field(default_factory=list)

    def index_of(self, address):
        for index, instruction in enumerate(self.instructions):
            if instruction.address == address:
                return index
        return None


@dataclass(frozen=True)
class Cost:
    calls: int = 0
    indirect_calls: int = 0
    dingo_calls: int = 0

    def __add__(self, other):
        return Cost(
            self.calls + other.calls,
            self.indirect_calls + other.indirect_calls,
            self.dingo_calls + other.dingo_calls,
        )

    def key(self):
        return (self.calls, self.indirect_calls, self.dingo_calls)


def parse(disassembly):
    """The functions of a disassembly, by name."""
    functions = {}
    section = None
    current = None
    for line in disassembly.splitlines():
        stripped = line.strip()
        header = SECTION_RE.match(stripped)
        if header is not None:
            section = header.group(1)
            current = None
            continue
        symbol = SYMBOL_RE.match(stripped)
        if symbol is not None:
            current = Function(symbol.group(2), section, int(symbol.group(1), 16))
            functions[current.name] = current
            continue
        if current is None:
            continue
        relocation = RELOCATION_RE.match(line)
        if relocation is not None:
            if current.instructions:
                sign, offset = relocation.group(4), relocation.group(5)
                addend = int(offset, 16) if offset else 0
                current.instructions[-1].relocation = (
                    relocation.group(3),
                    -addend if sign == "-" else addend,
                )
            continue
        instruction = INSTRUCTION_RE.match(line)
        if instruction is None:
            continue
        mnemonic, operands = instruction.group(2), instruction.group(3)
        while mnemonic in PREFIXES and operands:
            mnemonic, _, operands = operands.partition(" ")
            operands = operands.strip()
        current.instructions.append(
            Instruction(int(instruction.group(1), 16), mnemonic.lower(), operands)
        )
    return functions


class Paths:
    """The cheapest paths through the functions of one object file."""

    def __init__(self, disassembly):
        self.functions = parse(disassembly)
        self.by_section = {}
        for function in self.functions.values():
            self.by_section.setdefault(function.section, []).append(function)
        for functions in self.by_section.values():
            functions.sort(key=lambda function: function.start)
        self.memo = {}

    def cheapest(self, name):
        """The cost of the cheapest path of `name` to a return, or None."""
        function = self.functions[name]
        return self._cheapest(function, 0)

    def _function_at(self, section, address):
        for function in reversed(self.by_section.get(section, [])):
            if function.start <= address:
                return function
        return None

    def _target(self, function, instruction):
        """The function and the instruction index a branch goes to, the name
        of a function outside the object file, or None when unknown."""
        if instruction.relocation is not None:
            symbol, addend = instruction.relocation
            # A branch addend is relative to the end of the instruction field.
            if symbol in self.functions:
                callee = self.functions[symbol]
                return callee, 0
            name, _, _ = symbol.partition("+")
            if name in self.by_section:
                address = addend + 4
                callee = self._function_at(name, address)
                if callee is not None:
                    return callee, callee.index_of(address)
            return symbol
        target = ADDRESS_RE.match(instruction.operands)
        if target is None:
            return None
        address = int(target.group(1), 16)
        callee = self._function_at(function.section, address)
        if callee is None:
            return target.group(2)
        return callee, callee.index_of(address)

    def _cheapest(self, function, entry):
        key = (function.name, entry)
        if key in self.memo:
            return self.memo[key]
        # A recursive path is not the cheapest.
        self.memo[key] = None
        best = None
        queue = [((0, 0, 0), entry, Cost())]
        settled = set()
        while queue:
            _, index, cost = heapq.heappop(queue)
            if index in settled:
                continue
            settled.add(index)
            if index is None or index >= len(function.instructions):
                continue
            instruction = function.instructions[index]
            for next_index, step, returns in self._steps(function, index, instruction):
                total = cost + step
                if returns:
                    if best is None or total.key() < best.key():
                        best = total
                elif next_index is not None and next_index not in settled:
                    heapq.heappush(queue, (total.key(), next_index, total))
        self.memo[key] = best
        return best

    def _callee_cost(self, function, instruction, indirect):
        """The cost of the call `instruction` makes, or None when the callee
        never returns."""
        if indirect:
            return Cost(1, 1, 0)
        target = self._target(function, instruction)
        if isinstance(target, tuple):
            callee, entry = target
            name = callee.name
            inner = self._cheapest(callee, entry or 0)
            if inner is None:
                return None
            return Cost(1, 0, 1 if is_dingo(name) else 0) + inner
        name = target or ""
        if any(stop in name for stop in NORETURN) or DIAGNOSTIC_RE.search(name):
            return None
        return Cost(1, 0, 1 if is_dingo(name) else 0)

    def _steps(self, function, index, instruction):
        """The successors of an instruction: (index, cost, returns)."""
        mnemonic = instruction.mnemonic
        following = index + 1
        if mnemonic in RETURNS:
            return [(None, Cost(), True)]
        if mnemonic in TRAPS:
            return []
        if mnemonic in CALLS or mnemonic in INDIRECT_CALLS:
            indirect = mnemonic in INDIRECT_CALLS or "*" in instruction.operands
            cost = self._callee_cost(function, instruction, indirect)
            return [] if cost is None else [(following, cost, False)]
        conditional = (
            mnemonic.startswith("j") and mnemonic not in JUMPS
        ) or mnemonic.startswith("b.") or mnemonic in CONDITIONAL_ARM
        if mnemonic in JUMPS or mnemonic in INDIRECT_JUMPS or conditional:
            if mnemonic in INDIRECT_JUMPS or "*" in instruction.operands:
                # A tail call through a pointer.
                return [(None, Cost(1, 1, 0), True)]
            steps = [(following, Cost(), False)] if conditional else []
            target = self._target(function, instruction)
            if isinstance(target, tuple) and target[0] is function:
                steps.append((target[1], Cost(), False))
            elif isinstance(target, tuple):
                # A tail call, or a jump into the cold part of the function.
                callee, entry = target
                inner = self._cheapest(callee, entry or 0)
                if inner is not None:
                    steps.append((None, inner, True))
            elif target is not None:
                cost = self._callee_cost(function, instruction, False)
                if cost is not None:
                    steps.append((None, cost, True))
            return steps
        return [(following, Cost(), False)]


def is_dingo(name):
    return "dingo::" in name and not DIAGNOSTIC_RE.search(name)
