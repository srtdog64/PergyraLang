# 2026-10-24: String receipt census after the rows check repair

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`f22d406d9` (local, not pushed). Census tool: the receipts pass forced to
run after every final owner (`instr_patch_receipts.py`), with each stored
call result's freshness proof failure logged (`instr_patch_rsrc.py`).

## Result

| Run | Final | Outside `direct_mir_*` | Landed copies | Wall |
|---|---|---|---|---|
| `1a75ca2ee` census, receipts pass not run (mg4) | 118 | 77 | 2321 | 2578 s |
| `1a75ca2ee`, receipts pass forced (receipts3) | not kept | | 3632 | |
| `f22d406d9`, receipts pass forced (receipts4) | 116 | 75 | 3382 | 2734 s |

Peak memory stayed at 3977376 KiB. In the final analysis of receipts4 the
receipts pass refuses one site ("An excluded String payload owner has no
current non-owning history proof"); every other shallow String store is
either proved or copied where it is stored, so the 905 refusals measured
on 2026-10-23 are no longer distance to admission. The receipts pass
lands about 1060 String copies and 74 stores across the landing rounds.

Before the rows check repair, 651 of the first analysis's freshness proof
failures came from one store failure inside the receipts pass
(`SemanticCanonicalTypeArgument`); with it, 11 remain of that kind.

## Why the copies stay (first analysis, receipts4)

| Failing premise of `string_result_source_at` | Count |
|---|---|
| the callee formal's summary is not current (JSON emitters first) | 707 |
| a returned formal is not a String (a field of a facts struct) | 350 |
| two arguments return storage of different sources | 50 |
| no current declared result frame (recursion) | 42 |

The first row traces to the no-store graph: a `TextBuilderAppend` into a
local builder is a mutable output there, because no fact says the builder
is the routine's own. The second row is real: such a result shares a
caller's storage, so a copy is the contract.

## Flow summary failures still in the census

`CollectionRuntimeNominalRecordArrayBlock` (first analysis) and
`JsonEmitFieldRaw` (third analysis) fail their required return relation
inside late demands ("required return backing relation is incomplete or
stale"), and the store closes for the rest of that analysis. The final
analysis has none, but the copies those analyses selected after the
failure are part of the 3382.
