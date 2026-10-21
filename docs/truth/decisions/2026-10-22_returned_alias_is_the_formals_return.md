# 2026-10-22: an alias returned inside a result is the formal's own return

Decided by: the Claude lane. It applies
[path aliases follow the formal](2026-10-19_path_aliases_follow_the_formal.md)
to the order in which a formal's facts are computed. The user can overturn
it.

## Rule

A formal's storage walk reads the parameter summary after the backing
frame has settled it. When the frame proves that a local alias of the
formal (`let sealed = graph`) leaves in the returned value, the summary
carries RETURN, and the walk counts the returned alias as the formal's own
return: the call lends the formal, and the
[loan proof](2026-10-21_every_loan_of_a_path_is_checked.md) decides the
caller's later uses. Before, the walk ran first, saw a returned alias with
no RETURN in the summary, and marked the storage unproved for good (the
flags only grow), so every caller that used the argument again was
refused as a move.

## Why

On the canonical driver input 44 refused moves had a read-only summary
and unproved storage; the chain under
`SemanticAstArtifactAnalysisMatches` (27) ends in
`SemanticAstExpressionSurfaceFactsWithGraph`, which keeps its graph in a
local and returns it inside the facts. Such a callee lends, it does not
change what it receives.

Fixture: `returned_alias_lends_formal_valid` (refused as a move before,
admitted with no landing now, prints `22`).

Replaces: nothing.
