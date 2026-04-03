#!/usr/bin/env bash
set -euo pipefail

print_banner() {
    local title="$1"
    local value="$2"
    printf '%s %s\n\n' "${title}" "${value}"
}

print_version_line() {
    local binary="$1"
    local version_line=""

    case "${binary}" in
        cl)
            version_line="$("${binary}" 2>&1 | head -n 1 || true)"
            ;;
        *)
            version_line="$("${binary}" --version 2>&1 | head -n 1 || true)"
            ;;
    esac

    if [[ -n "${version_line}" ]]; then
        printf '  %-18s %s\n' "${binary}" "${version_line}"
    fi
}

print_group() {
    local title="$1"
    shift

    echo "${title}"
    for binary in "$@"; do
        if command -v "${binary}" >/dev/null 2>&1; then
            print_version_line "${binary}"
        fi
    done
    echo
}

print_banner "Selected toolchain:" "${DINGO_TOOLCHAIN:-unknown}"

print_group "GCC toolchains" \
    gcc g++ gcc-11 g++-11 gcc-12 g++-12 gcc-13 g++-13 gcc-14 g++-14 gcc-15 g++-15

print_group "Clang toolchains" \
    clang clang++ clang-14 clang++-14 clang-15 clang++-15 clang-17 clang++-17 \
    clang-18 clang++-18 clang-19 clang++-19 clang-20 clang++-20

print_group "MSVC toolchains" \
    cl

print_group "Build tools" \
    cmake ninja ccache gdb lldb clang-format clang-tidy clangd python3 mdformat
