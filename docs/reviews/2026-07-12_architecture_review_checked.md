# Architecture Review Check, 2026-07-12

Source review: external 2026-07-11 review of repository head `2259a622`.
Checked against current head `9f281db9` plus the active worktree. This document
records routing, not a production-readiness claim.

## Accepted Current Findings

- Self-hosting is a bounded DRV-2 MIR producer/replacement rung, not released
  default-path self-hosting.
- AST-attached checker facts are migration evidence. Executable backend facts
  must end in HIR/MIR-owned arenas before hard replacement can be claimed.
- Runtime behavior remains a release blocker. The intermittent LLVM blocked-send
  report is not closed by a single green run.
- Full call-ABI centralization, compiler text lifetime, runtime-bitcode
  optimization visibility, executor depth, and sandbox quotas remain open.
- C/LLVM equality alone cannot prove an LLVM optimizer attribute sound.

## Superseded Findings

- **with-slot early release:** closed after the reviewed head by `c9155225`.
  HIR `resource_scope_exit` facts select the MIR Release block after the loop
  body. `test-mir` owns the loop cleanup-order witness and passes 128/128 in the
  current worktree.
- **carriage implies pointer attributes:** already withdrawn. MIR carriage owns
  physical ABI only; `docs/143_evidence_parameter_attributes.md` and
  `mir-param-carriage-test-smoke` forbid deriving `noalias` or `readonly` from
  carriage.
- **V1 benchmark cardinality:** the review was correct about the defect, but it
  is now closed by `post_selfhost_validation_bug_classes.json`. Generated counts
  are N=10, S-containing=8, R-containing=4, S-only=6, R-only=2, S+R=2.
- **self-host 8.61 percent:** the ambiguous percentage is no longer canonical.
  `src/self_hosted/PROGRESS.md` separates implementation volume, bounded fixture
  coverage, executable-stage replacement, and released replacement; the live
  inventory is explicitly not a substitution percentage.
- **manual-only Coq:** Linux CI installs Coq and `formal-semantics-test-smoke`
  invokes `coqc` over the registered proof set. Local environments without Coq
  may skip, so this is Linux-CI reproducibility, not whole-language soundness.

## New Executable Evidence

- `post-selfhost-validation-manifest-test-smoke` recomputes claim cardinalities,
  validates diagnostic IDs and fixture ownership, and rejects generated-doc
  drift.
- `parallel-backpressure-stress-test-smoke` compiles each backend once and runs
  the blocked-send witness repeatedly with a per-run timeout. The current local
  result is C 64/64 and LLVM 64/64. Linux CI runs 32 iterations per backend;
  repeated CI evidence is required before closing the intermittent finding.
- TextBuilder rung 2 closes the first measured compiler text-owner slice. The language surface
  is move-only and fail-closed, each MIR call instruction carries a typed
  target-specific runtime-call ABI row, and C/LLVM consume that row. ABI/MIR
  mutation, C/LLVM differential, negative owner-exit, runtime-bitcode export,
and memory-lifetime witnesses are present. Program assembly and repeated token
rewrites now consume the owner. Same-input two-run sampling lowers peak
private memory from 3,347.3-3,394.5 MB to 1,989.5-1,990.4 MB while emitted C
  remains byte-identical. The remaining roughly 2.0 GiB is still active
  ordinary-String lifetime debt.

## Remaining Work Order

1. Keep the blocked-send stress witness green across repeated Linux CI runs; if
   it fails, preserve the exact backend, iteration, timeout, stdout, and stderr.
2. Continue hard self-host replacement through typed owner facts and verifier
   parity. Do not replace missing MIR facts with source-text or AST recovery.
3. Continue repointing the remaining per-character expression/literal/statement
   owners and add scope reclamation for ordinary String temporaries. Do not
   increase memory limits or broaden owner transfer without CFG/MIR evidence.
4. Move remaining executable capture/call-ABI facts into MIR-owned rows, with C,
   LLVM, and self-hosted consumers reading the same records.

## Integrated Bootstrap Refresh

The MIR-producing integrated driver now builds its full-source gen2 artifact.
Before the expensive full-source gen3 emit, the bootstrap runner consumes the
TestHarness-owned `mir_lower` source path and requires seed/gen2 emitted C to be
byte-identical. The current measured preflight completed in 34.5 seconds with
matching SHA-256 output. The full-source gen3 comparison was intentionally not
rerun after its cold gen2 phase took about 52 minutes, so the integrated
MIR-producing fixed point remains open. This is a CPU/algorithmic build-cost
blocker, not a memory-exhaustion result; working/private memory stayed near
1.1-1.2 GB during that run.

Current rule:

```text
No behavioral claim without a witness.
No optimizer promise without an independent proof fact.
No hard self-host replacement without C/LLVM/self-host artifact parity.
```
