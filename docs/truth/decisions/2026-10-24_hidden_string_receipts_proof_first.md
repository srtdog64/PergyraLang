# 2026-10-24: the hidden String receipt refusals go to the Claude lane, proof first

Decided by: the user ("전부 작업할래? ... 장기적으로 더 유리한 방향으로 해")
on the [open question](2026-10-23_open_hidden_string_receipts_owner.md):
the Claude lane takes the String receipt refusals the final owners hide,
in the direction that pays off long term.

## Rule

A shallow String store of a call result is admitted by a proof first: the
callee's result source (`function_param_flow_string_result_source_prepare`)
shows fresh storage on every path (a `Concat`, a copied literal, a String
the callee built), and the store of such a result needs no copy. Since
main's `abedba79e` a store no proof admits lands a copy where it is stored
(`semantic_loan_copy_landing_select_string`); that copy stays only where
the callee's result really shares storage or its source is unknown.

## Why

About 900 stores on the canonical program reach the receipts pass. A copy
at each would allocate at every such store while emitted code frees
nothing yet. A freshness fact is produced once per callee and reused at
every store of its result, and it keeps the copy for a callee that does
return shared storage.

Replaces: the OPEN question of 2026-10-23. The 2026-10-18 String proof
lane given to GPT is narrowed by it to the String work GPT already holds
(the formal String store landing in its working tree on 2026-10-24).
