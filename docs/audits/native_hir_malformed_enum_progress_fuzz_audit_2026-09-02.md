# Native HIR Malformed-Enum Progress Fuzz Audit

Status: READ-ONLY FINDING — REPRODUCED, NOT REPAIRED

Observed revision: `06207a293d9c1c313bc5734a7ee1ef49caa80422`

This audit records delegated fuzz evidence from `F:\tex_bug`. It is not a
semantic owner, SoT registry entry, progress increment, active implementation
lease, or completion claim.

## Unique finding

- The one-minimal reproducer is the exact six-byte input `enum({` with no
  trailing newline.
- `pgy SOURCE --native-pipeline --hir` exceeded both a 1-second timeout
  (observed wall time about 1.057s) and a 3-second timeout (about 3.249s).
  The result is a native HIR malformed-enum progress hang, not a crash receipt.
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
as `native_hir_malformed_enum_progress_owner.sh`: valid enum HIR must succeed;
the exact malformed fixture must exit nonzero within one second; timeout is a
failure; and token, AST, and public-MIR phase guards must all terminate. This
proposal is waiting work, not an implemented gate.
