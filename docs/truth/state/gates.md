# Gates

Owner: GPT root lane. Observed 2026-10-21 KST on the native loop candidate
identified in [main](main.md). Full receipts and executable hashes:
[native loop cleanup](../evidence/2026-10-21_native_mir_loop_local_cleanup.md).

## Finish line (OPEN)

| Gate | Last result |
|---|---|
| Full unchanged canonical | RED before MIR; ordinary baseline/current input diagnostics match; no fresh downgraded census |
| Current bootstrap and gen2 == gen3 | not established on current main |
| Installed current C/LLVM driver | not established on current main |
| Required CI, all required jobs executed at one published SHA | see [main](main.md) |

The [Claude blocker](blocker.md) owns the last all-final-owner census.
Ordinary cascading errors are not that census or distinct-defect progress.

## Scoped regression evidence, not closure

Build copy: `/root/gpt-loop-local-20261021-A`, non-Git archive copy.

| Receipt | Observed result |
|---|---|
| Native loop gate | PASS C/LLVM; borrowed/owned payloads, return/continue/break, nested loops, explicit drops; escape/defer refuse |
| MIR / AIR units | 691/0 and 161/0 |
| MIR ASan+UBSan | 691/0, unit-suite leaks OFF |
| Emitted cleanup fixture | C ASan+UBSan; LLVM IR ASan + runtime ASan/UBSan; both leaks ON, stderr empty |
| Function-flow target | PASS ordinary and currency/worklist modes; phase13429/0, memo48/0, worklist31/0 |
| Speculation / header / documentation | PASS; documentation state/lane sizes checked at this update |

## Integration limits, not waived

| Gate | Last result |
|---|---|
| `make test-mir` | RED at match-binding installed-driver step; units, loop, topology and destructure gates passed |
| String-result control | RED in semantic analysis; same refusal also seen with existing Claude compiler |
| Self-host cleanup reader | source guard added; runtime probe blocked by imported borrowed-ref refusals |
| Declared non-retaining call | still refuses loop rebirth until semantic publication reaches MIR |
| Part-level independent review | not run; producer/consumer part is still incomplete |

[Next producer/consumer handoff](../../agent_work_directives/mir_loop_cleanup_array_preservation_projection_2026-10-21.md):
Claude publishes the existing preservation result; GPT carries/consumes it.
Task B has no route deletion or installed general-emitter evidence yet.
This native candidate is not self-host substitution or V1 closure.

Earlier V10 and backend matrices are
[historical evidence](../evidence/2026-10-18_v10_root_replay.md), not rerun here.
