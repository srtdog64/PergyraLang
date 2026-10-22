# 2026-10-22: what a deep Clone of every aggregate would admit, measured and not taken

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`64e69eb92` with a scratch change to the checker only (no backend emits
it; reverted).

## The change measured

`semantic_param_publication_clone_independent` also called a `Clone`
deep for an Array whose elements are themselves copied deeply, and for a
`HashMap` with String or scalar keys and such values. Today both
backends copy those by value
([struct Clone is deep](../decisions/2026-10-19_struct_clone_is_deep.md)
covers structs of Strings and scalar or String Arrays only).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `64e69eb92` | 3404, 476, 385, 383 | 341 |
| deep Clone in the checker only | 3404, 389, 244, 242 | 204 |

Outside `direct_mir_*`: moves 102 -> 25, local copies whose other side
changed 55 -> 0, `ref` escapes through a call 63 -> 59.

## Why it is not taken

The types the widened rule would have copied, by how often it was asked
on the canonical input: `SemanticExpressionGraphView` (10208),
`SemanticExpressionGraphFacts` (4070), `SemanticAstExpressionSurfaceFacts`
(4031), `SemanticAstEnumFacts`, `SemanticAstNominalConstructorFacts`,
`SemanticAstArtifactAnalysis` (about 1200 each), `HashMap<String, Int>`
(806), then smaller fact structs. A copy landed for these would duplicate
a whole expression graph or analysis at each call that lends it, and
emitted code frees nothing yet, so the self-hosted compiler's memory
would grow with every such call. These loans need proofs (a loan that
stays, an owner that does not change), not copies.

The 55 local-copy refusals are of this kind: for example
`SemanticAstCollectionExclusiveValueFactsFrom` copies maps of its facts
(`let consumed = facts.consumed`), changes the facts through
`SemanticAstCollectionExclusiveRecordLocal`, and reads the copies again.
A `MapSet` that rehashes would leave the copy reading freed buckets, so
the refusal is the right verdict for the program as written.
