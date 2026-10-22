# 2026-10-22: canonical census after an unwritten `inout` formal preserves the caller's storage

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`920092fb6` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-22_unwritten_inout_formal_preserves_storage.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `920092fb6` | 3421, 757, 652, 650 | 500 |
| `inout` walk only | 3421, 758, 653, 651 | 501 |
| `inout` walk and typed Map query results | 3404, 741, 636, 634 | 484 |

Outside `direct_mir_*`, final run: local copies whose other side changed
70 -> 55 (all 15 in `SemanticAstMapStringPayloadAtCallCompletion`), moves
155 -> 153 (`unproved_read_roots` in
`SemanticAstCollectionMemberMoveUsePublish`), opaque String crossings
85 -> 86. No kind was added.

## How the cause was found

The routine copies four maps of its `inout` loans into locals, hands
`loans` to `SemanticAstMapStringPayloadInvalidated`, declared `inout` but
only reading, then changes the locals and writes them back. The `inout`
walk alone admitted a probe with `MapHas` but not the canonical shape:
`let keys = MapKeys(marks)` made `keys` an alias of the map in the walk,
so `ArrayDropOwnedStrings(keys)` read as releasing it.

## The refusal added

`SemanticAstAnalysisResolveCallTargetsFromAdmittedBody` 99:52 stores
`target.name` into `analysis.expression_surfaces...call_target_names[node_id]`
by assignment. The census records it as a String element of the local
`names` (an `Array<String>` made inside the loop body) crossing an opaque
boundary with no call. It was not refused at `920092fb6`; the more
precise proofs of the callees' formals now name the String's owner. No
copy lands for an element assigned this way; only builtin stores
(`ArrayPush`, `ArraySet`) take one. Most of the 86 opaque crossings have
this shape or pass a String to a struct constructor.

## Fixtures (717 compared against `920092fb6`)

Only `local_copy_inout_read_only_callee_valid` (new) changed: refused
before, admitted now, prints `10` on C and LLVM. The new
`local_copy_inout_callee_changes_invalid` is refused at 18:12 and 19:19
on both trees.

## Gates and mutations

`function-param-flow-summary` gate passed (163 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the `test-mir`
target still exits 2 at its self-host driver step, as on main).

| Mutation | Caught by |
|---|---|
| no write rule for an `inout` formal | unit test: a field-assigning `inout` formal published preserved |
| every `inout` argument counts as a change again | `local_copy_inout_read_only_callee_valid` refused |
| `inout` path formals not walked | the same fixture refused; unit test |
| the frame's own input not excluded from the assignment copy | `inout_self_assignment_lands_nothing_valid` ran 1 landing |
| typed Map query result read as an alias | `local_copy_inout_read_only_callee_valid` refused |
