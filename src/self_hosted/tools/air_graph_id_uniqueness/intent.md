# AIR Graph Node-Id Uniqueness -- Intent / Contract

**Status:** rung-1 minimal. A consumer/analysis pass over an AIR-graph-shaped
JSON dump: it extracts every node id and asserts each id is unique. Duplicate
node ids in the compiler IR graph are a real defect, so this is an invariant
check on the graph contents, not a schema validator. No JSON parser --
substring-anchored scan only, the same surface the AIR graph JSON validator
established.

## Intent

The AIR graph dump assigns an id to every node. A producer pass that reuses an
id, or a merge that fails to renumber, yields a graph where two distinct nodes
share an id -- a defect that downstream passes silently mishandle. This Pergyra
origin reads the dumped ids and proves the set has no collision, dogfooding the
`Array<String>` accumulate-and-search substrate that compiler symbol indices
need.

## Input Contract

- **fixture_owner**:
  `src/self_hosted/tools/air_graph_id_uniqueness/fixture/graph_ids.json`
  (text, UTF-8 without BOM, single-line AIR-graph-shaped JSON with `"id":`
  node entries).
- A duplicate fixture
  (`fixture/graph_ids_dup.json`) carries one repeated id for the negative test.

The path is fixed relative to repository root; no CLI argument surface yet.

## Output Contract

JSON document on stdout, schema `pgy.selfhost.air-id-uniqueness.v1`:

```json
{
  "schema": "pgy.selfhost.air-id-uniqueness.v1",
  "ok": true,
  "source": {
    "input_schema": "pgy.air.graph.v1",
    "fixture_owner": "src/self_hosted/tools/air_graph_id_uniqueness/fixture/graph_ids.json"
  },
  "counts": { "ids": 4, "distinct": 4, "duplicates": 0 },
  "findings": []
}
```

- `ok = (counts.duplicates == 0)`.
- `findings[]` carries one `{ "kind": "duplicate_id", "id": "..." }` per repeated
  id (each later occurrence past the first), or one
  `{ "kind": "input_error", ... }` when the fixture is missing.

Exit code: `0` on `ok:true`, `1` on `ok:false`.

## Oracle

The shell ground truth is `grep -oE '"id":[^,}]*'` over the same fixture, piped
through `sort | uniq -d` to surface duplicates. The Pergyra origin must agree:
zero duplicate lines on the clean fixture, exactly one on the dup fixture.

The parity rung asserts:

- Pergyra origin exits `0` on the clean fixture and emits JSON byte-matching
  `expected/clean.json`.
- The dup fixture yields `rc=1` and `ok:false` with a `duplicate_id` finding.
- The C-compiled and LLVM-compiled Pergyra tool produce byte-identical output
  (`assert_llvm_leg`).

## Not In Scope

- Full JSON parsing (escapes, nested objects, mixed arrays).
- Validating that ids are dense or monotonic; only uniqueness.
- Cross-checking ids against edge endpoints (a later reachability pass).
