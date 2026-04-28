# AIR Abstraction Safety

Last updated: 2026-04-29

Status: `beta-proof-obligation`

Stable surface: AIR (Abstraction Intent Representation) is a verification-only
synthesis IR for intent abstraction safety. Phase 1 covers `Intent Node`,
`Boundary Node`, `PGY_SEM_INTENT_BOUNDARY_DRIFT` for sync/async drift, and
default strict evidence diagnostics for missing HIR CFG, RIR boundary, and RIR
authority proof.

Out of scope for beta: AIR is not a codegen IR, not the ownership/borrow checker
home, not the type checker home, not the effect propagation engine, and not a
source of new keyword or syntax. Phase 2 `Constraint Node` / `Effect Node`
expansion and Phase 3 metadata source-of-truth migration are post-Phase-1 work.

## Judgments

`DIR -> step -> intent_node`

A DIR intent step maps to exactly one AIR intent node preserving owner, step
index, sync class, failure class, and compensation hook when present.

`DIR, HIR, RIR -> boundary -> boundary_node`

Existing compiler IR metadata maps implementation boundaries to AIR boundary
nodes. Phase 1 starts with DIR intent step boundary metadata; beta completion
requires HIR routine boundary and RIR authority fact cross-check coverage.

`intent_step_ast -> execution_boundary -> boundary_node`

Intent step execution clauses (`using`, `intent`, `pre`, `guard`, `post`,
`invariant`, `expect`, `on`, `compensate`) are scanned for stable execution
boundaries. `spawn`, `async`, and `parallel` are async parallel boundaries;
`channel` operations and `select` are async channel boundaries; known file/line
IO calls are either-sync IO boundaries. These are AIR `Boundary Node`s, not
ad-hoc diagnostic helpers.

`AIR -> no_drift`

Every intent node and the boundary nodes reachable from that intent agree on
the frozen abstraction contract being checked. Phase 1 checks sync/async
compatibility.

`AIR evidence_complete`

Every AIR boundary node in the stable intent subset must satisfy the evidence
policy for its boundary class. Zone and world boundaries require RIR boundary
evidence. Parallel, channel, IO, and execution implementation boundaries require
HIR CFG evidence; parallel, channel, and IO additionally require source-specific
RIR boundary evidence. Authority-requiring boundaries must also have matching
RIR authority evidence. Missing evidence is reported as
`PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`; it is not silently accepted as no
drift. `PGY_AIR_STRICT_EVIDENCE=0` is a development/debug opt-out, not the beta
default.

Authority evidence is participant-sensitive. If an AIR boundary is derived from
`authorized by: X`, matching RIR authority evidence must name `X`; unrelated
authority facts or authorize ops in the same scope do not discharge the proof
obligation.

World-handoff evidence is operation-sensitive. If an AIR `World` boundary is
derived from `transfer: from -> to`, matching the enclosing RIR intent/world
scope is insufficient by itself; AIR requires RIR `Move` or `Claim` evidence for
the boundary source alias.

`AIR well_formed`

The AIR graph must be structurally valid before drift facts are meaningful.
Intent owner/step names and boundary owner/source names are non-empty, each
boundary references an existing intent, each boundary owner equals the
referenced intent owner, each boundary step index equals the referenced intent
step index, and boundary sync shape is frozen: world, parallel, and channel
boundaries are async; IO boundaries are either-sync; zone boundaries may reflect
the enclosing step's sync class. Existing drift inventory is also checked before
recomputation: a drift node must have a concrete drift kind, valid
intent/boundary references, and a non-empty message. Violations are
`PGY_AIR_INVARIANT_INVALID` compiler IR failures, not user-correctable semantic
drift.

## Theorem: AIR Synthesis Read-Only

If `air_synthesize(HIR, DIR, RIR)` succeeds, it does not mutate the input HIR,
DIR, or RIR programs.

- **Reason**: AIR is a verification-only synthesis IR beside the codegen path.
  Mutating source IRs would make AIR stale-state bugs observable in C / LLVM
  output.
- **Evidence**: `src/compiler/air.c` reads DIR metadata plus HIR routine
  evidence and RIR boundary/authority evidence, then allocates an independent
  `AIRProgram`. Synthesized intent, boundary, and authority names are AIR-owned,
  so parsed-source AIR does not borrow DIR/AST string lifetime after parser
  teardown. Each `Boundary Node` records whether HIR routine, RIR boundary, and
  RIR authority evidence was found, plus AIR-owned provenance names for the
  matching HIR routine, RIR scope, and RIR authority participant; `src/test_air.c`
  exercises direct synthesis and parsed-source teardown-safe boundary names. The
  HIR/DIR/RIR evidence collection regression snapshots representative DIR step
  metadata, HIR routine metadata, and RIR scope/op/fact metadata before and after
  synthesis to keep AIR read-only at the owner/evidence seam.
