# Native HIR Malformed-Enum Progress Fuzz Audit

Status: READ-ONLY FINDING — REPRODUCED, NOT REPAIRED

Latest observed revision: `1b6954cd086e85240f83f68e82ce92c3424c6296`

This audit records delegated fuzz evidence from `F:\tex_bug`. It is not a
semantic owner, SoT registry entry, progress increment, active implementation
lease, or completion claim.

## Original finding

- The one-minimal reproducer is the exact six-byte input `enum({` with no
  trailing newline.
- `pgy SOURCE --native-pipeline --hir` exceeded both a 1-second timeout
  (observed wall time about 1.057s) and a 3-second timeout (about 3.249s).
  This was first observed through native HIR, but the later stage-localization
  campaign below proves that the hang begins in native module loading before
  AST production. It is not a HIR-lowering hang or a crash receipt.
- The same input terminates through the sampled neighboring phases: tokens
  exits 0 in about 52ms, AST exits 1 in about 49ms, public MIR exits 1 in about
  79ms, and C emission exits 1 in about 43ms.
- Valid control `examples/enum_test.pgy` completes in all sampled phases in
  approximately 27-185ms.

## Additional campaign evidence

- The corrected campaign used seed `20260902`, 80 cases, 60 self-contained
  admitted seeds, two import-context exclusions, and two executions per case
  for 160 executions.
- It found no additional crash, hang, internal diagnostic, or determinism
  mismatch. The unique finding count remains one.
- The observed compiler executable SHA-256 was
  `464C55FAF5F2489F9293E678BF42377A640A6321A14C6A67BE157810FC7972F1`.

### Independent follow-up campaign

- A second delegated read-only campaign began at D revision
  `eace7842fb9549b678140c28335e5a5d3dafd54f` and ended after the concurrent
  documentation publication moved HEAD to
  `a7e99d5c2eced2a16b5d2cd3095296ae87401781`. The compiler executable hash
  remained unchanged. The requested `F:\tex\_bug` path did not exist, so the
  observed repository was the existing `F:\tex_bug` path.
- From 100 corpus inputs, 65 self-contained native-HIR-green seeds were
  admitted; 8 import-context inputs and 27 baseline rejects were excluded.
  Mutation operators were run with seeds `20260902`, `20260903`, and `424242`
  for 120 cases total.
- Every case ran twice through tokens, AST, public MIR, C, LLVM, and native HIR
  with a one-second timeout: 1,440 mode invocations in 97.22 seconds. The
  campaign found zero new crash, hang, internal diagnostic, or determinism
  mismatch and left no OS temporary residue.
- The exact `enum({` canary remained the sole finding: tokens, AST, MIR, C, and
  LLVM terminated in 42-196ms, while native HIR timed out at approximately
  1.071s and 3.173s under the one- and three-second checks. This is duplicate
  evidence, not a second finding or an implementation authorization.

### Native module-load boundary campaign

- A third delegated read-only campaign used the two `F:\tex_bug` enum-bearing
  corpus seeds `pergyra-0093-examples_enum_test.pgy` and
  `pergyra-0024-examples_bsd_packet_server_main.pgy`. It deterministically
  generated 222 adjacent brace, parenthesis, and bracket mutations; no RNG
  seed was used.
- Each case ran twice through tokens, public AST, and explicit native AST for
  exactly 1,332 core mode invocations. Short classification timeouts were
  0.35 seconds for native AST and 0.75 seconds for public AST. Separate
  minimization, one-/three-second confirmations, and stage-localization calls
  are not included in that count. The campaign took 327.59 seconds.
- Tokens exited 0 for all 222 cases. Public AST rejected all 222 explicitly
  with exit 1. Explicit `--native-pipeline --ast` rejected 83 with exit 1 and
  timed out on 139; every timeout classification repeated identically. There
  were no crashes, internal diagnostics, or determinism mismatches.
- The 139 timeouts reduce to five deletion-minimum spellings: the existing
  `enum({` family (45 cases) and new `enum)` (58), `enum[` (18), `enum]` (16),
  and `enum{{` (2) families. Each new minimum timed out under both one- and
  three-second confirmation. These are distinct minimal spellings, not proof
  of five independent root causes.
- The first hanging mode is explicit `--native-pipeline --ast`; native RIR,
  AIR, HIR, CFG, DOM, and SSA requests inherit the same hang. The last debug
  stage is `[driver stage] module_load`, so the owned defect boundary is native
  module-load/parser progress before AST creation. Public AST, DIR, MIR, C,
  and LLVM all reject the same malformed inputs promptly.
- The observed `bin\pgy.exe` SHA-256 remained
  `464C55FAF5F2489F9293E678BF42377A640A6321A14C6A67BE157810FC7972F1`.
  Neither repository was modified, and the empty OS temporary directory left
  by the interrupted preliminary sweep was verified and removed by the
  integration owner.

## Harness limitations observed

- The corpus runner contains a stale hard-coded `E:\` path and an obsolete
  fail-closed `pgy SOURCE --hir` invocation.
- Windows text handling can fail on U+FFFD under CP949 unless Python runs with
  UTF-8 mode.
- Second-run return/output values are collected but not adjudicated, and the
  harness has no finding deduplication.
- Imported-local corpus context is absent or flattened, and fixed `/tmp/fz`
  C/LLVM outputs can collide.

## Proposed bounded falsifier

A future independently leased repair should add one focused progress gate such
as `native_pipeline_malformed_enum_progress_owner.sh`: valid enum native AST
must succeed; all five deletion-minimum malformed spellings must exit nonzero
within one second; timeout is a failure; and token, public-AST, and public-MIR
phase guards must all terminate. The repair owner must be the native
module-load/parser progress boundary, not HIR lowering. This proposal is
waiting work, not an implemented gate.
