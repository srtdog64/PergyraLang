# 2026-10-24: canonical census after merging main and repairing two flow premises

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`b13178024` (local, not pushed): main merged at `fdc661ff9`
(`de2bc65c4`), the map alias case moved (`70d35f37d`), and the two flow
summary repairs ([snapshot](../decisions/2026-10-24_constructor_snapshot_element_proof_stays_with_its_consumer.md),
[member qualifier](../decisions/2026-10-24_declared_result_check_walks_member_qualifiers.md)).

## Canonical census

| Run | Final | Outside `direct_mir_*` | Analyses | Wall, peak |
|---|---|---|---|---|
| map deep Clone, before the merge (mc1) | 144 | 102 | 4 | 1092 s, 3979556 KiB |
| merged (mg2) | 35975 | 19958 | 6 | 686 s, 3698656 KiB |
| merged, snapshot repair (mg3) | 35977 | 19960 | 6 | 671 s, 3759880 KiB |
| merged, both repairs (mg4) | 118 | 77 | 10 | 2578 s, 3975500 KiB |

mg2 and mg3 failed the flow summary store closed: first at
`CodegenCallableReceiverFactsFromAdmittedRowsOrDie`, then, with the
snapshot repaired, at `SelfMirLowerLetFromArtifact`. The map branch of
`clone_copies_deeply` is not involved: a census of mg2 without it failed
the same way (37245).

Against mc1, gone: 11 String uses without a definition receipt, 8 Strings
borrowing a released Array element, 3 shallow String element writes to an
owned snapshot, 2 opaque String crossings, the 2 field-take hand-offs of
`SemanticAstIndexedStringBorrowStateRetire` (main's field take rule), 1
ArrayDrop obligation, 1 move (`base` into `ParserExpressionStructLiteral`)
and 2 moves inside `direct_mir_*`. Added: 2 calls passing overlapping
aggregate storage to a moving parameter (one inside `direct_mir_*`), 1
moving aggregate parameter whose String-array field path does not
preserve owned elements, 1 more inout argument that is not a variable
(`CanonicalMirIdentityEpochArtifactFromTreeText`).

## Cost

The census ran nine landing analyses instead of three, each a whole
analysis of the input, so its wall time grew 2.4 times while peak memory
stayed the same. Which landings chain is not measured yet.

## Not measured

The String receipts pass still runs only after every final owner admits
the program; this census does not count its refusals.
