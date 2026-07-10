# Verified Projection Plan: Intent Observability Row

Status: `gate-backed-partial`

This document records the first native `VerifiedProjectionPlan` row. It closes
one decision only: whether the C or LLVM projection erases intent-observability
runtime support (`OBS0`) or materializes it (`OBS1`). It does not claim that the
full projection planner, AIR certificate join, or all runtime materialization
axes are complete.

## Owners

`src/compiler/mir_surface_usage.c` captures the canonical MIR inventory fact:

- `has_inventory_surface_usage_facts`
- `inventory_uses_intent_observability_surface`

`src/compiler/verified_projection_plan.c` consumes that fact and emits one
`PgyVerifiedProjectionPlanRow`:

| MIR fact | disposition | runtime profile | reason |
|---|---|---|---|
| false | `ERASE` | `OBS0` | `mir:inventory:no_intent_observability_surface` |
| true | `MATERIALIZE` | `OBS1` | `mir:inventory:intent_observability_surface` |

Missing MIR or missing inventory facts fail closed. The planner does not scan
AST, HIR direct calls, routine names, instruction strings, or source payloads.
C and LLVM select the same row after MIR validation; neither backend owns a
second observability-usage query.

The call ABI has one independent owner:
`src/common/intent_observability_abi.c`. Its 51 source-name-sorted rows own a
stable, non-zero, unique `RuntimeCallAbiId`, source name, runtime symbol, arity,
argument kind, and return kind. The current family-level argument fact is `Int`
for every handle/index argument and is consumed by semantic and LLVM type
projection rather than duplicated there. IDs are append-only identities, not
sorted row positions; a new source name receives a fresh ID without
renumbering existing rows.
Semantic checking, builtin classification, C emission, LLVM emission, and LLVM
runtime declaration consume those rows. The previous names-only table,
per-call `BuiltinKind` aliases, semantic spec table, and backend-local runtime
symbol tables are retired.

## Gate

`make verified-projection-plan-test-smoke` proves:

- missing MIR usage facts are rejected;
- `OBS0` maps to `ERASE` and `OBS1` maps to `MATERIALIZE`;
- C and LLVM consume target-specific rows from the same planner;
- all 51 ABI rows have unique non-zero IDs and deterministic source-name order;
- semantic/C/LLVM consumers use the canonical ABI owner;
- retired aliases, backend-local runtime symbols, and AST/HIR fallback scans
  cannot return;
- a negative source-inference mutation is detected.

## Remaining Closure

The native plan currently has one axis row and a MIR fact witness. Full
projection closure still requires AIR certificate/digest binding, typed layout
and cleanup rows, target capability rows, materialization residue attribution,
Artifact Zone identity, and self-hosted consumption of the same plan.
