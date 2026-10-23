# 2026-10-24: the rows check reads only established declared results

Decided by: the Claude lane, while taking the hidden String receipts
([decision](2026-10-24_hidden_string_receipts_proof_first.md)). The defect
is older than the main merge: the branch before it fails the same way.

## Rule

`function_param_flow_backing_rows_capture` checks a routine's declared
result frame only when it is established: built, complete, neither
building nor pending. `function_param_flow_declared_result_frame_current`
answers any other frame with NULL, and its callers use the callee's
summaries, so such a frame is no premise of any summary. A frame that is
building, pending or not built yet may become established later, so a
pass that skips one keeps no memo; a frame built incomplete and not
pending is never rebuilt.

## Why

With two mutually recursive String routines
(`SemanticCanonicalTypeArgument` and `SemanticCanonicalTypeName` on the
canonical input), building one's declared result opens a demand for the
other, and that demand checks its required return relation while the
first frame is still building, or after it was left pending with no
pending relation (declared results are no part of the pending relation
completion). The rows check refused such a frame, the relation stayed
unproved, and the flow summary store failed closed. On the canonical
input that happened inside the String receipts pass, where every later
stored call result then lost its freshness proof (651 of the receipt
proof failures in the census of the merged tree).

## Falsifier

`mutual_recursion_declared_result_stored_valid` stores both recursive
results and then a `Concat` result: one `Clone` lands per recursive result
and none for the `Concat` result. Without the rule the store fails closed
and the `Concat` result is copied too (three `Clone`s in `Main`).

## Not covered

The two recursive results are still copied: their freshness proof needs
the declared result frames to take part in the fixed point, which they
do not today.
