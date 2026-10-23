# AST graph atom prebind census

Status: RED integration receipt. Observed 2026-10-23 KST.

## Objective and boundary

Expression-graph text used by typed-AST binders must not grow an already
escaped `AstArena.atom_table`. The fact owner is
`AstTreeArtifactFromTextWithExpressionGraphs`; the last reached consumers are
the expression-owner and destructure binders, followed by the existing MIR
arena-storage retirement boundary. The forbidden fallback is a late
`ArrayPushOwnedString`, a new deep-drop claim for borrowed graph strings, or a
weaker ownership refusal.

Exact source commit:
`4716f9539b84841c507010bd919a427cfb73cc86`.

## Implemented contract

- Atom slot zero remains the empty sentinel.
- One slot per expression-graph node is reserved before the table escapes,
  then filled from the retained graph owner.
- The graph owner and the borrowed String descriptors leave in one
  `AstTreeArtifact`.
- Binders validate graph-node identity and text, then write only scalar atom
  indices. They do not grow the escaped String array.
- The final cleanup remains shallow Array-backing retirement; this slice does
  not invent String ownership or a manual deep drop.

## Focused evidence

- `tests/self_hosted_component_contract_smoke.sh`: PASS, including 14 checker
  unit tests, source-size caps and generator drift.
- `ast_graph_atom_prebind_valid.pgy`: native C and LLVM both ran with exact
  output `graph` then `tree`.
- `ast_graph_atom_late_owned_growth_invalid.pgy`: native C and LLVM both
  refused before publishing an artifact with `PGY_SEM_BORROW_ESCAPE`.
- The existing collection-ownership parity owner now carries both fixtures;
  its installed-driver lanes remain downstream of canonical admission.

## Fixed canonical census

Input: unchanged
`src/self_hosted/compiler/driver_bootstrap_main.pgy`.

Instrumented native checker SHA-256:
`b1a0e7d3c83c50755d93b771dc9ca83d2e7b569598aecf36683edae8838ae31c`.

| Result | Before | After |
|---|---:|---:|
| Final semantic ownership refusals | 310 | 309 |
| Semantic additions | 0 | 0 |
| Semantic removals | 0 | 1 |
| Hard census errors | 0 | 0 |

The removed row is the escaped-storage refusal in
`AstTreeArtifactBindExpressionOwnerKinds`. Location and import-root paths were
normalized before multiset comparison. No target producer, binder or final
arena-retirement row remains in the final census generation. The exact-source
run took 1432.40 s and peaked at 3,976,804 KiB. Raw evidence remains outside
the repository at `/root/cc-graph-atom-v3/`.

## Remaining boundary

The full canonical program is still refused with 309 semantic rows. The next
read-only map covers the eight owned-String release rows across four release
owners before choosing a shared contract. Bootstrap, gen2 == gen3, the
installed C/LLVM driver and exact-SHA CI were not reached. This receipt does
not claim V1 closure or whole-language memory safety.
