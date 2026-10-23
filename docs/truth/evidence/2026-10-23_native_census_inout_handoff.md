# 2026-10-23: canonical census after an `inout` formal handed to an `inout` formal escapes what the callee keeps

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`288072950` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_inout_formal_handed_to_inout_formal_keeps_what_the_callee_keeps.md).

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `288072950` | 209 | 167 |
| `inout` hand-off keeps what the callee keeps | 198 | 156 |

Removed: `Array argument call requires preserved array storage` 9 times
(the `EmitStmtList` recursion 8, `CodegenIntentPlanEmitTerminals` 1) and
`ArrayPushOwnedString requires preserved array storage` twice. Added:
none. The four `ParseDeclsInto` and `ParseDecls` refusals of the same kind
stay.

The census took 1036 s and 3978572 KiB at peak, against 996 s and
3978872 KiB on `288072950`. It ran beside the mutation builds below, not
back to back, and stays within the 927 to 1036 s spread of this session's
runs; the change only reads the callee's formal summary where a blanket
call escape was added before.

## Gates

`function-param-flow-summary` gate passed (193 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units passed, 0 failed (the target exits 2 at its
self-host driver step, as on main).

## Fixtures (783 compared against `288072950`)

Errors, landing analyses and `Clone` counts per routine changed only in
`inout_defers_handed_to_recursive_emitter_valid`, new: refused at 8:30 on
`288072950`, prints `dd` on C and LLVM now. Its two negatives are refused
the same on both trees: an alias of the defers changed by an `inout`
callee, and a callee that keeps the defers in another `inout` Array before
the nested call.

## Mutations (211 loan, getter, lend, payload, local-copy and `inout` fixtures)

| Mutation | Caught by |
|---|---|
| the hand-off keeps the blanket call escape | `inout_defers_handed_to_recursive_emitter_valid` refused |
| the returned-backing walk keeps an `inout` input's alias as an escape | the same |
| any formal of the routine handed whole to an `inout` formal | not caught |

The last stays as a guard: the rule's ground, that the caller lent its
place for change, holds only for an `inout` formal. A by-value Array formal
handed to an `inout` formal shares storage the write-back does not reach,
which this branch's storage walk still admits: `288072950` reads freed
storage (ASan) after such a callee grows the Array. That hole is fixed
separately.
