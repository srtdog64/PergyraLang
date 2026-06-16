# AIR Graph Live Reference Integrity -- Intent / Contract

**Status:** rung-1 minimal. A live-schema AIR graph consumer over the
drift-guarded `pgy --air-json` fixture. It checks that real dump back-references
stay in range: every evidence `"boundary"` reference is either `null` or names
an existing boundary id, and every boundary `"intent"` reference names an
existing intent id.

## Intent

The fixture-shaped `air_graph_ref_integrity` checker proves generic edge
endpoint checking over `"from"` / `"to"` pairs. The live AIR dump uses a
different shape: dense per-array ids and back-reference fields. This checker
closes that schema gap without writing a full JSON parser. It uses the
compiler-owned summary counts as the bounds for valid references.

## Input Contract

- **fixture_owner**:
  `src/self_hosted/tools/air_graph_json_validator/fixture/sample.json` -- the
  committed drift-guarded AIR graph fixture produced from `pgy --air-json`.

The path is fixed relative to repository root; no CLI argument surface yet.

## Output Contract

JSON document on stdout, schema `pgy.selfhost.air-ref-live.v1`:

```json
{
  "schema": "pgy.selfhost.air-ref-live.v1",
  "ok": true,
  "source": {
    "input_schema": "pgy.air.graph.v1",
    "fixture_owner": "src/self_hosted/tools/air_graph_json_validator/fixture/sample.json"
  },
  "counts": {
    "boundary_refs_dangling": 0,
    "intent_refs_dangling": 0,
    "dangling": 0
  },
  "findings": []
}
```

- `ok = (counts.dangling == 0)`.
- `findings[]` carries one
  `{ "kind": "dangling_reference", "boundary_refs": N, "intent_refs": M }`
  when any reference falls outside its summary count, or one
  `{ "kind": "input_error", ... }` when the dump is missing.

Exit code: `0` on `ok:true`, `1` on `ok:false`.

## Oracle

The shell ground truth extracts `intent_count` and `boundary_count`, then scans
`"intent":` and `"boundary":` references. A non-null reference is valid iff it
is in `[0, count)`. The Pergyra origin must report zero dangling references on
the clean fixture and detect a corrupted reference fixture.

The parity rung asserts:

- Pergyra origin exits `0` on the live dump and emits JSON byte-matching
  `expected/clean.json`.
- The clean fixture has zero dangling references according to both shell and
  Pergyra.
- A corrupted dump yields `rc=1`, `ok:false`, and a `dangling_reference`
  finding.
- The C-compiled and LLVM-compiled Pergyra tool produce byte-identical output.

## Not In Scope

- Full JSON parsing.
- Validating every object kind in the AIR graph schema.
- Reachability or id uniqueness, which are separate consumers.
