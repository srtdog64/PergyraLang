# Production C Size Checker -- Intent / Contract

**Status:** *rung-2 DirWalk-owned* (2026-07-06). Sister tool to the production
header size checker, but covering production `.c` translation units against
the 699-LOC hard cap. It enumerates `DirWalk("src")` and filters the same
production `.c` scope as `tests/test_inc_size_smoke.sh`.

## Intent

The 699-LOC hard cap is the beta-closure threshold for production `.c`
owners. Production `.h` owners still use the stricter 600-LOC hard gate
because header fan-out amplifies compile-time and include-boundary debt.
Production `.c` translation units must stay in the 600s; coherent owners
that grow past that line need a responsibility-named split, not a generic
helper bucket.

## Input Contract

- **dirwalk_owner**: `src`.
- **filter**: `test_inc_size_production_c_scope`, meaning `src/**/*.c` files
  excluding `src/tests/*` and files whose basename starts with `test_`.
- Each `.c` file content is `ReadFile`d relative to repository root.

## Output Contract

JSON document on stdout, conforming to schema
`pgy.selfhost.production-c-size.v1`:

```json
{
  "schema": "pgy.selfhost.production-c-size.v1",
  "ok": true,
  "source": {
    "dirwalk_owner": "src",
    "filter": "test_inc_size_production_c_scope",
    "cap_lines": 699
  },
  "counts": {
    "c_files": 0,
    "violations": 0,
    "max_lines": 0
  },
  "findings": []
}
```

- `ok = (counts.violations == 0)`.
- `findings[]` carries one entry per over-cap file, capped at 8:
  `{ "kind": "c_over_cap",
     "path": "src/...", "lines": <int>, "location": "..." }`.

Exit code: `0` on `ok:true`, `1` on `ok:false`.

## Oracle

The clean oracle is the committed expected JSON artifact
`expected/clean.json`, compared through the shared ArtifactZone/TestHarness
path. The broader C-side cap is still enforced separately by
`tests/test_inc_size_smoke.sh`; this self-hosted parity rung must not recompute
the clean production C counts in shell.

The parity rung (`tests/self_hosted/parity/`) asserts:

- The Pergyra origin exits `0` on the clean repo.
- Emitted JSON byte-matches `expected/clean.json` with `max_lines` normalized
  to avoid fixture churn on ordinary line-count drift.
- A synthetic over-cap fixture (1001-line generated `.c` under `src/runtime`)
  yields `rc=1` with a `"kind":"c_over_cap"` finding.
- Both C and LLVM legs compile the same self-hosted checker.

If clean inventory semantics drift, update the committed expected artifact or
the self-hosted owner. Do not add a shell `find`/`wc` implementation as a
second clean oracle.

## Why Now

This is the *ninth* soft self-host tool. It is the closest sibling to tool
8 (`production_header_size_checker`) -- same shape, different file class.
The current form closes the stale manifest alias and makes the Pergyra tool
own production C inventory through `DirWalk`.

## Not In Scope

- Auto-split suggestions.
- Comment/whitespace-stripped LOC counts (raw newline counts mirror the
  `wc -l` gate).
- `.h` files (covered by tool 8).
