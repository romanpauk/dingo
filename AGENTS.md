# Quality Gate

Configure the development build with:

```bash
cmake -S . -B build -DDINGO_DEVELOPMENT_MODE=ON
```

The available quality targets and their scope are documented in the
[development guide](docs/development.md#quality-targets). Run one with
`cmake --build build -t <target>`.

Before finishing a non-trivial change, run `check`. For a C++ source or header
edit, run `format` first. Keep quality targets out of the default build.

If `check` fails because of unrelated files, fix the stale formatting or report
the blocker explicitly.
