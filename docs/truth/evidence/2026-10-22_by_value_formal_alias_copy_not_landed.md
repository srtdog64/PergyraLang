# 2026-10-22: copying a changed alias of a by-value formal, measured and not landed

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`c877a46e4` plus the change measured, which was then reverted.

## The cause it addressed

`SemanticExpressionGraphCallTargetCaptureFromSignatures(graph: ...)` takes
`graph.arena.call_target_kinds` and `..._names` into locals and sets their
elements. The locals share that storage with the caller's graph, so the
call moves the graph (summary READ | CALL). Clone copies the graph only by
value (it holds Arrays of structs), so no copy lands at the call. Through
`SemanticAstArtifactAnalyzeFromExpressionSurfaces`, whose input surfaces
are handed to it, this is also what fails most loans of
`SemanticAstExpressionSurfaceFactsFromTypedArtifact` (49 refused moves):
a loan that reaches a formal which changes its storage leaves.

## The change tried

A change through a local copy of a by-value formal's path selected the
copy where the routine takes it (`let kinds = Clone(graph.kinds)`), on the
reasoning that the copy's other side is the caller's value.

## Result

| Run | Final refusals | Outside `direct_mir_*` |
|---|---|---|
| `c877a46e4` | 904 | 615 |
| with the alias copy | 942 | 653 |

Moves fell by 6, but `Clone` of an `Array<String>` is a snapshot that owns
its Strings, and the routine then stores borrowed Strings into it:
"ArraySet cannot apply shallow String-element mutation to owned snapshot"
rose 3 -> 20, `ref` escapes through a call 149 -> 175, and eight flow
fixtures changed (the flow gate failed on
`default_aggregate_move_reuse_invalid`, now landing twice).

## What it needs instead

A copy that renews the Array's backing but keeps its elements borrowed
from the same owner, which no current operation provides (`Clone` and
`ArrayPushOwnedString` both make owned elements). Without it the formal's
move stays, and the 49 loan refusals stay with it.
