# 2026-10-22: canonical census after ArrayPop keeps nothing of its receiver

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`d531726e2` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-22_array_pop_keeps_nothing_of_its_receiver.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `d531726e2` | 3420, 454, 363, 361 | 319 |
| ArrayPop receiver used for the call only | 3420, 443, 352, 350 | 308 |

"Borrowed ref boundary value cannot escape through helper/function call"
63 -> 52 outside `direct_mir_*`; no refusal was added.

## How the cause was found

Two scratch censuses on the canonical input: one printed, for each `ref`
call refused as retaining or escaping, the callee formal's escape mask
(most were `READ|CALL` or `READ|RETURN|CALL`); one printed each frame that
gained a call escape and the call node it walked. In the `SelfMirLower*`
chain the escape entered at `SelfMirRoutineAtLocalCount(build, ...)`,
whose `build` formal had a call escape from `ArrayPop` on aliases of its
Arrays. An incomplete-frame census showed no incomplete frame in that
chain.

## Fixtures (733 compared against `d531726e2`)

New: `ref_loan_through_popping_helper_valid`, refused on `d531726e2` at
26:6 and 34:30 ("'input' cannot escape through helper/function call to
'Lower'"), admitted now; `ab` on C and LLVM, ASan clean. No other fixture
changed.

## Gates

`function-param-flow-summary` gate passed. `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the target exits 2
at its self-host driver step, as on main).

## Mutations

| Mutation | Caught by |
|---|---|
| ArrayPop's receiver escapes again | `ref_loan_through_popping_helper_valid` refused |
