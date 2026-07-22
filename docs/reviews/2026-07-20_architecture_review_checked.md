# Architecture Review Check, 2026-07-20

Source review: the attached `PergyraLang Architecture Review — 2026-07-20`,
which observed commit `f25625f5`. Checked against `main` at
`fb8d2c22` on 2026-07-21. The checked head is 41 commits newer than the
reviewed head. The working tree also contains uncommitted MIR-only/backend
work; closure claims below are based on the committed head and named gates,
not on those uncommitted changes.

This is a claim audit and routing document, not a production-readiness
declaration.

## Objective Card

- Objective: prevent stale review findings from reopening closed owners while
  routing the remaining compiler-scale, projection, concurrency, and sandbox
  blockers to executable gates.
- Priority: semantic identity and one SoT, owner-directed facts,
  missing-fact failure, executable substitution, then breadth and tuning.
- Fact owners: the existing SoT registry, MIR/AIR owners, the verified
  projection plan, runtime context owners, and the build-pressure measurement
  owner. This document is not an additional authority.
- Last consumers: C, LLVM, self-hosted projections, runtime facades, and the
  CI/bootstrap harness named by each row.
- Forbidden fallback: backend source/AST reclassification, process-global
  state presented as per-content isolation, bounded consumer evidence claimed
  as full producer substitution, or elapsed time used without peak-live data.
- Gates: execution-lane policy/golden, build-pressure contract, SoT authority
  edge, MIR/AIR fail-closed, projection-plan, runtime-cext, and the relevant
  self-host parity gates.

## Findings that are superseded or narrowed

### Runtime split-state repair: accepted, but process scope remains

The review's immediate capability, budget, and LLVM cancellation split-state
findings are not current blockers at this checked head. The runtime ownership
work and its cext/behavioral gates establish one native owner for the covered
state families, and the capability/budget slice now selects that state through
an explicitly bound `PgyRuntimeContext` rather than a process-global grant.

This does **not** establish mutually untrusted content isolation. Capability,
budget, cancellation, scheduler, and handle state are still process-scoped for
the trusted native profile. The review's distinction between singleton
correctness and per-instance sandboxing remains valid.

### Execution lane reclassification: superseded at this head

The source review reported that codegen still used
`ast_spawn_is_blocking`. That finding is no longer current:

- `docs/146_sea_execution_lanes.md` records the codegen-consumption landing;
- C and LLVM receive the verified spawn-lane plan from AIR;
- a current source scan finds `ast_spawn_is_blocking` only in the AIR boundary
  evidence producer, not in codegen emitters;
- `bash tests/execution_lane_policy_smoke.sh` passed the native 12/12 policy
  rows and the 13/13 AIR evidence rows on this Windows checkout.

This is a repaired SoT seam, not evidence that every runtime executor is
production-grade. The lane facade still documents worker-join scaffold depth
for several lanes.

### Build-pressure wiring: accepted as measurement infrastructure, not closure

`build-pressure-contract-smoke` passes and the repository has phase samples,
JSON summaries, and a 3 GiB fail-closed ceiling. That is useful evidence, but
it is not the peak-live-object model proposed by the review. The current
measurement owner observes process/phase memory; it does not yet attribute
created/retained bytes, cross-stage copies, string payloads, and release points
to every compiler store.

## Findings that remain current

### 1. Compiler-scale self-host cost is the dominant blocker

The existing measurements and self-host completion log still show a large
textual/codegen amplification surface. The next real closure is not a string
micro-optimization. It is a stage-lifetime and identity-carriage owner that
can report:

```text
createdBytes, retainedBytes, peakBytes, lastConsumer, releasePoint,
crossStageCopies, stringPayloadBytes, identityPayloadBytes
```

Stable `SymbolId`, `TypeId`, `SyntaxNodeId`, `RoutineId`, and runtime-call
identities should be carried as handles wherever the current owner permits.
The build-pressure gate remains the outer budget. The common compiler arena now
has a first executable ledger owner (`PgyArenaLedger`) with created/retained/
peak bytes, allocation count, string payload bytes, explicit cross-stage and
identity-copy counters, owner, last consumer, and release point. Lexer, HIR,
semantic, C, and LLVM arena owners are named, and
`tests/arena_ledger_smoke.sh` verifies the accounting and annotations.

This is a first stage-lifetime rung, not proof that every compiler allocation
is arena-backed. Heap-backed semantic tables and generated artifact buffers
remain outside this ledger and keep the per-store peak-live item open.

### 2. The native ownership spine is still bridged

The current registry reports `CLOSED=22 BRIDGE=19 ACTIVE=0`. The count differs
from the attached review because the registry denominator changed after the
reviewed head; it is not a percentage improvement claim.

The following remain bridged in the current registry:

- parser provenance and the symbol/type graph;
- `ResourceFlowUniverse`, loop-flow, and function-parameter summaries;
- HIR/DIR/RIR to the complete MIR execution graph;
- generic specialization identity convergence;
- AIR evidence lifetime beyond the bounded anchor;
- aggregate/runtime ABI rows and target profile breadth;
- complete verified projection-plan legalization.

