# 2026-10-22: canonical census after a store of held origins adds no call escape

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`0ff841851` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-22_store_of_held_origins_adds_no_call_escape.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `0ff841851` | 3388, 415, 324, 322 | 280 |
| held origins add no call escape | 3388, 392, 301, 299 | 257 |

Outside `direct_mir_*`: borrowed `ref` values escaping through a call
52 -> 32, and three single sites of the same message. No refusal was
added.

The same census run back to back on an otherwise idle machine took
966 s and 3977092 KiB at peak on `0ff841851`, and 955 s and 3977444 KiB
with the rule: no cost added.

## Gates

`function-param-flow-summary` gate passed. `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the target exits 2
at its self-host driver step, as on main).

## Fixtures (737 compared against `0ff841851`)

Only `ref_loan_field_written_back_valid` changed: refused on
`0ff841851` at 24:6, admitted now with one copy in `Plan`; `1` on C and
LLVM. New: `ref_loan_stored_into_inout_output_landed_valid` (a lent field
stored into an `inout` output that did not hold it: copied in `Fill` on
both trees) and `ref_loan_local_stored_into_inout_output_invalid` (a lent
local stored into such an output: refused at 22:6 and 29:10 on both
trees).

Probes, not fixtures: after `tmp = out`, a field assigned on `tmp` leaves
`out` unchanged at run time, and an Array pushed through `tmp.names` gets
a copy in `Fill`; both were refused on `0ff841851`.

## Mutations

| Mutation | Caught by |
|---|---|
| every origin counts as held (no call escape for any store into an output) | `ref_loan_local_stored_into_inout_output_invalid` admitted with no copy |
| no origin counts as held | `ref_loan_field_written_back_valid` refused at 24:6 |

A C build of the first mutation's admitted program under ASan runs
clean, because emitted code frees no Array it replaces: the run is no
witness either way, and the refusal is the checker's model.
