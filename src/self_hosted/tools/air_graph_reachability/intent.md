# AIR Graph Reachability -- Intent / Contract

**Status:** rung-1 minimal. A consumer/analysis pass over an AIR-graph-shaped
JSON dump: it runs a worklist reachability from the declared root(s) over the
directed edges and reports any node no path reaches (an orphan). Orphan IR nodes
are dead work and often a symptom of a producer pass that dropped an edge. This
is the third and richest of the AIR graph consumer passes, after node-id
uniqueness and edge referential integrity. The scalar AIR graph facts come from
the shared AIR graph scan owner, not a checker-local token scanner.

## Intent

Reachability is the canonical graph fixpoint a compiler runs (dead-code
elimination, live-set, dominators all start here). Doing it in Pergyra proves
the worklist substrate the hard-self-host tier requires: a push-only
`Array<String>` frontier with an advancing cursor (a BFS queue without pop) plus
a visited `Array<String>`. No `Set`, no `Queue` pop, no recursion -- only the
push/index operations already proven safe.

## Input Contract

- **fixture_owner**:
  `src/self_hosted/tools/air_graph_reachability/fixture/graph_rooted.json`
  (text, UTF-8 without BOM, single-line AIR-graph-shaped JSON). The graph
  declares one or more `"root":<id>` entry points, `"id":` nodes, and
  `"from":`/`"to":` directed edges.
- An orphan fixture (`fixture/graph_orphan.json`) drops the edge into one node so
  it is unreachable, for the negative test.

The path is fixed relative to repository root; no CLI argument surface yet.

## Output Contract

JSON document on stdout, schema `pgy.selfhost.air-reachability.v1`:

```json
{
  "schema": "pgy.selfhost.air-reachability.v1",
  "ok": true,
  "source": {
    "input_schema": "pgy.air.graph.v1",
    "fixture_owner": "src/self_hosted/tools/air_graph_reachability/fixture/graph_rooted.json"
  },
  "counts": { "nodes": 3, "reachable": 3, "orphans": 0 },
  "findings": []
}
```

- `ok = (counts.orphans == 0)`.
- `findings[]` carries one `{ "kind": "orphan_node", "id": "..." }` per node not
  reached from any root, or one `{ "kind": "input_error", ... }` when the fixture
  is missing.

Exit code: `0` on `ok:true`, `1` on `ok:false`.

## Oracle

The shell ground truth on the clean fixture is the node count (`grep -oE
'"id":[0-9]+' | wc -l`); the Pergyra origin must report `reachable == nodes` and
`orphans == 0`. The negative fixture must yield `rc=1` with an `orphan_node`
finding. A full shell BFS is intentionally not reimplemented; the byte-equal
committed `expected/clean.json` plus the negative fixture pin the behavior.

The parity rung asserts:

- Pergyra origin exits `0` on the clean fixture and emits JSON byte-matching
  `expected/clean.json`.
- `reachable` equals the shell node count and `orphans` is 0 on the clean
  fixture.
- The orphan fixture yields `rc=1`, `ok:false`, and an `orphan_node` finding.
- The C-compiled and LLVM-compiled Pergyra tool produce byte-identical output.

## Not In Scope

- Full JSON DOM construction; the shared owner still provides bounded fact
  reads, not a general-purpose tree.
- Cycle reporting, dominator/postdominator structure (later passes).
- Multi-edge weighting or labelled edges.
