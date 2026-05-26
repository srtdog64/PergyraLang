# Soft Self-Host Parity Harness

Status: first rung-2 parity harness active.

This folder holds oracle comparisons for soft self-host tools. The C compiler
and existing shell/C smokes remain the source of truth until a tool can run in
Pergyra and produce deterministic JSON output that agrees with the C oracle.

The first active tool is `diagnostic_catalog_checker`. Its parity harness
checks:

- the C diagnostic-registry smoke exit class;
- the Pergyra tool exit class;
- deterministic clean JSON against `expected/clean.json`;
- `codes`, `documented`, `missing`, `duplicates`, and `orphans` counters
  against shell drift detectors.
- a synthetic missing-code fixture that must emit `ok:false`, one `findings[]`
  entry, and exit `1`.
- a synthetic missing-input fixture that must emit `input_error` and exit `1`.

Minimum parity contract for each tool:

- run the existing C or shell oracle against the same input;
- run the Pergyra tool against the same input;
- compare exit class first;
- compare JSON schema and stable counters incrementally while the Pergyra tool
  remains a partial implementation.
- compile/run copied Pergyra tool sources from an ignored build scratch
  directory such as `.tmp/self_hosted/...`; parity harnesses must not leave
  `.exe`, `.o`, `.d`, or probe artifacts beside `self_hosted/tools/*/main.pgy`.

No compiler-core self-host migration is allowed from this folder.
