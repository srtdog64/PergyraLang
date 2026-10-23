# 2026-10-23: canonical census after a refused owned String push lands as a Clone

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`d769a84db` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_refused_owned_string_push_lands_as_clone.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `d769a84db` | 3388, 392, 301, 299 | 257 |
| refused owned push lands as a Clone | 3388, 362, 271, 269 | 227 |

Outside `direct_mir_*`: owned pushes into escaped Array storage 23 -> 0,
borrowed `ref` values escaping through a call 33 -> 29 (each refused
push had made its routine's summary fail closed), local copies whose
other side changed 28 -> 25. No refusal was added.

The same census run back to back took 1016 s and 3978096 KiB at peak on
`d769a84db`, and 1001 s and 3977236 KiB with the rule: no cost added.
Another lane's census ran beside the first part of the first run.

## Measured before building

Three counterfactual censuses (measurement only, unsound) bounded the
candidates on the same input: a call argument with no callee formal read
as a loan read (-10 moves, but 303 in all), no assignment failing a loan
proof (-9 moves), and this rule (-30). A version that kept the old
formal-only copy for receivers with borrowed elements removed 2: an Array
bound from a field also counts as of unknown element ownership.

## Gates

`function-param-flow-summary` gate passed. `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the target exits 2
at its self-host driver step, as on main).

## Fixtures (739 compared against `d769a84db`)

Two changed. New: `string_local_pushed_into_field_array_landed_valid`
(a local String a `ref` input lends, pushed into an Array taken from a
field, returned in rows or written back into the field: refused at 28:19
on `d769a84db`, one Clone in each of `Build` and `Plan` now; `yx` on C
and LLVM). `string_element_push_copy_landing_mixed_invalid` is now
`string_element_push_copy_landing_mixed_landed_valid`: its receiver holds
borrowed literals, so the owned push is still refused, and the String
lands as one Clone in `Selected`; `header023` on C and LLVM. Without the
uniform-ownership check the push would be an owned push with no Clone.

## Mutations

| Mutation | Caught by |
|---|---|
| only a formal's String path lands as a Clone (the old selection) | `string_local_pushed_into_field_array_landed_valid` refused |
| every origin counts as held ([earlier rule](2026-10-22_native_census_held_origin_store.md)) | `ref_loan_local_stored_into_inout_output_invalid` admitted |
