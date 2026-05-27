# Backend Output Comparator -- Intent / Contract

**Status:** *rung-2 minimal* (2026-05-27). Reads two output files (`expected.txt`
and `actual.txt`) from a fixed fixture path, compares line-by-line, emits a
verdict JSON, and exits `1` on any mismatch. No diff payload yet -- only
counts and a small `findings[]` enumeration of the first few mismatching
lines.

## Intent

C/LLVM parity is the spine of the staged self-host plan. When parity drifts,
the existing `tests/compare_backends.sh` driver catches it but only as a
binary pass/fail. This tool is the *first* Pergyra-origin gate that reads
two text outputs and emits a structured diff verdict, so downstream tooling
can route on `counts.mismatch_lines` instead of grepping logs.

## Input Contract

- **expected_owner**: `self_hosted/tools/backend_output_comparator/fixture/expected.txt`
  (text, UTF-8, line-oriented).
- **actual_owner**: `self_hosted/tools/backend_output_comparator/fixture/actual.txt`
  (text, UTF-8, line-oriented).

Both paths are fixed relative to repository root. CLI argument parsing is
not yet on the Pergyra surface; the parity script swaps fixture contents
via a temp directory to exercise both match and mismatch scenarios.

## Output Contract

JSON document on stdout, conforming to schema
`pgy.selfhost.backend-output-comparator.v1`:

```json
{
  "schema": "pgy.selfhost.backend-output-comparator.v1",
  "ok": true,
  "source": {
    "expected_owner": "self_hosted/tools/backend_output_comparator/fixture/expected.txt",
    "actual_owner": "self_hosted/tools/backend_output_comparator/fixture/actual.txt"
  },
  "counts": {
    "expected_lines": 0,
    "actual_lines": 0,
    "mismatch_lines": 0,
    "extra_actual_lines": 0,
    "missing_actual_lines": 0
  },
  "findings": []
}
```

- `ok = (counts.mismatch_lines == 0 && counts.extra_actual_lines == 0 &&
  counts.missing_actual_lines == 0)`.
- `findings[]` carries at most the first 8 mismatching line indices:
  `{ "kind": "mismatch" | "extra" | "missing" | "input_error",
     "line": <int>, "location": "..." }`.

Exit code: `0` on `ok:true`, `1` on `ok:false`. Missing input reports
`input_error` and exits `1`.

## Oracle

The shell drift detector is `diff -q expected.txt actual.txt` plus
`wc -l` for line counts. There is no existing C-side compiler smoke
that emits this structured JSON; the Pergyra origin is the primary
implementation and the shell `diff` is the auxiliary parity backend.

The parity rung (`self_hosted/parity/`) asserts:

- The Pergyra origin exits `0` when the committed fixture matches.
- The emitted JSON byte-matches `expected/clean.json`.
- A synthetic mismatch fixture (`actual.txt` replaced with a 1-line drift)
  yields `rc=1` and `ok:false` with `counts.mismatch_lines >= 1`.
- A synthetic missing-input fixture (delete `actual.txt`) yields `rc=1`
  and `ok:false` with `input_error` finding.

## Why Now

This is the *fourth* soft self-host tool. The first three operated on
single inputs (file + macro registry, manifest doc, JSON dump). This one
exercises *paired* input -- read two files of comparable shape and
report structured drift. It exposes the parallel-array-iterate pattern
without forcing a new language lift. The shell `diff` is the fallback
oracle; the Pergyra origin is the structured-JSON owner.

## Not In Scope

- Full diff payload (added/removed line text, hunks, context lines).
- Multi-pair input (CLI args needed first).
- Whitespace-insensitive or trailing-newline-tolerant comparison.
- Source/target file format auto-detection.
