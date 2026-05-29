# Beta Readiness Checklist

This file is now the lightweight index for the beta closure checklist. The
checklist body was split on 2026-05-29 because the single file had become too
large to review safely.

Current strict beta readiness remains about **72-74%**. Do not call this 75% or
80% until the P0 source-of-truth closures below have consumer-completeness and
smoke evidence.

## Shards

- `docs/100a_beta_active_status.md` - current status, active blocker summary,
  and recent closure context.
- `docs/100b_beta_p0_semantics_systems_air.md` - formal semantics, systems
  baseline, CFG/body dataflow, core semantic closure, runtime panic/security,
  user quality, AIR, compiler design quality, and DAG P0 notes.
- `docs/100c_beta_dag_mir_abi_runtime.md` - module boundary, DAG closure, MIR
  declaration debt, ABI ownership/arena lifetime, parallel/core keyword tests,
  and pain-point loop.
- `docs/100d_beta_execution_log.md` - immediate execution order and historical
  progress log.

## P0 Source-Of-Truth Order

1. CFG/body safety source-of-truth: ownership, cleanup, drop, and zone/effect
   body facts must be consumed from CFG/MIR facts, not AST/helper fallbacks.
2. AIR abstraction-boundary verification: EvidenceNode and `pgy.air.graph.v1`
   must remain the stable verifier surface for boundary drift.
3. DAG recursive compatibility seam removal: semantic decisions must use the
   graph/materialized metadata path instead of recursive resolver compatibility.
4. MIR/LLVM declaration bootstrap parity: frozen subset declaration inventory
   must be DIR/RIR/MIR-owned rather than AST-carried metadata.
5. ABI/Slot/Pin ownership freeze: Slot/Pin/Zone-bound handle, raw escape, and
   runtime-none policy must be documented, smoked, and backend-stable.

## Editing Rule

Do not grow this index into another mega-checklist. Put details in the owning
shard and keep completed backend/declaration evidence in
`docs/133_beta_completed_closure_archive.md`.
