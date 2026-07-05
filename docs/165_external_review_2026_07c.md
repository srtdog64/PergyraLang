# 165. External Review #4 Judgment - Intent/Evidence Compiler Thesis

Status: `judged, actions-routed`. Date: 2026-07-06.

This note records the latest external review observation set. It is not a
release verdict and not a new feature board. It separates accepted signals from
claims that still need repository gates or official-source revalidation.

## Accepted Signals

- Pergyra is not a game DSL. The accurate identity is an
  intent/evidence-based compiled language for safe interactive systems.
- AIR remains verification-only. C and LLVM must consume MIR/ABI facts, not AIR
  as a second codegen truth.
- SEA's remaining P0 is no longer facade routing. The frontier is precise
  `BoundaryCaptureFact` production: per-boundary value/raw/pin/movability
  evidence owned by MIR/RIR, never source-kind or boundary-kind guessing.
- The self-host story must keep M1 and M2 separate. Codegen fixed-point is a
  valid M1 achievement; whole-compiler self-eating bootstrap remains M2 and is
  not complete.
- Runtime materialization is acceptable only when evidence requires it. Hidden
  materialization is the bug; explicit retain/summarize/erase/reject evidence is
  the contract.
- The performance claim should be evidence-amortized cost, not zero-cost. Proven
  evidence may become a backend assumption; heuristic evidence must retain a
  guard.

## Deferred Or Unverified Signals

- Third-party version/date claims for WASI/WIT/Wasmtime or papers are not
  accepted as current facts until checked against official sources in the turn
  where they are used.
- Native WASM/NPU/GPU/dataflow projection remains a long-term projection
  contract. It is not a beta capability until host ABI, quota, and backend
  parity gates exist.
- Executor implementation depth is intentionally below the language contract.
  `ExecutionLaneFact` is semantic; worker pool, blocking pool, fiber, or
  work-stealing executors are replaceable implementations under that fact.

## Routed Actions

| Review action | Repository owner |
|---|---|
| Precise value-capture coverage | `docs/146_sea_execution_lanes.md`, `src/compiler/air_evidence_mir.c`, future closure-capture producer |
| AIR JSON lane matrix must use real source rows | `tests/sea_execution_lane_golden_smoke.sh` |
| Self-host M1/M2 split | `docs/160_m2_completeness_execution_plan.md`, `docs/self_hosted/` |
| Guard amortization framing | `docs/142_evidence_guard_amortization.md` |
| Materialization visibility | AIR retain/summarize/erase/reject facts and future relation export |
| Sandbox/frame budget | sandbox board and future host-call quota fixtures |

## Immediate Closure From This Review

- `sea-execution-lane-golden-test-smoke` now refuses synthetic AIR-style
  coverage by requiring the public AIR graph schema, HIR/RIR/MIR input flags,
  and source locations in the emitted `--air-json`.
- `docs/146` now records the current 31/31 self-host SEA parity rows and the
  rule that full AIR JSON lane matrix rows may expand only from valid source
  fixtures.

## Red-Team Rule

Do not let a review signal become a marketing sentence before it has one of:

1. a source fixture,
2. a generated artifact,
3. a parity check,
4. a fail-closed negative case,
5. or an explicit deferred status with an owner.

That is the line between a Pergyra thesis and a Pergyra claim.
