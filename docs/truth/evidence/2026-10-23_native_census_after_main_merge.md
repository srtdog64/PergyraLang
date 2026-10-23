# 2026-10-23: canonical census after merging main into the native checker branch

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`048633974` (merge of main `12e2589d4` into `bf1164bf8`; local, not
pushed).

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `bf1164bf8` (branch) | 189 | 147 |
| `048633974` (merge) | 167 | 125 |

Removed with main's commits: 7 moves (`SelfMirProgramFactsBeforeCanonicalIdsObserved`
5 and its driver caller, `SelfMirAppendRoutine`), 6 local copies in
`SemanticAstCollectionExclusiveValueFactsFrom` (main's nominal field
copies keep each constructor argument at its own field), 8
`ArrayDropOwnedStrings cannot release escaped array storage` (main's
`inout` member release restoration), one `ref` escape and one `ArrayDrop`
row that reappears with the merged sources' syntax ids. Added: none. The
census took 1070 s and 3978176 KiB at peak.

## Gates on the merge

`function-param-flow-summary` gate passed (198 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 163, `test-rir` 44,
`test-hir` 58, `test-mir` units passed, 0 failed (the target exits 2 at its
self-host driver step, as on main).

`tests/aggregate_storage_transfer_smoke.sh` fails 12 checks on the merge
and the same 12 on main's own build of `12e2589d4`; the branch failed 14
before the merge (main's nominal field copies fix the sibling positive).
