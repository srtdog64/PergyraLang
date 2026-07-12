# Architecture Review Check, 2026-07-12

Source review: external 2026-07-11 review of repository head `2259a622`.
Checked through current main `299091c6` plus the active worktree. This document
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
and memory-lifetime witnesses are present. Program assembly and binding
rewrites consume the builder owner; runtime builtin projection is a single
identifier scan whose C names still come from runtime ABI owners. Same-input
two-run sampling lowers codegen-only peak private memory from
3,347.3-3,394.5 MB to 956.1-956.5 MB and elapsed time from 22.690-23.205 s to
15.792-15.877 s relative to the preceding rung, while emitted C remains
byte-identical. Integrated-driver text lifetime remains active debt.
- Typed-arena cross-row validation remains mandatory at `AstTreeArtifactReady`,
  but hot accessors no longer recount every parallel row per field read. The
  same integrated-driver artifact stayed byte-identical and peak private memory
  fell from 223.4 MB to 159.4-161.0 MB. Runtime stayed effectively flat, so the
  result closes an ownership/validation-cost seam rather than the integrated
  CPU blocker.
- Shared parser/semantic source scanning now has an allocation-free byte/code
  owner. The converted trivia, parser-cursor, and semantic-text regions no
  longer materialize one-character or keyword-window Strings. Parser 188/188,
  semantic 110/110, and integrated-driver C/LLVM artifact parity are green;
  the same-input runtime moved from 37.915-38.071 seconds to
  36.891-37.131 seconds. Unmigrated text consumers remain active debt.
- Top-level expression operator discovery now has one semantic fact owner shared
  by expression typing and logical/binary validation. On the same integrated
  driver input, allocating `CharAt` calls fell from 2,851,682 to 1,128,849
  (60.4 percent). Runtime remained neutral at 37.002-37.273 seconds versus the
  preceding 36.891-37.131-second rung, so this is recorded as SoT and allocation
  surface closure, not as a CPU speedup. C- and LLVM-built drivers emitted the
  same 151,762-byte artifact with SHA-256
  `A7760C88DCAD10D7EEA87195800ABE642C506640AFAE4147E8A5A2DEEF12044F`.
- Qualified callable resolution now validates source-name byte ranges directly,
  replaces namespace separators in one runtime operation, and compares local
  suffixes without temporary segment Strings. This lowered integrated-driver
  `CharAt` calls again from 1,128,849 to 776,073 (31.2 percent for the rung,
  72.8 percent cumulative from the original 2,851,682). A same-window control
  ran in 40.686 seconds and candidates in 39.823-40.236 seconds, which is not
  enough evidence for a CPU-speed claim. The emitted artifact remained
  byte-identical.

## Remaining Work Order

1. Keep the blocked-send stress witness green across repeated Linux CI runs; if
   it fails, preserve the exact backend, iteration, timeout, stdout, and stderr.
2. Continue hard self-host replacement through typed owner facts and verifier
   parity. Do not replace missing MIR facts with source-text or AST recovery.
3. Continue repointing the remaining per-character expression/literal/statement
   owners and add scope reclamation for ordinary String temporaries. Prefer
   `CharCode`, `StrView`, `TextSpan`, or another explicit non-owning fact over
   changing `String` lifetime semantics to make `CharAtN` appear cheap. Do not
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
