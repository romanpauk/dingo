# Quality Gate

Before finishing a non-trivial change, run the repository quality gate:

```bash
cmake --build build -t check
```

This target verifies C++ formatting and Markdown formatting. On non-Windows
builds it also runs clang-tidy through `check-tidy`; Windows skips clang-tidy.
Do not wire it into the default build; it is an explicit validation target for
agents, local review, and CI.

Run clang-tidy directly when you want only static-analysis coverage:

```bash
cmake --build build -t check-tidy
```

This target requires `DINGO_EXAMPLES_ENABLED=ON` so clang-tidy can analyze real
example translation units for this header-only library.

C++ formatting is pinned to `clang-format` 21 from the locked `uv`
development environment. If the local system formatter differs, configure CMake
with the locked executable:

```bash
cmake -S . -B build -DDINGO_DEVELOPMENT_MODE=ON \
  -DDINGO_CLANG_FORMAT_EXE="$(uv run --locked python -c 'import shutil; print(shutil.which("clang-format"))')"
```

For any C++ source or header edit, format before verification:

```bash
cmake --build build -t format
cmake --build build -t check-format
```

If the `build` directory has not been configured, configure a development build
first:

```bash
cmake -S . -B build -DDINGO_DEVELOPMENT_MODE=ON
```

If the quality gate fails because of files unrelated to the current change, do
not ignore the failure. Either fix the stale formatting as part of the change or
call out the blocker explicitly in the final update.
