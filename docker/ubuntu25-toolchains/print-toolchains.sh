#!/usr/bin/env bash
set -euo pipefail

print_group() {
    local title="$1"
    shift

    echo "${title}"
    for binary in "$@"; do
        if command -v "${binary}" >/dev/null 2>&1; then
            printf '  %-18s %s\n' "${binary}" "$("${binary}" --version | head -n 1)"
        fi
    done
    echo
}

print_group "GCC toolchains" \
    gcc g++ gcc-11 g++-11 gcc-12 g++-12 gcc-13 g++-13 gcc-14 g++-14 gcc-15 g++-15

print_group "Clang toolchains" \
    clang clang++ clang-14 clang++-14 clang-15 clang++-15 clang-17 clang++-17 \
    clang-18 clang++-18 clang-19 clang++-19 clang-20 clang++-20

print_group "Build tools" \
    cmake ninja ccache gdb lldb clang-format clang-tidy clangd python3 mdformat
