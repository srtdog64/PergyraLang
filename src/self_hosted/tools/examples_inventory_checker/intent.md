# Examples Inventory Checker -- Intent / Contract

**Status:** *rung-2 DirWalk-owned* (2026-06-15). Enumerates
top-level `examples/*.pgy` paths through `DirWalk("examples")`, verifies the
expected inventory count, and checks each file has at least one
newline-terminated line. Reports empty files or inventory count drift as
findings.

## Intent

The `examples/` directory is the canonical user-facing surface for showing
Pergyra in action. When a file gets accidentally truncated, replaced
with an empty placeholder, truncated without a terminating newline, or
removed without manifest update, the dogfood loop silently misses the
regression. This tool surfaces that drift by gating presence +
non-emptiness per file.

## Input Contract

- **dirwalk_owner**: `examples`. The tool calls `DirWalk("examples")`, filters
  top-level `.pgy` files, and treats that sorted snapshot as the inventory
  source of truth.
- **expected_examples**: `120`. Count changes are intentional surface changes
  and must update the tool expectation and clean fixture together.
- Each example content is `ReadFile`d relative to repository root.

## Output Contract

JSON document on stdout, conforming to schema
`pgy.selfhost.examples-inventory.v1`:

```json
{
  "schema": "pgy.selfhost.examples-inventory.v1",
  "ok": true,
  "source": {
    "dirwalk_owner": "examples",
    "expected_examples": 120
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

- `ok = (counts.examples == source.expected_examples && counts.empty == 0)`.
- `findings[]` carries one entry per drift, capped at 8:
  `{ "kind": "inventory_count_drift" | "empty_example",
     "path": "examples/...", "location": "..." }`.

Exit code: `0` on `ok:true`, `1` on `ok:false`.

## Oracle

The Pergyra origin is the inventory owner: `DirWalk("examples")` supplies the
sorted file snapshot and `TextScan.CountLines` supplies line counts. The shell
harness only builds the tool, checks byte-equal clean JSON, and constructs a
negative fixture; it no longer owns the clean inventory list.

The parity rung (`src/self_hosted/parity/`) asserts:

- The Pergyra origin exits `0` on the clean repo.
- Emitted JSON byte-matches `expected/clean.json`.
- A synthetic count-drift fixture (copy top-level examples and omit one
  file) yields `rc=1` with a `"kind":"inventory_count_drift"` finding.

## Why Now

This is the *tenth* soft self-host tool and the *third* consumer of
`TextScan.CountLines` -- it triggered the `CountLines` lift from inline
duplicates in tools 8 and 9 into `src/self_hosted/lib/text_scan.pgy`. The
The earlier manifest-driven pattern was validated across header and `.c`
inventories; this tool now proves the directory-walk substrate can own a
real self-hosted inventory.

## Not In Scope

- Compile-validation of each example (would require subprocess pgy).
- `func Main()` presence check (some examples are library modules).
- Recursive campaign examples (`examples/<game>/`) are deliberately filtered
  out; this checker owns the top-level user-facing example set.
