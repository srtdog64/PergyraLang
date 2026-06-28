# AIR Graph Referential Integrity -- Intent / Contract

**Status:** rung-1 minimal. A consumer/analysis pass over an
AIR-graph-shaped JSON dump: it extracts node ids and directed edge endpoints,
then reports any `"from"` or `"to"` endpoint that has no matching node id.
The scalar AIR graph facts come from the shared AIR graph scan owner, not a
checker-local token scanner.

## Intent

AIR graph edges must only point at nodes that exist. A dangling endpoint is a
real IR defect: a producer pass deleted or failed to number a node while still
leaving an edge behind. This Pergyra origin checks that invariant with shared
AIR graph scalar fact reads and `Array<String>` accumulation.

## Input Contract

- **fixture_owner**:
  `src/self_hosted/tools/air_graph_ref_integrity/fixture/graph_edges.json`
  (text, UTF-8 without BOM, single-line AIR-graph-shaped JSON with `"id":`,
  `"from":`, and `"to":` numeric tokens).
- A dangling fixture
  (`fixture/graph_edges_dangling.json`) carries one endpoint with no matching
  node for the negative test.

The path is fixed relative to repository root; no CLI argument surface yet.

## Output Contract

JSON document on stdout, schema `pgy.selfhost.air-ref-integrity.v1`:

```json
{
  "schema": "pgy.selfhost.air-ref-integrity.v1",
  "ok": true,
  "source": {
    "input_schema": "pgy.air.graph.v1",
    "fixture_owner": "src/self_hosted/tools/air_graph_ref_integrity/fixture/graph_edges.json"
  },
  "counts": { "nodes": 3, "endpoints": 4, "dangling": 0 },
  "findings": []
}
```

- `ok = (counts.dangling == 0)`.
- `findings[]` carries one
  `{ "kind": "dangling_edge_endpoint", "id": "..." }` per dangling endpoint,
  or one `{ "kind": "input_error", ... }` when the fixture is missing.

Exit code: `0` on `ok:true`, `1` on `ok:false`.

## Oracle

The shell ground truth extracts node ids and edge endpoints with `grep`, then
uses `comm` set difference to count endpoints missing from the node-id set. The
Pergyra origin must report zero dangling endpoints on the clean fixture and a
`dangling_edge_endpoint` finding on the negative fixture.

The parity rung asserts:

- Pergyra origin exits `0` on the clean fixture and emits JSON byte-matching
  `expected/clean.json`.
- The clean fixture has zero dangling endpoints according to both shell and
  Pergyra.
- The dangling fixture yields `rc=1`, `ok:false`, and a
  `dangling_edge_endpoint` finding.
- The C-compiled and LLVM-compiled Pergyra tool produce byte-identical output.

## Not In Scope

- Full JSON DOM construction; the shared owner still provides bounded fact
  reads, not a general-purpose tree.
- Reachability from roots.
- Dense or monotonic id numbering.