`semantic.destructure_binding_type` is currently `CLOSED`, but that local
closure does not promote the native spine as a whole.

### 3. Verified projection is still partial

`VerifiedProjectionPlan` remains the sole executable disposition owner, which
is the correct architecture. Its current native row family is still primarily
intent observability plus target/machine binding. Layout, cleanup, runtime
checks, capability retention, composed loss, artifact residue, and full
self-host rows are not all legalized.

The first bounded MIR-owned row family has now landed for parallel-capture
dispositions. `PgyVerifiedParallelCapturePlan` is AIR-bound, digest-checked,
and consumed by the C/LLVM async and parallel-join emitters; snapshot-copy is
`materialize`, join index/read-only is `retain`, and unknown facts are
`reject`. `parallel-capture-projection-test-smoke` prevents direct MIR-table
reads from returning. This narrows the finding but does not close projection
as a whole: layout, cleanup, runtime checks, capability retention, composed
loss, artifact residue, and full self-host rows remain open. Do not broaden the
enum or add a placeholder row without a real consumer migration.

### 4. Full bootstrap is wired, not proven green here

The newer commits require the Pergyra-built self driver and preserve the DRV-2
fixed-point path in the CI/build contract. That proves the job is load-bearing,
not that the latest remote workflow completed successfully. In this Windows
session the self-host execution-lane parity probe skipped because the available
compiler binary was not runnable; this is incomplete evidence, not a green
result or a blocker classification.

The correct claim remains: bounded/self-host rungs are executable evidence;
released default-driver substitution and full bootstrap cost closure remain
open.

### 5. Per-content sandbox has a capability/budget context rung; full isolation remains open

The review's `ContentInstance` boundary is still a valid P0. The new
`PgyRuntimeContext` owner binds capability masks and quantitative budgets per
content instance through TLS, and `runtime-context-test-smoke` proves two
contexts do not share either manifest or counter state. A process-wide
singleton runtime is therefore no longer the authority for those two covered
families.

This does not yet serve mutually untrusted games, documents, or interactive
scenes as a complete sandbox. The remaining context obligations are
cancellation root, scheduler, handles, asset namespace, linear memory,
fuel/deadline, random stream, and diagnostics; each needs explicit carriage and
cross-instance negative tests.

Wasm/component interfaces or Capsicum-like host compartments may implement the
outer boundary, but they do not replace Pergyra's static capability and AIR
admission facts. No sandbox claim should be promoted until cross-instance
negative tests exist.

### 6. Sparse/incremental analysis remains a research-backed work order

The existing dense summaries are useful oracle evidence. A sparse solver is not
presently a replacement owner. The safe sequence is:

```text
dense oracle -> symbol-specific sparse solver -> denseResult == sparseResult
               -> recursion/loop/channel/ownership differential corpus
               -> production replacement
```

Do not remove the dense path or claim incremental invalidation until the
differential corpus and revision invalidation gate exist.

### 7. Bootstrap provenance remains deferred; cache identity has a first rung

Diverse double compilation is a release-campaign control, not a per-commit
gate. The runtime cache now has an executable v2 identity owner in
`src/compiler/compiler_runtime_cache.c`: its path digest includes the cache
schema, ABI version, linkage family, profile, observability mode, compiler
revision, target identity, toolchain selector, sanitizer mode, and the thread
flag. `tests/runtime_cache_identity_smoke.sh` proves that two supplied
compiler revisions cannot reuse the same runtime object path, and the C/LLVM
link consumers receive the keyed path rather than the legacy profile/mtime-only
name.

This is intentionally a first rung, not release closure. A build that does not
provide `PGY_COMPILER_REVISION` remains keyed as `unversioned`; the release
driver must supply a stable compiler revision and still needs a runtime-source
digest plus the full normalized flag/toolchain identity before cache hits can
be accepted as a bootstrap correctness boundary. The negative rule is now
explicit: legacy cache paths are not considered fresh by the v2 owner.

## Action routed by this check

No code patch was made for the stale lane finding because that owner is already
closed at the checked head. The active implementation order is instead:

1. Extend the new `PgyArenaLedger` from the named arena owners to the remaining
   compiler stores, beginning with the integrated self-host producer/consumer
   boundary, and keep the existing build-pressure budget as the blocking cap.
2. Promote one real MIR-owned `VerifiedProjectionPlan` row family with a
   missing-fact negative gate.
3. Extend `PgyRuntimeContext` to cancellation/scheduler/handle/asset owners as a
   separate runtime workstream; do not mix it into the compiler MIR-only
   substitution rung.
4. Add the sparse-vs-dense differential solver only after stable identity and
   revision invalidation owners are explicit.
5. Run diverse double compilation for release/bootstrap-chain changes, not as
   evidence for ordinary local parity.

The cache identity rung is now between items 4 and 5: it is an executable
partial closure with a supplied-revision requirement, while the release
provenance campaign remains open.

Current rule:

```text
A repaired lane owner is not reopened by a stale review.
A process-wide singleton is not a content sandbox.
A build-pressure cap is not a peak-live ownership ledger.
A verified projection row is not full IR legalization.
A configured CI job is not a completed status check.
```
