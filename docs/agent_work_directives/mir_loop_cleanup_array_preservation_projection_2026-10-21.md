# Loop-local cleanup: semantic publication to MIR (2026-10-21)

Status: HANDOFF REQUEST. The user chose **Claude publishes the evidence;
GPT connects the MIR consumers** on 2026-10-21. This is the remaining
producer/consumer seam of Task A in
[the active split](mir_loop_local_birth_and_direct_mir_split_2026-10-21.md),
not another ownership track or a semantic authority.

## Objective and boundary

Preserve the existing UNBORN/RETIRED birth rule and retire an iteration-local
Array only after its last non-retaining synchronous use. Existing owner:
function-parameter flow and its immutable semantic snapshot. Last consumers:
MIR cleanup admission, then native C/LLVM terminators. Forbidden: treating a
zero Slot mask as Array preservation, reopening a callee body from MIR,
matching declared functions by spelling, or adding source drops/copies.

## Claude: producer request

`PgyFunctionParamFlowFact` currently publishes function identity, formal
ordinal and `mask`. The same demanded owner already computes independent
descriptor/element preservation in
`function_param_flow_array_preservation_failures`. Publish that result in
the **same immutable fact and generation**, not a second query/store.

Proposed additive fields, for the consumer patch to agree on:

```c
bool has_array_preservation;
uint32_t array_preservation_failures;
```

- The flags use the existing `FUNCTION_PARAM_ARRAY_*_UNPROVED` meanings.
  An absent/unavailable/stale proof must never look like zero failures.
- Keep the identity, currentness, publication, teardown and consumer-facing
  snapshot validation of this row together. Failed/unsupported analysis
  cannot publish a success; do not demand new analysis from a snapshot read.
- Preservation alone is **not** non-retention. The existing flow mask and
  result-origin relations remain authoritative for escape and mutation.
- Please record the chosen field names and an exact producer commit before
  handoff. No `src/compiler/*` or `src/codegen/*` edits in this producer part.
  Neither OPEN move/copy decision is resolved by this request.

## GPT: consumer change set, after the producer is available

1. Carry the fields unchanged through HIR and MIR parameter-summary rows;
   validate presence, flags, function/ordinal identity and currentness.
2. Resolve a declared direct call by MIR declaration identity. Initially
   admit only a non-own/non-inout Array formal with a scalar/Void result,
   a read-only non-escaping flow mask, and current descriptor **and** element
   preservation. These conditions keep the caller's cleanup glue unchanged.
   Unknown calls, result views, retention, mutation and transfer stay closed.
3. Extend the existing loop cleanup use check, not a function-name route.
   Bind the carried fields into existing MIR/AIR publication and JSON facts.
   The legacy self-host reader must not silently drop new cleanup facts.
4. Keep one shared integration gate: the no-drop Count example below on
   C/LLVM, existing loop positive/escape/defer controls, missing/stale/cross-
   formal/preservation fault controls, `make test-mir`, String-result and
   function-flow targets, then the unchanged canonical census.

## Exact next falsifier

```pergyra
func Count(ref rows: Array<String>) -> Int { return ArrayLength(rows); }
func Main() -> Int {
    var n: Int = 0;
    var total: Int = 0;
    while n < 3 {
        var rows: Array<String> = [];
        ArrayPush(rows, "x");
        total = total + Count(rows);
        n = n + 1;
    }
    Print(ToString(total));
    return 0;
}
```

Before publication this still refuses at `invalid-empty-birth`, with no
MIR artifact. After the complete fact chain it must print `3` on both
backends without a source drop. A retaining or backing-changing callee must
not receive that cleanup permission. The real canonical consumer is
`SemanticAstGenericSpecializationAppend(..., actuals)`; do not special-case
it or infer preservation from its name.
