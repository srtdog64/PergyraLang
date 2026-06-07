# Production C Size Checker -- Intent / Contract

**Status:** *rung-2 minimal* (2026-05-27). Sister tool to the production
header size checker, but covering production `.c` translation units against
the 900-LOC hard cap. The 600-LOC line remains a split-review signal, not a
mechanical failure threshold for coherent `.c` owners.

## Intent

The 600-LOC split-review threshold is a beta-closure review signal for
production owners. Production `.h` owners still use the stricter 600-LOC
hard gate because header fan-out amplifies compile-time and include-boundary
debt. Production `.c` translation units use a 900-LOC hard cap so coherent
implementation owners are not mechanically split into helper buckets.

## Input Contract

- **manifest_owner**:
  `src/self_hosted/tools/production_c_size_checker/fixture/c_files_manifest.txt`
  (text, UTF-8, one repo-relative `.c` path per line; produced by
  `find src/codegen src/runtime src/compiler src/semantic src/parser src/lsp
  -name '*.c' -type f | sort`).
- Each `.c` file content is `ReadFile`d relative to repository root.

## Output Contract

JSON document on stdout, conforming to schema
`pgy.selfhost.production-c-size.v1`:

```json
{
  "schema": "pgy.selfhost.production-c-size.v1",
  "ok": true,
  "source": {
    "manifest_owner": "src/self_hosted/tools/production_c_size_checker/fixture/c_files_manifest.txt",
    "cap_lines": 900
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
  `{ "kind": "c_over_cap" | "input_error" | "missing_manifest",
     "path": "src/...", "lines": <int>, "location": "..." }`.

Exit code: `0` on `ok:true`, `1` on `ok:false`.

## Oracle

The shell drift detector is `wc -l` per manifest entry. There is no
existing C-side smoke for the `.c` cap today (TODO Section 0 calls it a
"split-review threshold" for `.c`); the Pergyra origin is the primary
implementation and the shell `find + wc -l + awk` is the auxiliary parity
backend.

The parity rung (`src/self_hosted/parity/`) asserts:

- The Pergyra origin exits `0` on the clean repo.
- Emitted JSON byte-matches `expected/clean.json`.
- `c_files / violations / max_lines` match shell ground truth.
- A synthetic over-cap fixture (701-line generated `.c` appended to a tmp
  manifest) yields `rc=1` with a `"kind":"c_over_cap"` finding.

## Why Now

This is the *ninth* soft self-host tool. It is the closest sibling to tool
8 (`production_header_size_checker`) -- same shape, different file class.
Shipping the pair together completes the "production file size gate" axis
for soft self-host. The duplication overlap with tool 8 makes both tools
candidate consumers for a shared `lib/text_scan.pgy::CountLines` helper
once a *third* tool wants the `wc -l`-style line counter.

## Not In Scope

- Auto-split suggestions.
- Comment/whitespace-stripped LOC counts (raw newline counts mirror the
  `wc -l` gate).
- `.h` files (covered by tool 8).
