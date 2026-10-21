# 2026-10-22: canonical census after loan copies leave own and ref arguments alone

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`15e0cc346` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-22_loan_copy_never_stands_at_own_or_ref_argument.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `15e0cc346` | 3463, 1094, 967, 961 | 670 |
| loan copies only at by-value arguments | 3463, 1064, 958, 956 | 667 |

Outside `direct_mir_*`: "Value boundary arguments ... must use a named
variable" 5 -> 0, moves 218 -> 217, `ref` escapes through a call 146 ->
149. The three escapes are the loans those copies hid: `admitted` and
`routines` handed to `ref` formals of `DriverRung2IntentTreeEmissionOrDie`
and `MirExpressionGraphFactsForArtifact`, whose loans cannot be copied
where they are made either. No other function changed.

All 709 flow fixtures give the same refusals before and after. The new
`by_value_getter_lend_into_own_formal_landed_valid` (a getter loan handed
to an `own` formal that keeps it) was refused before after its landing
analysis ("must use a named variable" at the `own` argument); now one
landing analysis copies the loan where the getter makes it, C and LLVM
print `1`, and C with ASan and UBSan runs it clean. Restoring the old
carrier test fails that fixture.

## Seen on the way, not changed

A getter loan handed to an `own` formal that only reads it
(`Take(own inner)` returning `ArrayLength(inner.items)`, then a use of the
lender) is admitted with no copy on this branch and before. It is harmless
while emitted code frees nothing; a cleanup synthesized for `own` formals
must copy or refuse that hand-off first, or it frees the lender's storage.
