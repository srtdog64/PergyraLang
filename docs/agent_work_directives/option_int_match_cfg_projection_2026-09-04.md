# Option<Int> match CFG projection lease — 2026-09-04

Status: `IMPLEMENTATION COMPLETE — EXACT CI PENDING`

Exact base: `f6837b36157f0a97890831b6734083164e323ade` on `origin/main`.
Executable candidate: `0a8b186c545d9f237c40393114bc5b2d408b6067`.

This is a temporary coordination contract. It owns no language semantics,
compiler progress, SoT status, ABI status, or successor rung.

## Objective card

- Objective: replace the installed DRV-2 source-to-LLVM rejection of the
  canonical Option<Int> match CFG with a Pergyra-owned condition and target
  projection that preserves exactly-once scrutinee evaluation.
- Priority: match-binding identity, admitted Option ABI, unique CFG scope,
  missing/mutated-fact rejection, existing match-family parity, then physical
  owner size.
- Fact owners: `MirMatchInstructionCapture` owns source pattern/binding facts;
  `MirRoutineFactIndex` carries the declared-source-local prefix;
  `DirectMirScalarCfgLocalRefPlan` carries instruction-scoped binding identity;
  `DirectMirOptionMatchAbiFact` owns Option layout.
- Last legitimate consumer:
  `DirectMirScalarCfgProgramAppendTypedExpressionFieldWithOptionIntMatch`
  appends one normalized condition before target match-family dispatch.
- Forbidden fallback: source/MIR text recovery, a second type or local table,
  backend-local tag/field inference, native retry, C round-trip, fixture-name
  routing, or merging same-spelled bindings across distinct CFG scopes.
- Verification gate:
  `tests/self_hosted/parity/match_scrutinee_single_evaluation_owner.sh` must run
  installed C and LLVM with the exact four-line oracle, then reject binding-type
  and Option-tag mutations for both targets without an artifact.

## Independent edit scopes and forbidden overlap

- The primary implementation scope is the scalar CFG expression/local owners,
  Option<Int> ABI consumers, C/LLVM match dispatch, the focused parity gate,
  owner inventory/caps, and component contract.
- Documentation scope is this directive, current collaboration/handoff, and
  the top current-progress summary. It must not edit semantic registries or
  claim a census/percentage change.
- No delegated implementation scope is active. Read-only review may report an
  observation under `docs/audits/`, but it may not edit or publish this rung.
- Protected user paths listed in the handoff remain outside inspection, edit,
  staging, and cleanup.

## Commands and validation budget

- Static owner/cap/syntax checks should finish in 60 seconds when isolated.
- Focused installed C/LLVM parity and its two mutations should finish in five
  minutes. Existing Option<Int> and payload-enum match regressions share that
  budget.
- Complete component inventory may exceed five minutes because it includes its
  own execution action; observe the actual duration rather than hiding it.
- Run official bounded driver bootstrap and installed DRV-2 build once after
  the focused slice is stable. Full fixed point belongs to exact-head CI.

## Integration state

- Integration owner: the primary task on `main`.
- One integration gate: exact-head GitHub Actions matrix after selective push.
- Local observations: focused installed C/LLVM parity, both owned mutations,
  existing Option<Int>/payload-enum regressions, complete component contract,
  bounded driver bootstrap, and installed driver build are green. The existing
  bootstrap unreachable-statement warning remains explicit.
- Output classification: `0a8b186c` is an implementation candidate backed by
  local executable evidence. It becomes published closure only after exact CI;
  this directive alone cannot mark the rung or a SoT row closed.
