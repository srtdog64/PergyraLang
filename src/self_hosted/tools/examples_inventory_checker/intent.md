# Examples Inventory Checker -- Intent / Contract

**Status:** *rung-2 minimal* (2026-05-27). Reads a committed manifest of
`examples/*.pgy` paths, iterates each, verifies it exists and has at
least one newline-terminated line. Reports empty / missing files as findings.

## Intent

The `examples/` directory is the canonical user-facing surface for showing
Pergyra in action. When a file gets accidentally truncated, replaced
with an empty placeholder, truncated without a terminating newline, or
removed without manifest update, the dogfood loop silently misses the
regression. This tool surfaces that drift by gating presence +
non-emptiness per file.

## Input Contract

- **manifest_owner**:
  `src/self_hosted/tools/examples_inventory_checker/fixture/examples_manifest.txt`
  (text, UTF-8, one repo-relative example path per line; produced by
  `find examples -maxdepth 1 -name '*.pgy' -type f | sort`).
- Each example content is `ReadFile`d relative to repository root.

## Output Contract

JSON document on stdout, conforming to schema
`pgy.selfhost.examples-inventory.v1`:

```json
{
  "schema": "pgy.selfhost.examples-inventory.v1",
  "ok": true,
  "source": {
    "manifest_owner": "src/self_hosted/tools/examples_inventory_checker/fixture/examples_manifest.txt"
  },
  "counts": {
    "examples": 0,
    "missing": 0,
    "empty": 0,
    "max_lines": 0
  },
  "findings": []
}
```

- `ok = (counts.missing == 0 && counts.empty == 0)`.
- `findings[]` carries one entry per drift, capped at 8:
  `{ "kind": "missing_example" | "empty_example" | "missing_manifest",
     "path": "examples/...", "location": "..." }`.

Exit code: `0` on `ok:true`, `1` on `ok:false`.

## Oracle

The shell drift detector is `find + wc -l` per manifest entry. There is
no existing C-side smoke for the examples inventory contract today; the
Pergyra origin is the primary implementation and the shell `find + wc`
loop is the auxiliary parity backend.

The parity rung (`src/self_hosted/parity/`) asserts:

- The Pergyra origin exits `0` on the clean repo.
- Emitted JSON byte-matches `expected/clean.json`.
- `examples / missing / empty / max_lines` match shell ground truth.
- A synthetic missing-example fixture (delete one manifest target's
  file) yields `rc=1` with a `"kind":"missing_example"` finding.

## Why Now

This is the *tenth* soft self-host tool and the *third* consumer of
`TextScan.CountLines` -- it triggered the `CountLines` lift from inline
duplicates in tools 8 and 9 into `src/self_hosted/lib/text_scan.pgy`. The
manifest-driven pattern is now well-validated across header (489),
`.c` (793), and example (117) inventories.

## Not In Scope

- Compile-validation of each example (would require subprocess pgy).
- `func Main()` presence check (some examples are library modules).
- Recursive subdirectory scans (`examples/<game>/` campaigns not
  covered here; would need ListDir).