- **Remaining obligation**: expand this from representative field snapshots to
  full structural hashes once the IRs expose stable hash helpers.

## Theorem: Intent Node Coverage

Every stable beta intent step is represented by exactly one AIR `Intent Node`.

- **Reason**: abstraction drift diagnostics must not silently miss an intent
  step.
- **Evidence**: `air_synthesize` counts DIR intent steps and emits one AIR
  intent node per step. `src/test_air.c` includes parser-source integration
  tests that lower real intent declarations through semantic analysis into
  DIR/HIR/RIR/AIR. Synthesis also hard-fails if the precomputed intent count
  differs from the emitted intent count, so the inventory cannot silently drift.
- **Remaining obligation**: extend parser-source coverage as new stable intent
  clauses become semantically representable instead of pre-AIR rejected.

## Theorem: Boundary Closure

Every implementation boundary relevant to a stable beta intent step is
represented by at least one AIR `Boundary Node`.

- **Reason**: drift detection is only sound if it sees the boundary that can
  violate the declared abstraction contract.
- **Evidence**: Phase 1 maps DIR `where` and transfer metadata to separate
  zone/world boundary nodes when both are present, recursively scans intent-step
  execution clauses for `spawn` / `async` / `parallel`, `channel` / `select`,
  and known IO calls, then records HIR/RIR evidence flags and provenance names
  per boundary. Implementation boundaries (`spawn` / `async` / `parallel`,
  `channel` / `select`, IO calls, `with` / `unsafe` / `defer`) must have HIR CFG
  evidence, so RIR evidence alone cannot discharge a body-boundary proof.
  World boundaries require source-specific RIR `Move` / `Claim` transfer
  evidence instead of accepting a generic matching RIR intent scope. Default
  strict evidence mode turns missing HIR CFG, RIR boundary, or RIR authority
  evidence into `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`. `src/test_air.c` covers direct
  AST-backed spawn and IO boundary synthesis, parsed-source IO boundary
  missing-evidence, direct world-boundary transfer evidence accept/reject, and
  parsed-source `where + transfer` zone/world boundary preservation with RIR
  evidence provenance on both emitted boundaries.
  `tests/diagnostics_json_smoke.sh` covers parsed-source missing authority
  evidence and parsed-source missing IO boundary evidence through the full
  driver JSON path. Parsed-source `transfer` coverage now includes a negative
  authority path on the transferred step: AIR must keep both the zone and world
  boundaries, observe RIR transfer evidence for the world boundary, and still
  reject the zone boundary when the required authority participant has no RIR
  authority proof. Synthesis hard-fails if the precomputed boundary count and
  emitted boundary count diverge, blocking silent AIR boundary inventory drift.
  Expression-derived boundaries keep their expression span when available and
  fall back to the enclosing intent-step span when parser expression nodes have
  no location, so boundary diagnostics do not lose source provenance. `air_dump`
  prints the same per-boundary evidence provenance names, and the AIR unit suite
  gates that debug surface so it cannot regress to boolean-only evidence output.
- **Well-formedness gate**: `src/compiler/air_verify.c` rejects empty intent or
  boundary names, boundary-owner mismatch, step-index mismatch, and invalid
  boundary sync-shape before drift computation. It also rejects invalid stale
  drift inventory before recomputing. `src/test_air.c` carries direct negative
  tests for owner mismatch, world-boundary sync-shape mismatch, and invalid
  drift inventory.
- **Remaining obligation**: extend this parsed-source negative coverage to
  sync/async transfer drift once HIR/RIR can express that mismatch without
  earlier semantic rejection.

## Theorem: Strict Evidence Failure Soundness

If AIR reports `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`, then the synthesized
boundary lacks required HIR CFG evidence, required RIR boundary evidence, or
required RIR authority evidence.

- **Reason**: abstraction safety cannot be beta-trusted if an intent boundary can
  pass without lowering-visible boundary or authority proof.
- **Evidence**: `src/compiler/air_verify.c` checks each boundary's HIR/RIR
  evidence policy by default, and `src/test_air.c` covers strict-evidence
  missing-boundary, implementation-boundary-without-HIR-CFG-evidence,
  mismatched-authority, world-boundary-without-transfer-op drifts, and
  parsed-source transfer with zone missing-authority evidence while preserving
  world transfer evidence.
  `src/compiler/driver_app.c` includes the expected authority participant list
  in `Reason:` when authority evidence is missing, and includes the AIR evidence
  provenance summary (`hir`, `rir_boundary`, `rir_authority`) so missing
  boundary/authority proof is visible in both text and JSON diagnostics.
