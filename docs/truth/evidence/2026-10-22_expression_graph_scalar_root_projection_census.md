# Expression graph scalar-root projection census

Recorded 2026-10-22 KST from local `main` base
`167e4903eb74383a088befb58af9618408827c30` plus the measured source diff.
The unchanged canonical input was
`src/self_hosted/compiler/driver_bootstrap_main.pgy`.

## Change measured

- `SemanticExpressionGraphFacts` remains the graph owner.
- `SemanticExpressionGraphRootAt` returns only `Option<Int>` for consumers
  that need a root identity but not graph storage.
- `SemanticAstExpressionSurfaceFactsWithGraph` uses that projection while
  validating the three expression lanes. The graph-carrying `ViewAt` contract
  remains available to consumers that actually traverse the graph.
- A range-missing root is checked as `None`; there is no default root and no
  whole-graph `Clone`.

The measured source hashes were:

- graph node view owner: `fb30bb084b90df652ed1d8e5cc13c35bb2c38202f12eeb5f898941dafc97a911`
- surface fact owner: `c4cc7f6ee9eb21f69d4d1bd6e7daea86781c948fdd47936c2a5cce1a8f5eb392`
- surface contract owner: `dbfa89624f8864f742c96c2148128c72f56cdb22bd04ac8e34ac11532d981fde`

## Focused evidence

- All three changed Pergyra files loaded through the current native AST path.
- `tests/self_hosted_component_contract_smoke.sh`: PASS, including line caps
  and generator drift.
- A self-contained graph/root falsifier compiled and ran on C and LLVM; both
  printed `20`, kept using the source graph after two view calls, and rejected
  the range-missing root as `None`.
- The full expression-surface parity executable remains unreachable through
  the current self-host build because the existing canonical ownership
  baseline is red. It is not recorded as a pass.

## Fixed final-owner census

The successful prior census instrumented binary was reused unchanged
(`b1a0e7d3c83c50755d93b771dc9ca83d2e7b569598aecf36683edae8838ae31c`).
Only the current self-host source snapshot was replaced. A first attempt to
regenerate that binary was discarded before analysis because an external
scratch instrumentation patch no longer matched the native C source.

| Measure | Before | After |
|---|---:|---:|
| Final refusals | 350 | 342 |
| Outside `direct_mir_*` | 308 | 300 |
| Outside `direct_mir_*` moved-value refusals | 102 | 94 |
| `SemanticExpressionGraphViewAt` move refusals | 8 | 0 |
| Hard census errors | 0 | 0 |

The new run performed three landing analyses and the final analysis in
951.49 s, peaking at 3,981,392 KiB. A normalized multiset comparison by
module, function, node kind, callee and diagnostic meaning found exactly the
eight intended removals and no added refusal. One retained `ArrayDrop`
diagnostic's generated syntax IDs shifted with the added source declarations;
its location and diagnostic meaning were unchanged.

The V1 finish line remains open: 342 ownership refusals remain, the unchanged
canonical program is not admitted, and bootstrap, installed-driver and
exact-SHA CI evidence were therefore not run for this red candidate.
