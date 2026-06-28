# Quality Gate

Before finishing a non-trivial change, run the repository quality gate:

```bash
cmake --build build -t check
```

This target verifies C++ formatting and Markdown formatting. Do not wire it into
the default build; it is an explicit validation target for agents, local review,
and CI.

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
