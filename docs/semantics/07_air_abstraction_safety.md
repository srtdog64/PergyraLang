# AIR Abstraction Safety

Last updated: 2026-04-26

Status: `beta-proof-obligation`

Stable surface: AIR (Abstraction Intent Representation) is a verification-only
synthesis IR for intent abstraction safety. Phase 1 covers `Intent Node`,
`Boundary Node`, `PGY_SEM_INTENT_BOUNDARY_DRIFT` for sync/async drift, and
default strict evidence diagnostics for missing RIR boundary/authority proof.

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

Every AIR boundary node in the stable intent subset must have matching RIR
boundary evidence, and authority-requiring boundaries must also have matching
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
  RIR authority evidence was found; `src/test_air.c` exercises direct synthesis
  and parsed-source teardown-safe boundary names.
- **Remaining obligation**: add a regression that snapshots full HIR/DIR/RIR
  structural hashes before and after synthesis once HIR/RIR cross-checks are
  wired.

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
  and known IO calls, then records HIR/RIR evidence flags per boundary. World
  boundaries require source-specific RIR `Move` / `Claim` transfer evidence
  instead of accepting a generic matching RIR intent scope. Default strict
  evidence mode turns missing RIR boundary/authority evidence into
  `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`. `src/test_air.c` covers direct
  AST-backed spawn and IO boundary synthesis, parsed-source IO boundary
  missing-evidence, direct world-boundary transfer evidence accept/reject, and
  parsed-source `where + transfer` zone/world boundary preservation.
  `tests/diagnostics_json_smoke.sh` covers parsed-source missing authority
  evidence and parsed-source missing IO boundary evidence through the full
  driver JSON path. Synthesis hard-fails if the precomputed boundary count and
  emitted boundary count diverge, blocking silent AIR boundary inventory drift.
- **Remaining obligation**: add parsed-source negative diagnostics for
  transfer/world boundary drift as those surfaces become representable without
  being rejected before AIR.

## Theorem: Strict Evidence Failure Soundness

If AIR reports `PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING`, then the synthesized
boundary lacks either RIR boundary evidence or required RIR authority evidence.

- **Reason**: abstraction safety cannot be beta-trusted if an intent boundary can
  pass without lowering-visible boundary or authority proof.
- **Evidence**: `src/compiler/air.c` checks each boundary's RIR evidence flags
  by default, and `src/test_air.c` covers strict-evidence missing-boundary,
  mismatched-authority, and world-boundary-without-transfer-op drifts.
  `src/compiler/driver_app.c` includes the expected authority participant list
  in `Reason:` when authority evidence is missing.
- **Remaining obligation**: expand parsed-source negative regressions for
  transfer/world boundaries that can reach AIR validation.

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

- `src/compiler/air.{h,c}` keeps independent AIR data structures and read-only
  synthesis.
- `src/test_air.c` covers pass and drift cases.
- AIR synthesis includes stable intent-step execution-boundary AST scanning for
  `spawn` / `async` / `parallel`, `channel` / `select`, and known IO calls.
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
