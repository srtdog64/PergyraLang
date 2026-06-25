# Production Header Size Checker -- Intent / Contract

**Status:** *rung-2 DirWalk-owned* (2026-06-15). Enumerates production header
paths through `DirWalk("src")`, filters the same six owner prefixes as
`tests/production_header_size_smoke.sh`, reads each file, counts newlines, and
asserts <= 600 LOC per the BDFL split-review threshold. Reports any over-cap
headers in `findings[]`.

## Intent

The 600-LOC split-review threshold (`TODO.md Section 0 Core Rules`) is a beta-
closure invariant. The shell-side `tests/production_header_size_smoke.sh`
already gates this for production `.h` owners, but only as a binary
pass/fail. This tool is the *first* Pergyra-origin gate that emits a
structured per-header verdict, so downstream tooling can sort violations
by overage instead of grepping logs.

## Input Contract

- **dirwalk_owner**: `src`.
- **filter**: `production_header_smoke_scope`, meaning `.h` files under
  `src/codegen`, `src/runtime`, `src/compiler`, `src/semantic`, `src/parser`,
  and `src/lsp`.
- Each header content is `ReadFile`d relative to repository root.

## Output Contract

JSON document on stdout, conforming to schema
`pgy.selfhost.production-header-size.v1`:

```json
{
  "schema": "pgy.selfhost.production-header-size.v1",
  "ok": true,
  "source": {
    "dirwalk_owner": "src",
    "filter": "production_header_smoke_scope",
    "cap_lines": 600
  },
  "counts": {
    "headers": 0,
    "violations": 0,
    "max_lines": 0
  },
  "findings": []
}
```

- `ok = (counts.violations == 0)`.
- `findings[]` carries one entry per over-cap header, capped at 8:
  `{ "kind": "header_over_cap",
     "path": "src/...", "lines": <int>, "location": "..." }`.

Exit code: `0` on `ok:true`, `1` on `ok:false`.

## Oracle

The shell drift detector is `find + wc -l + awk` over the same prefix filter
and comparison against the 600-LOC cap. The C-side oracle is
`tests/production_header_size_smoke.sh` itself; this tool re-derives the
verdict in Pergyra and the parity script asserts both agree on file count,
violation count, and max line count.

The parity rung (`tests/self_hosted/parity/`) asserts:

- The Pergyra origin exits `0` on the clean repo.
- Emitted JSON byte-matches `expected/clean.json`.
- The `headers / violations / max_lines` counts match shell ground truth.
- A synthetic over-cap fixture (a 701-line `.h` under `src/runtime`) yields
  `rc=1` with a `"kind":"header_over_cap"` finding.

## Why Now

This is the *eighth* soft self-host tool. It originally introduced the
manifest input + per-line file I/O pattern. The current form closes that
manifest alias and makes the Pergyra tool own the production header inventory
through `DirWalk`, while still stress-testing Pergyra `ReadFile` throughput at
a realistic scale.

## Not In Scope

- Auto-fix / suggested split points.
- `.c` file size gating (different threshold, different review process).
- Whitespace-stripped LOC counting (raw newline count is the gate).
