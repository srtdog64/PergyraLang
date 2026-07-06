# AIR Graph Node-Count Integrity -- Intent / Contract

**Status:** rung-1 minimal. The first AIR graph consumer that reads the *live*
`pgy --air-json` dump rather than a hand-authored fixture. It checks a
cross-summary invariant: the number of `"id":` fields across the
`intents` / `boundaries` / `evidence` arrays must equal
`intent_count + boundary_count + evidence_count` from the `summary` block the
compiler emits. The clean-output oracle is the TestHarness-projected
`expected/clean.json`.

## Intent

The earlier three AIR consumers (id uniqueness, referential integrity,
reachability) ran on hand-shaped fixtures, which proved the substrate but not the
real schema. This one closes that gap: it consumes the actual compiler output.
If a producer pass emits a node without bumping its count, or bumps a count
without emitting a node, `id_count != declared` and the invariant fires. This is
the smallest real consistency check that only the live dump can exercise.

## Input Contract

- **fixture_owner**:
  `src/self_hosted/tools/air_graph_json_validator/fixture/sample.json` -- the
  committed live `pgy --air-json` dump, already drift-guarded against the
  compiler by the air-graph-json-validator parity rung. This tool reuses that
  drift-guarded fixture rather than maintaining its own copy.

The parity runner passes the fixture path through `Args()[0]`; the default path
is retained only for local manual execution. Summary counts are read through
`air_graph_json_validator/scan_owner.pgy::AirGraphSummaryIntField`; this tool
does not own a second JSON summary-number scanner. Node id fields are read
through `AirGraphScalarFieldValues`, so this tool also does not own a second
AIR graph scalar-token scanner.

## Output Contract

JSON document on stdout, schema `pgy.selfhost.air-node-count.v1`:

```json
{
  "schema": "pgy.selfhost.air-node-count.v1",
  "ok": true,
  "source": {
    "input_schema": "pgy.air.graph.v1",
    "fixture_owner": "src/self_hosted/tools/air_graph_json_validator/fixture/sample.json"
  },
  "counts": { "ids": 14, "declared": 14, "intents": 1, "boundaries": 1, "evidence": 12 },
  "findings": []
}
```

- `ok = (id_count == intent_count + boundary_count + evidence_count)` and all
  three counts present (non-negative).
- `findings[]` carries one
  `{ "kind": "node_count_mismatch", "ids": N, "declared": M }` when they differ,
  or one `{ "kind": "input_error", ... }` when the dump is missing.

Exit code: `0` on `ok:true`, `1` on `ok:false`.

## Oracle

The clean-output oracle is the TestHarness-projected `expected/clean.json`,
compared through `backend_output_comparator`. Shell does not recompute `"id":`
or summary counts for the clean fixture. It remains only the process runner and
negative-fixture mutator.

The parity rung asserts:

- Pergyra origin exits `0` on the live dump and emits JSON byte-matching
  `expected/clean.json`.
- A corrupted dump (a summary count altered) yields `rc=1`, `ok:false`, and a
  `node_count_mismatch` finding.
- The C-compiled and LLVM-compiled Pergyra tool produce byte-identical output.

## Not In Scope

- Full JSON parsing or per-array id scoping.
- Verifying ids are dense or that per-array ids are 0-based contiguous (a later
  pass).
- Cross-referencing `boundary`/`intent` back-references (the referential
  integrity pass, adapted to the live schema, is the next promotion).
