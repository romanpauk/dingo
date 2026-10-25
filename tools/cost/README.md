# Cost Probes

`measure.py` records and compares the cost of a compilation probe.

The hard gate uses deterministic values:

- text, data, and bss size of the compiled object;
- preprocessed bytes for the same probe;
- the number of `dingo::` events in Clang's `-ftime-trace` output, when
  requested.

Wall time is recorded for trends but is never a failure condition. Use the same
compiler, standard, optimization level, and probe on both sides of a
comparison.

```bash
uv run --locked python tools/cost/measure.py record \
    --include /path/to/baseline/include \
    --source test/lit/codegen-static-probes.cpp \
    --out build/cost/baseline.json
uv run --locked python tools/cost/measure.py record \
    --include include \
    --source test/lit/codegen-static-probes.cpp \
    --out build/cost/current.json
uv run --locked python tools/cost/measure.py compare \
    build/cost/baseline.json build/cost/current.json
```

The default tolerances are zero. Pass `--size-tolerance`,
`--header-tolerance`, or `--instantiation-tolerance` only when a declared change
justifies a budget increase.
