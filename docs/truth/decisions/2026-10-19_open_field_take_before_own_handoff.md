# OPEN 2026-10-19: a field taken out before an `own` hand-off

Asked by: the Claude lane. Not decided; not worked on yet.

## The case

`SemanticAstIndexedStringBorrowStateRetire(inout state: ...)` in
`src/self_hosted/semantic/ast_collection_indexed_string_borrow_state_owner.pgy`
does:

```
let source_storage_ids: Array<Int> = state.source_storage_ids;
let invalidated: Array<Bool> = state.invalidated;
state.source_storage_ids = [];
state.invalidated = [];
SemanticAstIndexedStringBorrowSourceStorageRetire(source_storage_ids);   // own
SemanticAstIndexedStringBorrowInvalidationStorageRetire(invalidated);    // own
```

Each retire callee takes its Array `own` and calls
`CompilerRetireArrayStorage`, which frees the storage. The native checker
refuses both hand-offs (42:55 and 43:61: "An own Array argument cannot
transfer escaped array storage"): a local made from a field is an alias,
and nothing records that replacing the field leaves the local as the only
name of the old storage. A third refusal of the same owner, "An own Array
handoff requires preserved storage after prior calls", has no module in
the census (250:51).

## Why it matters

These three are the only refusals of
`collection_owned_element_requirements_finalize` on the canonical program.
That owner fails before the default-aggregate move, allocator lane and
named-call owners, so a normal compile never runs them; only the census
forces them. The canonical program must stay unchanged.

## What a rule must prove

After `let x = p.f; p.f = <fresh>;`, `x` names the old storage. It is the
only name only if nothing else shares that storage. A by-value struct is a
shallow copy, so a caller can hold another struct value whose field is the
same storage (`let b = a;` and then `a` passed `inout`). Retiring `x` then
frees storage `b` still reads.

| | Take rule, local proof | Take rule with exclusive owner | Keep refused |
|---|---|---|---|
| What admits the hand-off | the field replaced right after the take, nothing read in between | the same, and the caller's `inout` argument proved exclusive at every call | nothing |
| Soundness | unsound when a caller shares the storage | sound | sound |
| Work | small, in `array_storage_release_owner.c` | a cross-routine exclusivity fact for `inout` arguments | none, but the canonical stays refused |

No recommendation yet: the exclusive-owner fact needs a survey of how
`inout` struct arguments are made at the call sites first.
