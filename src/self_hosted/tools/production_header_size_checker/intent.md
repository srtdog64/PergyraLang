# Production Header Size Checker -- Intent / Contract

**Status:** *rung-2 minimal* (2026-05-27). Reads a committed manifest of
production header paths
(`fixture/headers_manifest.txt`), iterates each, reads the file content,
counts newlines, asserts <= 600 LOC per the BDFL split-review threshold.
Reports any over-cap headers in `findings[]`.

## Intent

The 600-LOC split-review threshold (`TODO.md Section 0 Core Rules`) is a beta-
closure invariant. The shell-side `tests/production_header_size_smoke.sh`
already gates this for production `.h` owners, but only as a binary
pass/fail. This tool is the *first* Pergyra-origin gate that emits a
structured per-header verdict, so downstream tooling can sort violations
by overage instead of grepping logs.

## Input Contract

- **manifest_owner**:
  `src/self_hosted/tools/production_header_size_checker/fixture/headers_manifest.txt`
  (text, UTF-8, one repo-relative `.h` path per line; produced by
  `find src/codegen src/runtime src/compiler src/semantic src/parser src/lsp
  -name '*.h' -type f | sort`). Regenerate the manifest before commit when
  the production header tree shifts.
- Each header content is `ReadFile`d relative to repository root.

## Output Contract

JSON document on stdout, conforming to schema
`pgy.selfhost.production-header-size.v1`:

```json
{
  "schema": "pgy.selfhost.production-header-size.v1",
  "ok": true,
  "source": {
    "manifest_owner": "src/self_hosted/tools/production_header_size_checker/fixture/headers_manifest.txt",
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
  `{ "kind": "header_over_cap" | "input_error" | "missing_manifest",
     "path": "src/...", "lines": <int>, "location": "..." }`.

Exit code: `0` on `ok:true`, `1` on `ok:false`.

## Oracle

The shell drift detector is `wc -l` per manifest entry + comparison
against the 600-LOC cap. The C-side oracle is
`tests/production_header_size_smoke.sh` itself; this tool re-derives the
verdict in Pergyra and the parity script asserts both agree on the count
of over-cap files (currently 0).

The parity rung (`src/self_hosted/parity/`) asserts:

- The Pergyra origin exits `0` on the clean repo.
- Emitted JSON byte-matches `expected/clean.json`.
- The `headers` count matches the manifest line count.
- The `violations` count matches a shell `wc -l + awk` ground truth.
- A synthetic over-cap fixture (insert a 700-line synthetic `.h` into a
  tmp manifest) yields `rc=1` with a `"kind":"header_over_cap"` finding.

## Why Now

This is the *eighth* soft self-host tool. It introduces the *manifest
input + per-line iteration with per-line file I/O* pattern -- the same
shape a future `imported-module audit` or `LLVM-pass inventory` tool
would use. The 489-header manifest also stress-tests the Pergyra `for`
loop + `ReadFile` throughput at a realistic scale.

## Not In Scope

- Auto-fix / suggested split points.
- `.c` file size gating (different threshold, different review process).
- Whitespace-stripped LOC counting (raw newline count is the gate).
- Live manifest regeneration vs committed manifest -- the committed
  manifest is a snapshot; drift between manifest and live tree is a
  separate gate.
