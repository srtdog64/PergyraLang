# 2026-10-21: canonical census after a handoff is judged by the calls before it

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`4f7692f38` plus the change measured (local, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** a call that hands an Array on is judged against the
  calls sequenced before it, not against its own handoff, and a callee that
  keeps the descriptor still needs owned elements. A defer's Array release is
  admitted again at each routine exit
  ([record](2026-10-21_defer_release_admitted_at_each_exit.md)); no canonical
  source releases an Array in a defer.

| Run | Refusals by analysis | Final, in `direct_mir_*` |
|---|---|---|
| `4f7692f38` | 3431, 1298, 1168, 1159 | 315 |
| handoffs judged by earlier calls | 3415, 1279, 1149, 1140 | 315 |

No flow store failed in any analysis. Outside `direct_mir_*` (844 before,
825 after) one kind changed:

| Kind | Before | After |
|---|---|---|
| an Array argument call requires preserved array storage | 85 | 66 |

Of the 19 removed, 14 are error returns `return
SemanticAstGenericSpecializationError(code, msg, call_node_ids, ...)` after
`SemanticAstGenericSpecializationIndexForCall(ref call_node_ids, ...)`: the
error call keeps the Arrays in its facts, and that was counted as a call
before itself. The other 5 are calls to
`SemanticExpressionGraphArenaFromTopologyWithIdentities` (2),
`MirExpressionIdentityEpochAppendDeclarationMethods`,
`MirExpressionGraphSequenceFromExtendedRows` and
`BuildMirMatchBindingLocalFacts`. None was added. Of the 66 left, the most
are calls to `EmitStmtList` (8), `SelfMirRoutineLocalInventoryError` (7),
`SelfMirDeclarationAppendMethodContract` (4),
`MirDomainTopologyOwnerGraphAddEdge` (4) and `AppendLine` (4).

A second finding, not caused by this change: once a routine is admitted,
MIR validation refuses an `Array<String>` declared with `[]` inside a
`while` body ("MIR collection ownership transition is invalid ...
stage=invalid-empty-birth"). The loop's back edge carries the previous
iteration's binding state, which no retire ends. `4f7692f38` refuses the
same program. The canonical source has this shape (`let actuals:
Array<String> = []` in `SemanticAstGenericSpecializationFacts...`).
Smallest program:

```
func Main() -> Int {
    let n: Int = 0;
    let total: Int = 0;
    while n < 3 {
        let actuals: Array<String> = [];
        ArrayPush(actuals, "x");
        total = total + ArrayLength(actuals);
        n = n + 1;
    }
    Print(ToString(total));
    return 0;
}
```

`Array<Int>` in the same place, a non-empty literal, and an `if` block pass.