- **Remaining obligation**: expand parsed-source negative regressions for
  transfer/world sync-class drift once that mismatch can reach AIR validation.

## Theorem: Drift Detection Soundness

If AIR reports `PGY_SEM_INTENT_BOUNDARY_DRIFT`, then the intent step contract
and its implementation boundary disagree on the checked abstraction dimension.

- **Reason**: false drift reports would force users to rewrite correct intent
  code and weaken trust in abstraction diagnostics.
- **Evidence**: `src/test_air.c` covers sync intent + async boundary as a
  positive drift, async intent + async boundary as a non-drift, direct
  AST-backed `spawn` boundary drift, IO `either` boundary non-drift, parsed
  source no-drift lowering, and parsed IO boundary missing-evidence.
  `src/compiler/driver_app.c` reports the first drift with
  `PGY_SEM_INTENT_BOUNDARY_DRIFT`, `Reason:`, `Fix:`,
  `PGY_CAUSE_INTENT_BOUNDARY_DRIFT`, and
  `PGY_FIX_ALIGN_INTENT_BOUNDARY_SYNC`.
- **Remaining obligation**: extend source-backed integration tests once HIR/RIR
  boundary synthesis can create real sync/async drift from parsed programs
  instead of direct AIR construction.

## Theorem: Codegen Non-Impact

AIR validation does not directly change C or LLVM output.

- **Reason**: AIR is a semantic/compiler validation layer, not a backend
  lowering layer. Backend parity must stay tied to MIR and runtime ABI.
- **Evidence**: AIR is not part of `CompilerIRBundle`; C and LLVM backend source
  files receive only HIR/DIR/RIR/MIR. `make air-drift-test-smoke` checks that
  the backend bundle does not carry AIR. `make air-backend-nonimpact-test-smoke`
  compares generated C and LLVM text with relaxed AIR and default strict AIR for
  the core AIR-related backend fixture set. `make
  air-backend-nonimpact-full-test-smoke` extends that generated-text check to
  the full frozen backend-compare fixture set. `make
  air-strict-backend-compare-test-smoke` runs C/LLVM execution parity under
  strict evidence.
- **Remaining obligation**: add Windows native evidence for the full AIR backend
  non-impact gate.

## Acceptance Rule

AIR Phase 1 is beta-complete only when all of these are true:

- `src/compiler/air.h` keeps independent AIR data structures,
  `src/compiler/air.c` keeps read-only synthesis, and
  `src/compiler/air_verify.c` keeps global validation/drift ownership.
- `air_verify(...)` is the global AIR validation entry point. It validates AIR
  inventory invariants, evidence provenance invariants, and drift/evidence
  failures before MIR lowering. `air_check_drift(...)` is only a compatibility
  wrapper over `air_verify(...)`.
- AIR inventory validation rejects non-zero intent/boundary/drift counts without
  backing arrays and rejects a boundary whose `step_index` no longer matches
  the referenced intent node. These are validation failures, not drift facts.
- RIR authority evidence is accepted only when the same boundary already has RIR
  boundary evidence and is authority-required. Boolean-only authority evidence
  on an unrelated boundary is rejected as invalid AIR inventory.
- Invalid AIR inventory is diagnosed with `PGY_AIR_INVARIANT_INVALID`, not with
  semantic intent drift codes. This keeps compiler IR contract failures separate
  from user-correctable abstraction mismatch.
- `src/test_air.c` covers pass and drift cases.
- AIR synthesis includes stable intent-step execution-boundary AST scanning for
  `spawn` / `async` / `parallel`, `channel` / `select`, and known IO calls.
  `src/test_air.c` covers the stable execution boundary set directly, not only
  IO and spawn.
- `PGY_SEM_INTENT_BOUNDARY_DRIFT` is registered and documented.
- `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING` is registered, documented, and
  tested under strict evidence mode.
- Driver semantic-validation diagnostics surface AIR drift with source span,
  `Reason:`, `Fix:`, cause_ir, and fix_source. Authority-evidence failures must
  also show the expected authority participant list.
- HIR + DIR + RIR synthesis coverage prevents boundary blind spots for the
  stable intent subset.
- C / LLVM backend non-impact smoke confirms AIR is not a hidden codegen
  dependency.
