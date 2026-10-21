# 2026-10-22: gates for exporting a re-entered loop's last transfer

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`bdac13a32` plus the change measured (local, not pushed), on main
`7e7b63673`. Rule: [decision](../decisions/2026-10-22_reentered_loop_exports_last_transfer.md).

## Reproducer

```pergyra
func Accept(inout ids: Array<Int>, id: Int) -> Bool { ArrayPush(ids, id); return true; }
func Main() -> Void {
    var ids: Array<Int> = [];
    let node_id: Int = 0;
    while node_id < 3 {
        let actual_i: Int = 0;
        while actual_i < 2 { actual_i = actual_i + 1; }
        if !Accept(ids, node_id) { Log(0); }
        node_id = node_id + 1;
    }
    Log(ArrayLength(ids));
}
```

Before: semantic 0 errors, then `HIR validation failed: HIR routine 'Main'
loop-flow summaries share loop SyntaxNodeId`, on old main `4f7692f38` and
on `7e7b63673`. Without the inner loop, or without the call, it passed.
After: HIR and MIR pass. A probe copied from the canonical loop around
`SemanticAstGenericSpecializationAppend` now stops at the next wall, MIR
`invalid-empty-birth` for its loop-local `actuals`, which the declared-call
consumer of the [Array verdict](../decisions/2026-10-21_array_preservation_published_per_formal.md)
is to remove.

## Gates (WSL copy `/root/cc-defer2`)

| Gate | Result |
|---|---|
| `test-semantic` | 3237 passed, 0 failed (2 new) |
| `test-transpile` / `test-dir` / `test-air` / `test-rir` / `test-hir` | 1014 / 15 / 161 / 44 / 58 passed, 0 failed |
| `test-mir` units | passed; the target exits 2 at its self-host driver step, as on main |
| `function-param-flow-summary-test-smoke` | passed, with `inout_call_after_inner_loop_valid` paired valid and printing `3` on C and LLVM |
| `tests/loop_local_collection_cleanup_smoke.sh` | PASS |
| canonical census | 3463, 1184, 1059, 1050 by analysis; 744 outside `direct_mir_*`, no kind changed |

## Mutations

| Mutation | Caught by |
|---|---|
| export every recorded summary (the old behavior) | `test_loop_flow_reentry`: two rows |
| keep the first recorded summary instead of the last | `test_loop_flow_reentry`: the row's entry has `ids` unused |
| drop the publication call at the end of a function's check | both new cases |
| skip updating the latest summary in `record`'s duplicate branch | nothing: `try_apply` looks up the same loop, effect and entry first, so that branch is not reached |
| skip updating the latest summary on a memo hit | nothing: it changes the export only when a loop's entries return to an earlier one after a different one (A, B, A); no program found does that |

Both unfalsified lines keep the exported row equal to the summary the
loop's latest check used; they stay, with this record.

A nested `func` inside a body is not checked through the function hooks
(its loop is not traced), so the store is always empty when a function's
check begins; no publication is needed there.
