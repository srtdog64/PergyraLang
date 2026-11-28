# 2026-11-29: `air.evidence_graph` closes by scope

Decided by: the user, on 2026-11-29 ("다음 SoT 2개 더 닫아", closing by
scope decisions and choosing the better long-term option without asking);
recorded by the Claude lane. The user can overturn it.

## Rule

1. AIR (`air_synthesize`) is the one verification owner. It reads the MIR
   routine inventory in one adapter pass, and binds MIR evidence once
   through an AIR-owned anchor with a deterministic input fingerprint. A
   rebind is refused. The row's forbidden fallbacks name only MIR inventory
   rescans and rebinds, counters as proof, missing-evidence success and
   backends reading AIR, and none of them remains.
2. AIR reads HIR, RIR and DIR evidence during synthesis and verification,
   while those programs are alive. `driver_app.c` synthesizes and verifies
   AIR and hands its verification to the C and LLVM projections inside one
   compilation. It destroys AIR before MIR, RIR and HIR. No AIR consumer
   outlives the stage programs it read.
3. Copying HIR, RIR and DAG evidence into AIR-owned rows with their own
   anchors, the "cross-stage lifetime migration" the row listed, has no
   consumer that needs evidence beyond one compilation. The self-host
   compiler has no AIR input. After V1 the native pipeline, which owns AIR,
   is a frozen oracle
   ([decision](2026-11-24_native_pipeline_becomes_frozen_oracle_after_v1.md)).
   The migration is therefore not part of this row.

## Reopens when

- AIR evidence or certificates must outlive a compilation (cached,
  serialized for reuse or compared across runs); or
- an AIR consumer is found reading HIR, RIR or DIR data after its owner is
  destroyed.

Replaces: nothing.
