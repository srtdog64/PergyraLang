# Gates

Owner: GPT root lane. Observed 2026-10-18 KST on an uncommitted post-gap V10
candidate atop `4a5372b0c8dfbf30ed6891b03c4bf6c7fb37a613`.

## Finish line (OPEN)

| Gate | Last result |
|---|---|
| Full unchanged fixed/current canonical | RED:18,435 errors/16 warnings/MIR0 on final V10; first refusal at `diagnostic_owner.pgy:573:39` |
| Current bootstrap and gen2 == gen3 | not established on current main |
| Installed current C/LLVM driver | not established on current main |
| Required CI, all required jobs executed at one published SHA | see [main](main.md) |

The earlier `b8e66a387` driver/fixed point is branch-specific
[history](../evidence/2026-10-16_b8e66a387_fixed_point.md), not current-main proof.

## Scoped regression evidence, not closure

Build copy: `/root/pgy-nonowner-20261018.5t1W96`.
Full receipts: [post-gap V10](../evidence/2026-10-18_v10_capture_allocation_bounds_post_gap.md),
[root replay](../evidence/2026-10-18_v10_root_replay.md).

| Receipt | Observed result |
|---|---|
| Candidate binaries | pgy `6b5b00ba...56d0`; AIR `d014758f...c2b1`; MIR `2f1d170f...b720` |
| Root final rerun | phase13422/0, MIR683/0, AIR161/0, RIR44/0, DIR15/0, semantic3230/0, transpile1014/0 |
| Four representative post-certificate mutations | instruction identity, observability flag, boundary count and row count all refuse and restore |
| Allocation lifecycle | missing storage, empty reimport, partial allocation rollback and capacity-tail release PASS; focused leaks ON |
| Current sanitizers | AIR161/0 and MIR683/0 under ASan+UBSan; unit-suite leaks OFF |
| Root strict/portable/sanitized | C/LLVM function-flow PASS; fresh five-unit replay and missing-input restoration PASS; separate ASan+UBSan unit gates PASS |
| Prior context, not rerun post-gap | 40-source sanitizer corpus and broad backend observations |

## Broader current-branch gates

| Gate | Last result |
|---|---|
| Default strict backend compare | RED: ABI precheck 186 passed/4 failed across two LLVM fixtures |
| Explicit all discovered backend cases | RED: 895/975 passed; default registry also omits three fixture directories |
| Production-header gate | size/header portions PASS; self-host parity manifest RED on opaque String boundary |
| Fresh portable post-gap V10 replay | PASS on 20 inputs with two missing-input refusals and restoration |
| Immutable V10 export | 270 source/context files and 135 evidence files frozen; [export receipt](../evidence/2026-10-18_v10_frozen_review_export.md) |
| Independent review | [APPROVE, V10 bounds only](../evidence/2026-10-18_v10_independent_review.md); no new reviewer execution |

The previous V9 size-mismatch sanitizer failures are repaired in this
candidate. The broader failures above are not attributed wholesale to V10.
