"""What an object file holds, read without a tool of a toolchain: the size of
its sections by kind, and its functions with their sizes. ELF only, of either
class and byte order; another format gives nothing."""

import struct
from pathlib import Path

# The kinds of sections an object is measured by: its code, its constant
# data, its data, and what unwinds and describes it.
KINDS = {
    ".text": "code",
    ".rodata": "constants",
    ".data": "data",
    ".bss": "data",
    ".eh_frame": "unwind",
    ".gcc_except_table": "unwind",
    ".init_array": "data",
}

SHT_SYMTAB = 2
SHT_NOBITS = 8
STT_FUNC = 2


def kind_of(name):
    """The kind of a section by its name: a section of one function or one
    object, as -ffunction-sections or a template makes, is of the kind of its
    prefix."""
    for prefix, kind in KINDS.items():
        if name == prefix or name.startswith(prefix + "."):
            return kind
    return None


def read(path):
    """The sections of an object by kind, in bytes, and its defined functions
    by name with their sizes; None when it is not an ELF object."""
    data = Path(path).read_bytes()
    if data[:4] != b"\x7fELF":
        return None
    wide = data[4] == 2
    order = "<" if data[5] == 1 else ">"
    if wide:
        shoff, = struct.unpack_from(order + "Q", data, 0x28)
        shentsize, shnum, shstrndx = struct.unpack_from(order + "HHH", data,
                                                        0x3A)
    else:
        shoff, = struct.unpack_from(order + "I", data, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from(order + "HHH", data,
                                                        0x2E)

    def section(index):
        base = shoff + index * shentsize
        if wide:
            name, kind, _, _, offset, size, link, _, _, entsize = (
                struct.unpack_from(order + "IIQQQQIIQQ", data, base))
        else:
            name, kind, _, _, offset, size, link, _, _, entsize = (
                struct.unpack_from(order + "IIIIIIIIII", data, base))
        return name, kind, offset, size, link, entsize

    def text_at(table_offset, at):
        end = data.index(b"\0", table_offset + at)
        return data[table_offset + at:end].decode(errors="replace")

    names_offset = section(shstrndx)[2]
    sizes = {}
    functions = {}
    for index in range(shnum):
        name_at, kind, offset, size, link, entsize = section(index)
        name = text_at(names_offset, name_at)
        measured = kind_of(name)
        if measured is not None:
            sizes[measured] = sizes.get(measured, 0) + size
        if kind == SHT_SYMTAB and entsize:
            strings_offset = section(link)[2]
            for at in range(offset, offset + size, entsize):
                if wide:
                    symbol_name, info, _, shndx, _, symbol_size = (
                        struct.unpack_from(order + "IBBHQQ", data, at))
                else:
                    symbol_name, _, symbol_size, info, _, shndx = (
                        struct.unpack_from(order + "IIIBBH", data, at))
                if info & 0xF == STT_FUNC and shndx != 0 and symbol_size:
                    functions[text_at(strings_offset, symbol_name)] = (
                        symbol_size)
    return {"sections": sizes, "functions": functions}
